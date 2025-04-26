#include "client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <csignal>
#include <arpa/inet.h>


enum class LogLevel
{
    INFO,
    ERROR,
    SEND,
    RECV
};
static const char *levelNames[] = {"ИНФО", "ОШИБКА", "ОТПРАВЛЕНО", "ПОЛУЧЕНО"};
static const char *levelColors[] = {"\033[32m", "\033[31m", "\033[36m", "\033[33m"};
static const char *RESET = "\033[0m";

// Утилита логирования: формат "YYYY-MM-DD HH:MM:SS | УРОВЕНЬ | method() -> сообщение"
void logMessage(LogLevel level, const std::string &method, const std::string &message)
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time, &tm_buf);
    char timeStr[20];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_buf);

    std::ostream &out = (level == LogLevel::ERROR) ? std::cerr : std::cout;
    out << levelColors[static_cast<int>(level)]
        << timeStr << " | "
        << levelNames[static_cast<int>(level)] << " | "
        << method << "() -> "
        << message
        << RESET << std::endl;
}

uint32_t client::generate_random_uint32()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dist(
        0, std::numeric_limits<uint32_t>::max());
    return dist(gen);
}

void client::run(UI &intf)
{
    const std::string method = "client::work";

    std::string sip = intf.get_serv_ip();
    if (sip.empty())
        throw client_error("Не удалось получить IP сервера");
    serv_ip = sip.c_str();

    port = intf.get_port();
    if (port == 1)
        throw client_error("Недопустимый порт");

    id = intf.get_username();
    if (id.empty())
        throw client_error("Не указано имя пользователя");

    // logMessage(LogLevel::INFO, method, "Запуск клиента: IP=" + serv_ip + ", порт=" + std::to_string(port) + ", пользователь=" + id);

    steady();
    connection();

    std::chrono::milliseconds tick(5);
    while (true)
    {
        uint32_t net_int;
        int n = recv(sock, &net_int, sizeof(net_int), MSG_DONTWAIT);
        if (n == 0)
            break;
        if (n > 0)
        {
            interval = ntohl(net_int);
            logMessage(LogLevel::INFO, method, "Новый интервал: " + std::to_string(interval) + " мс");
        }

        std::this_thread::sleep_for(tick);
        uint32_t rnd = htonl(generate_random_uint32());
        if (send(sock, &rnd, sizeof(rnd), 0) < 0)
        {
            throw client_error("Ошибка отправки рандомного числа");
        }
        std::ostringstream oss;
        oss << "0x" << std::hex << rnd;
        logMessage(LogLevel::SEND, method, "Отправлено число: " + oss.str());

        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }

    close_socket();
    logMessage(LogLevel::INFO, method, "Клиент завершил работу");
}

void client::steady()
{
    const std::string method = "client::start";
    std::signal(SIGPIPE, SIG_IGN);

    logMessage(LogLevel::INFO, method, "Создание сокета");
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        throw client_error("Не удалось создать сокет");
    logMessage(LogLevel::INFO, method, "Сокет создан");

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serv_ip, &serverAddr.sin_addr) <= 0)
        throw client_error("Ошибка преобразования IP");
    logMessage(LogLevel::INFO, method, std::string("Адрес сервера: ") + serv_ip + ":" + std::to_string(port));
}

void client::connection()
{
    const std::string method = "client::connect_to_server";
    logMessage(LogLevel::INFO, method, "Подключение к серверу...");

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        close_socket();
        throw client_error("Ошибка подключения к серверу");
    }
    logMessage(LogLevel::INFO, method, "Подключение установлено");
    logMessage(LogLevel::SEND, method, "Отправка ID: " + id);
    transfer_data(id);
}

std::string client::recieve()
{
    const std::string method = "client::recv_data";
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    logMessage(LogLevel::INFO, method, "Ожидание сообщения от сервера...");
    int rc;
    while (true)
    {
        buffer = std::make_unique<char[]>(buflen);
        rc = recv(sock, buffer.get(), buflen, MSG_PEEK);

        if (rc == 0)
        {
            close_socket();
            throw client_error("Соединение закрыто сервером");
        }
        if (rc < 0)
        {
            close_socket();
            throw client_error("Ошибка recv()");
        }
        if (static_cast<size_t>(rc) < buflen)
            break;

        buflen *= 2;
        logMessage(LogLevel::INFO, method, "Буфер увеличен до " + std::to_string(buflen) + " байт");
    }

    std::string msg(buffer.get(), rc);
    if (recv(sock, nullptr, rc, MSG_TRUNC) < 0)
    {
        close_socket();
        throw client_error("Ошибка MSG_TRUNC");
    }

    logMessage(LogLevel::RECV, method, "Получено сообщение: '" + msg + "' (" + std::to_string(msg.size()) + " байт)");
    return msg;
}

void client::close_socket()
{
    const std::string method = "client::close_sock";
    logMessage(LogLevel::INFO, method, "Закрытие сокета...");
    if (close(sock) == 0)
        logMessage(LogLevel::INFO, method, "Сокет закрыт");
    else
        logMessage(LogLevel::ERROR, method, "Не удалось закрыть сокет");
}

void client::transfer_data(std::string data)
{
    const std::string method = "client::send_data";
    logMessage(LogLevel::INFO, method, "Отправка данных: '" + data + "'");
    if (send(sock, data.c_str(), data.size(), 0) < 0)
    {
        close_socket();
        throw client_error("Ошибка send()");
    }
    logMessage(LogLevel::SEND, method, "Данные успешно отправлены");
}
