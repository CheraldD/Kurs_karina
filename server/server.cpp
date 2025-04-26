#include "server.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <thread>
#include <random>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
enum class LogLevel
{
    INFO,
    ERROR,
    FATAL,
    RECV,
    SESSION
};
static const char *levelNames[] = {"ИНФО", "ОШИБКА", "КРИТИЧЕСКАЯ", "ПОЛУЧЕНО", "СЕССИЯ"};
static const char *levelColors[] = {"\033[32m", "\033[31m", "\033[35m", "\033[36m", "\033[33m"};
static const char *RESET = "\033[0m";
void logMessage(LogLevel level, const std::string &method, const std::string &message)
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time, &tm_buf);
    char timeStr[20];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_buf);

    std::ostream &out = (level == LogLevel::ERROR || level == LogLevel::FATAL) ? std::cerr : std::cout;
    out << levelColors[static_cast<int>(level)]
        << timeStr << " | "
        << levelNames[static_cast<int>(level)] << " | "
        << method << "() -> "
        << message
        << RESET << std::endl;
}
int server::connection()
{
    const std::string method = "connect_to_cl";
    if (listen(serverSocket, 10) != 0)
        throw critical_error("Не удалось начать прослушивание");
    logMessage(LogLevel::INFO, method, "Сервер слушает входящие соединения");

    addr_size = sizeof(clientAddr);
    clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addr_size);
    if (clientSocket < 0)
        throw critical_error("Не удалось принять соединение");

    client_id = recieve(method + " | ошибка при получении ID клиента");
    if (client_id.empty())
        throw critical_error("ID клиента не получен");

    char cl_ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &clientAddr.sin_addr, cl_ip, sizeof(cl_ip)))
        throw critical_error("Не удалось получить IP клиента");
    client_ip = cl_ip;
    client_port = ntohs(clientAddr.sin_port);

    logMessage(LogLevel::INFO, method, "Новый клиент: IP=" + client_ip + ", порт=" + std::to_string(client_port) + ", ID=" + client_id);
    return 0;
}

std::string server::recieve(const std::string error_message)
{
    char buffer[256];
    int len = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0)
    {
        logMessage(LogLevel::ERROR, "recv_data", error_message);
        return "";
    }
    buffer[len] = '\0';
    return std::string(buffer);
}

server::server(uint port, uint s, uint b) : p(port), seed(s), buff_size(b) {}

void server::run()
{
    const std::string method = "work";
    logMessage(LogLevel::INFO, method, "Сервер запущен и ожидает клиентов");
    try
    {
        steady();
    }
    catch (const critical_error &e)
    {
        logMessage(LogLevel::FATAL, method, e.what());
        return;
    }

    while (true)
    {
        try
        {
            if (connection() == 0)
            {
                logMessage(LogLevel::INFO, method, "Клиент подключился, запуск потоков");
                overflowed.store(false);
                output_done = false;
                {
                    std::lock_guard<std::mutex> lg(buffer_mutex);
                    while (!buffer_byte.empty())
                        buffer_byte.pop();
                }
                std::thread t1(&server::work_with_client, this), t2(&server::output_thread, this);
                t1.join();
                t2.join();
                logMessage(LogLevel::INFO, method, "Сессия клиента завершена");
            }
        }
        catch (const critical_error &e)
        {
            logMessage(LogLevel::ERROR, method, std::string(e.what()) + ". Продолжаем ожидание...");
        }
    }
}

void server::work_with_client()
{
    const std::string method = "handle_client";
    interval = 50;
    session_start = std::chrono::steady_clock::now();
    update_interval(interval);
    while (true)
    {
        uint32_t netd;
        int bytes = recv(clientSocket, &netd, sizeof(netd), 0);
        if (bytes <= 0)
            return;
        uint32_t data = ntohl(netd);
        std::lock_guard<std::mutex> lg(buffer_mutex);
        if (overflowed.load() || buffer_byte.size() >= buff_size)
        {
            overflowed.store(true);
            interval *= 2;
            update_interval(interval);
            continue;
        }
        buffer_byte.push(data);
        buffer_cv.notify_one();
    }
}

void server::output_thread()
{
    const std::string method = "output_thread";
    std::unique_lock<std::mutex> lk(buffer_mutex);
    while (true)
    {
        buffer_cv.wait(lk, [this]
                       { return !buffer_byte.empty() || (overflowed.load() && buffer_byte.empty()); });
        if (!buffer_byte.empty())
        {
            uint32_t d = buffer_byte.front();
            buffer_byte.pop();
            lk.unlock();
            random_delay(seed);
            std::ostringstream oss;
            oss << "Получено значение: 0x" << std::hex << std::setw(8) << std::setfill('0') << d
                << " (" << std::dec << d << ")";
            logMessage(LogLevel::RECV, method, oss.str());
            lk.lock();
        }
        if (overflowed.load() && buffer_byte.empty())
        {
            auto dur = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - session_start)
                           .count();
            logMessage(LogLevel::INFO, method, "Буфер опустошён, поток завершается");
            logMessage(LogLevel::SESSION, "session", "Продолжительность: " + std::to_string(dur) + " с");
            lk.unlock();
            close_socket();
            return;
        }
    }
}

void server::random_delay(int seed)
{
    static std::mt19937 gen(seed);
    std::uniform_int_distribution<> dist(min_delay_ms, max_delay_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
}

void server::steady()
{
    const std::string method = "start";
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
        throw critical_error("Не удалось создать сокет");
    logMessage(LogLevel::INFO, method, "Сокет создан");
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(p);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        throw critical_error("Не удалось привязать сокет");
    logMessage(LogLevel::INFO, method, "Сокет привязан к порту " + std::to_string(p));
}

void server::update_interval(uint32_t interval)
{
    const std::string method = "send_interval";
    uint32_t n = htonl(interval);
    if (send(clientSocket, &n, sizeof(n), MSG_NOSIGNAL) == -1)
    {
        logMessage(LogLevel::ERROR, method, std::string("Не удалось отправить интервал: ") + strerror(errno));
        return;
    }
    std::ostringstream oss;
    oss << "Интервал отправлен клиенту: " << interval << " мс";
    logMessage(LogLevel::INFO, method, oss.str());
}

void server::close_socket()
{
    const std::string method = "close_sock";
    logMessage(LogLevel::INFO, method, "Соединение с клиентом (ID=" + client_id + ") закрыто");
    close(clientSocket);
    interval = 50;
}
