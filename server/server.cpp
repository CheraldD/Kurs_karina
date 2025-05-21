#pragma once
#include "server.h"
#include "application_protocol.h"
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
#include <errno.h>

// Уровни логирования для цветного вывода
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

// Форматированный вывод сообщения
void logMessage(LogLevel level, const std::string &method, const std::string &message)
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time, &tm_buf);
    char timeStr[20];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_buf);

    std::ostream &out = (level == LogLevel::ERROR || level == LogLevel::FATAL) ? std::cerr : std::cout;
    out << levelColors[(int)level] << timeStr
        << " | " << levelNames[(int)level]
        << " | " << method << "() -> " << message
        << RESET << std::endl;
}

// Отправка протокольного сообщения клиенту
void server::transfer_data(const std::string &header, const std::string &data)
{
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    const std::string method = "transfer_data";
    ApplicationProtocol msg(header, client_id, ApplicationProtocol::generateID(), data);
    std::string packet = msg.pack();
    ssize_t sent = send(clientSocket, packet.c_str(), packet.size(), 0);
    if (sent <= 0)
    {
        logMessage(LogLevel::ERROR, method, "Ошибка send: " + std::string(strerror(errno)));
        // close_socket();
    }
    else
    {
        logMessage(LogLevel::INFO, method, "Отправлено: " + packet);
    }
}
server::server(uint port, int s, uint b) : p(port), seed(s), buff_size(b) {}
// Чтение протокольного сообщения от клиента
std::string server::recieve(std::string context)
{
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    const std::string method = "recieve";

    char tmp[4096];
    int len = recv(clientSocket, tmp, sizeof(tmp) - 1, 0);
    if (len == 0)
    {
        logMessage(LogLevel::INFO, method, "Клиент закрыл соединение");
        return "";
    }
    if (len < 0)
    {
        logMessage(LogLevel::ERROR, method, "Ошибка приема; код ошибки " + std::to_string(errno));
        close_socket();
        return "";
    }

    tmp[len] = '\0';
    recv_buffer += tmp; // теперь это поле класса
    logMessage(LogLevel::RECV, method, tmp);

    auto pos = recv_buffer.find('\n');
    if (pos == std::string::npos)
    {
        return "";
    }

    std::string line = recv_buffer.substr(0, pos);
    recv_buffer.erase(0, pos + 1);
    auto proto = ApplicationProtocol::unpack(line);
    return proto.data();
}

// Ожидание входящего соединения
int server::connection()
{
    const std::string method = "connection";
    recv_buffer.clear();
    logMessage(LogLevel::INFO, method, "Ожидание нового клиента...");
    addr_size = sizeof(clientAddr);
    while (true)
    {
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addr_size);
        if (clientSocket < 0)
        {
            if (errno == EINTR)
            {
                // системный вызов был прерван — просто повторим accept()
                continue;
            }
            throw server_error("Не удалось принять соединение: " + std::string(strerror(errno)));
        }
        break;
    }

    // инициализация сессии
    std::string id = recieve(method + " | ошибка при получении ID клиента");
    if (id.empty())
        throw server_error("ID клиента не получен");
    client_id = id;

    char cl_ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &clientAddr.sin_addr, cl_ip, sizeof(cl_ip)))
        throw server_error("Не удалось получить IP клиента");
    client_ip = cl_ip;
    client_port = ntohs(clientAddr.sin_port);

    logMessage(LogLevel::INFO, method,
               "Новый клиент: IP=" + client_ip +
                   ", порт=" + std::to_string(client_port) +
                   ", ID=" + client_id);
    return 0;
}

// Отправка клиенту текущего интервала между приёмами
void server::update_interval(uint32_t inter)
{
    transfer_data("UPDATE_INTERVAL", std::to_string(inter));
}
void server::update_interval(std::string hollow)
{
    transfer_data("UPDATE_INTERVAL", hollow);
}
// Поток чтения данных от клиента: буферизация и управление переполнением
void server::work_with_client()
{
    const std::string method = "work_with_client";
    interval = 250;
    session_start = std::chrono::steady_clock::now();
    update_interval(interval); // отправить начальный интервал клиенту

    while (true)
    {
        std::string raw = recieve(method + " | ошибка при получении данных");

        if (raw.empty())
        {
            logMessage(LogLevel::INFO, method, "Клиент возможно закрыт, выход из потока");
            receiving_done.store(true);
            buffer_cv.notify_all(); // важно: разбудить output_thread
            return;
        }

        update_interval("OK");

        uint32_t data = static_cast<uint32_t>(std::stoul(raw));
        bool drop = false;

        {
            std::lock_guard<std::mutex> lg(buffer_mutex);

            if (overflowed.load() || buffer_byte.size() >= buff_size)
            {
                if (!overflowed.load())
                {
                    logMessage(LogLevel::INFO, method, "Буфер переполнен — включён режим сброса входящих данных");
                }

                overflowed.store(true);
                interval += 1000;
                drop = true;
            }

            if (!drop)
            {
                buffer_byte.push(data);
                buffer_cv.notify_one(); // разбудить поток вывода
            }
        }

        if (drop)
        {
            update_interval(interval);
        }
    }

    // Сигнализируем, что данных больше не будет

    // Ждём завершения вывода
    while (!output_done.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return;
    // close_socket();
}

// Поток вывода: забирает данные из буфера и логирует их
void server::output_thread()
{
    const std::string method = "output_thread";
    std::unique_lock<std::mutex> lk(buffer_mutex);

    while (true)
    {
        buffer_cv.wait(lk, [this]
                       { return !buffer_byte.empty() ||
                                (overflowed.load() && buffer_byte.empty()) ||
                                (receiving_done.load() && buffer_byte.empty()); });

        if (!buffer_byte.empty())
        {
            uint32_t d = buffer_byte.front();
            buffer_byte.pop();
            lk.unlock();

            random_delay(seed);

            std::ostringstream oss;
            oss << "Получено значение: 0x" << std::hex << std::setw(8)
                << std::setfill('0') << d << " (" << std::dec << d << ")";
            logMessage(LogLevel::RECV, method, oss.str());

            lk.lock();
        }

        if ((overflowed.load() && buffer_byte.empty()) ||
            (receiving_done.load() && buffer_byte.empty()))
        {
            auto dur = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - session_start)
                           .count();

            logMessage(LogLevel::INFO, method, "Буфер опустошён, поток завершается");
            logMessage(LogLevel::SESSION, "session", "Продолжительность: " + std::to_string(dur) + " с");

            output_done.store(true);
            buffer_cv.notify_all(); // на случай, если кто-то ждёт
            close_socket();
            return;
        }
    }
}

// Генерирует случайную задержку
void server::random_delay(int seed)
{
    static std::mt19937 gen(seed);
    std::uniform_int_distribution<> dist(min_delay_ms, max_delay_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
}

// Создание серверного сокета и привязка
void server::steady()
{
    const std::string method = "steady";
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
        throw server_error("Не удалось создать сокет");
    logMessage(LogLevel::INFO, method, "Сокет создан");

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(p);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        throw server_error("Не удалось привязать сокет");
    logMessage(LogLevel::INFO, method, "Сокет привязан к порту " + std::to_string(p));

    // Слушаем входящие соединения ровно один раз
    if (listen(serverSocket, 10) != 0)
        throw server_error("Не удалось начать прослушивание");
    logMessage(LogLevel::INFO, method, "Сервер слушает входящие соединения");
}

// Закрытие клиентского сокета и сброс флагов
void server::close_socket()
{
    const std::string method = "close_socket";

    // Правильное завершение чтения и записи
    shutdown(clientSocket, SHUT_RDWR);
    logMessage(LogLevel::INFO, method, "Соединение с клиентом (ID=" + client_id + ") закрыто");

    // Закрываем сокет
    close(clientSocket);

    // Сброс всех флагов к исходным значениям
    overflowed.store(false);
    receiving_done.store(false);
    output_done.store(false);

    // Интервал по умолчанию
    interval = 50;
}

void server::run()
{
    const std::string method = "work";
    logMessage(LogLevel::INFO, method, "Сервер запущен и ожидает клиентов");
    try
    {
        steady();
    }
    catch (const server_error &e)
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
        catch (const server_error &e)
        {
            logMessage(LogLevel::ERROR, method, std::string(e.what()) + ". Продолжаем ожидание...");
        }
    }
}