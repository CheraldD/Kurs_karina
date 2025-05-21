#include "client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <csignal>
#include <arpa/inet.h>
#include "application_protocol.h"
// Лог-уровни и цвета для цветного вывода сообщений
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

// Утилита логирования: форматирует время, выбирает цвет, выводит сообщение
void logMessage(LogLevel level, const std::string &method, const std::string &message)
{
    auto now = std::chrono::system_clock::now();        // текущий момент
    auto t = std::chrono::system_clock::to_time_t(now); // в time_t
    std::tm tm_buf;
    localtime_r(&t, &tm_buf); // в локальную структуру
    char timeStr[20];         // буфер для строки времени
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_buf);

    // Выбираем поток и цвет по уровню
    std::ostream &out = (level == LogLevel::ERROR ? std::cerr : std::cout);
    out << levelColors[static_cast<int>(level)] // префикс цвета
        << timeStr << " | " << levelNames[static_cast<int>(level)]
        << " | " << method << "() -> " << message
        << RESET << std::endl; // сброс цвета
}

// Генерация случайного 32-битного числа для отправки
uint32_t client::generate_random_uint32()
{
    static std::mt19937 gen{std::random_device{}()};     // инициализация RNG
    static std::uniform_int_distribution<uint32_t> dist( // полный диапазон uint32_t
        0, std::numeric_limits<uint32_t>::max());
    return dist(gen); // возвращаем случайное значение
}
void client::connection()
{
    const std::string method = "client::connect_to_server";
    logMessage(LogLevel::INFO, method, "Подключение к серверу...");

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        close_socket();
        throw speciphic_error("Ошибка подключения к серверу");
    }
    logMessage(LogLevel::INFO, method, "Подключение установлено");
    logMessage(LogLevel::SEND, method, "Отправка ID: " + id);
    transfer_data("ID_SEND", id);
}
// Основной цикл клиента: настройка, подключение, обмен данными
void client::run(UI &intf)
{
    const std::string method = "client::run";

    std::string ip = intf.get_serv_ip();
    serv_ip = ip.c_str();
    port = intf.get_port();
    id = intf.get_username();

    steady();
    connection();
    std::string rcv = recieve();
    interval = stoul(rcv);

    std::chrono::milliseconds tick(5);
    while (true) // критерий выхода добавить при необходимости
    {

        // Генерация и отправка числа
        uint32_t rnd = generate_random_uint32();
        transfer_data("SEND_DATA", std::to_string(rnd));

        std::ostringstream oss;
        oss << "0x" << std::hex << rnd;
        logMessage(LogLevel::SEND, method, "Отправлено число: " + oss.str());

        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        logMessage(LogLevel::INFO, method, "Текущий интервал: " + std::to_string(interval) + " мс");
        std::this_thread::sleep_for(tick);

        // Пытаемся получить новое значение интервала (в пределах 100 мс)
        std::string response = recieve(); // recieve(false): не логировать ошибку при таймауте
        if (!response.empty() && response != "OK")
        {
            try {
                interval = std::stoul(response);
                logMessage(LogLevel::INFO, method, "Интервал обновлён: " + std::to_string(interval) + " мс");
            } catch (...) {
                logMessage(LogLevel::ERROR, method, "Некорректный ответ сервера: " + response);
            }
        }
    }

    // Завершение (сюда можно вставить проверку условия остановки)
    close_socket();
    logMessage(LogLevel::INFO, method, "Клиент завершил работу");
}


// Создание TCP-сокета и настройка структуры адреса сервера
void client::steady()
{
    const std::string method = "client::steady";
    std::signal(SIGPIPE, SIG_IGN); // игнорируем SIGPIPE, чтобы не падать при записи в закрытый сокет

    logMessage(LogLevel::INFO, method, "Создаём сокет");
    sock = socket(AF_INET, SOCK_STREAM, 0); // создаём TCP-сокет
    if (sock < 0)
        throw speciphic_error("Не удалось создать сокет");
    logMessage(LogLevel::INFO, method, "Сокет создан");

    serverAddr.sin_family = AF_INET;   // IPv4
    serverAddr.sin_port = htons(port); // порт в сетевом порядке
    if (inet_pton(AF_INET, serv_ip, &serverAddr.sin_addr) <= 0)
        throw speciphic_error("Неверный IP сервера");
    // Логируем конечный адрес
    logMessage(LogLevel::INFO, method, std::string("Сервер: ") + serv_ip + ":" + std::to_string(port));
}

void client::transfer_data(const std::string &header, const std::string &data)
{
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    const std::string method = "transfer_data";
    ApplicationProtocol msg(header, id, ApplicationProtocol::generateID(), data);
    std::string packet = msg.pack();
    ssize_t sent = send(sock, packet.c_str(), packet.size(), 0);
    if (sent <=0)
    {
        logMessage(LogLevel::ERROR, method, "Ошибка send: " + std::string(strerror(errno)));
        close_socket();
        exit(1);
    }
    else
    {
        logMessage(LogLevel::INFO, method, "Отправлено: " + packet);
    }
}

std::string client::recieve()
{
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    const std::string method = "recieve";

    char tmp[4096];
    int len = recv(sock, tmp, sizeof(tmp) - 1, 0);
    if (len == 0) {
        logMessage(LogLevel::INFO, method, "Сервер закрыл соединение");
        return "";
    }
    if (len < 0) {
        logMessage(LogLevel::ERROR, method, "Ошибка приема; код ошибки " + std::to_string(errno));
        close_socket();
        return "";
    }

    tmp[len] = '\0';
    recv_buffer += tmp;  // теперь это поле класса
    logMessage(LogLevel::RECV, method, tmp);

    auto pos = recv_buffer.find('\n');
    if (pos == std::string::npos) {
        return "";
    }

    std::string line = recv_buffer.substr(0, pos);
    recv_buffer.erase(0, pos + 1);
    auto proto = ApplicationProtocol::unpack(line);
    return proto.data();
}
// Закрытие сокета клиента с логированием результата
void client::close_socket()
{
    const std::string method = "client::close_socket";
    logMessage(LogLevel::INFO, method, "Закрываем сокет");
    if (close(sock) < 0)
        logMessage(LogLevel::ERROR, method, "Не удалось закрыть сокет");
}
