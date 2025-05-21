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
enum class LogLevel { INFO, ERROR, FATAL, RECV, SESSION };
static const char *levelNames[] = {"ИНФО", "ОШИБКА", "КРИТИЧЕСКАЯ", "ПОЛУЧЕНО", "СЕССИЯ"};
static const char *levelColors[] = {"\033[32m", "\033[31m", "\033[35m", "\033[36m", "\033[33m"};
static const char *RESET = "\033[0m";

// Форматированный вывод сообщения
void logMessage(LogLevel level, const std::string &method, const std::string &message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf; localtime_r(&time, &tm_buf);
    char timeStr[20]; std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_buf);

    std::ostream &out = (level==LogLevel::ERROR||level==LogLevel::FATAL)? std::cerr: std::cout;
    out << levelColors[(int)level] << timeStr
        << " | " << levelNames[(int)level]
        << " | " << method << "() -> " << message
        << RESET << std::endl;
}

// Отправка протокольного сообщения клиенту
void server::transfer_data(const std::string &header, const std::string &data) {
    const std::string method = "transfer_data";
    ApplicationProtocol msg(header, client_id, ApplicationProtocol::generateID(), data);
    std::string packet = msg.pack();
    ssize_t sent = send(clientSocket, packet.c_str(), packet.size(), MSG_NOSIGNAL);
    if (sent <=0) {
        logMessage(LogLevel::ERROR, method, "Ошибка send: " + std::string(strerror(errno)));
    } else {
        logMessage(LogLevel::INFO, method, "Отправлено: " + packet);
    }
}
server::server(uint port, int s, uint b) : p(port), seed(s), buff_size(b) {}
// Чтение протокольного сообщения от клиента
std::string server::recieve(std::string messg) {
    static std::string buffer;    // накапливаем “хвосты”
    const std::string method = "recieve";
    char tmp[4096];
    int len = recv(clientSocket, tmp, sizeof(tmp)-1, 0);
    if (len <= 0) {
        logMessage(LogLevel::ERROR, method, "Ошибка приема; код ошибки " + std::to_string(errno));
        return "";
    }
    tmp[len] = '\0';
    buffer += tmp;              // добавили во временный буфер
    logMessage(LogLevel::RECV, method, std::string(tmp));

    // ищем первую завершённую строку
    auto pos = buffer.find('\n');
    if (pos == std::string::npos) {
        // ещё нет полного сообщения — ждём дальше
        return "";  
    }

    // вычленяем одну строку + обрезаем из буфера
    std::string line = buffer.substr(0, pos);
    buffer.erase(0, pos + 1);

    auto proto = ApplicationProtocol::unpack(line);
    return proto.data();
}

// Ожидание входящего соединения
int server::connection() {
    const std::string method = "connection";
    if (listen(serverSocket, 10) != 0) throw server_error("Не удалось начать прослушивание");
    logMessage(LogLevel::INFO, method, "Сервер слушает входящие соединения");

    addr_size = sizeof(clientAddr);
    clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addr_size);
    if (clientSocket < 0) throw server_error("Не удалось принять соединение");

    // Инициализация сессии: получаем client ID
    std::string id = recieve(method + " | ошибка при получении ID клиента");
    if (id.empty()) throw server_error("ID клиента не получен");
    client_id = id;

    char cl_ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &clientAddr.sin_addr, cl_ip, sizeof(cl_ip)))
        throw server_error("Не удалось получить IP клиента");
    client_ip = cl_ip;
    client_port = ntohs(clientAddr.sin_port);

    logMessage(LogLevel::INFO, method, "Новый клиент: IP=" + client_ip + ", порт=" + std::to_string(client_port) + ", ID=" + client_id);
    return 0;
}

// Отправка клиенту текущего интервала между приёмами
void server::update_interval(uint32_t inter) {
    transfer_data("UPDATE_INTERVAL", std::to_string(inter));
}
void server::update_interval(std::string hollow) {
    transfer_data("UPDATE_INTERVAL", hollow);
}
// Поток чтения данных от клиента: буферизация и управление переполнением
void server::work_with_client() {
    const std::string method = "work_with_client";
    interval = 250;
    session_start = std::chrono::steady_clock::now();
    update_interval(interval);

    while (true) {
        std::string raw = recieve(method + " | ошибка при получении данных");
        if (raw.empty()) break;
        update_interval("OK");
        uint32_t data = static_cast<uint32_t>(std::stoul(raw));

        {
            std::lock_guard<std::mutex> lg(buffer_mutex);
            if (overflowed.load() || buffer_byte.size() >= buff_size) {
                overflowed.store(true);
                interval *= 2;
                update_interval(interval);
                continue;
            }
            buffer_byte.push(data);
        }
        buffer_cv.notify_one();
    }

    close_socket();
    overflowed.store(true);
    buffer_cv.notify_all();
    return;
}

// Поток вывода: забирает данные из буфера и логирует их
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

// Генерирует случайную задержку
void server::random_delay(int seed) {
    static std::mt19937 gen(seed);
    std::uniform_int_distribution<> dist(min_delay_ms, max_delay_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
}

// Создание серверного сокета и привязка
void server::steady() {
    const std::string method = "steady";
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) throw server_error("Не удалось создать сокет");
    logMessage(LogLevel::INFO, method, "Сокет создан");

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(p);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
        throw server_error("Не удалось привязать сокет");
    logMessage(LogLevel::INFO, method, "Сокет привязан к порту " + std::to_string(p));
}

// Закрытие клиентского сокета
void server::close_socket() {
    const std::string method = "close_socket";
    update_interval("FIN");
    logMessage(LogLevel::INFO, method, "Соединение с клиентом (ID=" + client_id + ") закрыто");
    close(clientSocket);
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