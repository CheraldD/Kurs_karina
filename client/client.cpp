#include "client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <csignal>

// ANSI color codes for console output
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

uint32_t client::generate_random_uint32()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dist(
        0, std::numeric_limits<uint32_t>::max());
    return dist(gen);
}

void client::work(UI &intf)
{
    const std::string method = "client::work";

    // Получаем IP сервера
    std::string sip = intf.get_serv_ip();
    if (sip.empty())
        throw client_error(std::string(RED) + "[" + method + "] Не удалось получить IP" + RESET);
    serv_ip = sip.c_str();

    // Получаем порт
    port = intf.get_port();
    if (port == 1)
        throw client_error(std::string(RED) + "[" + method + "] Недопустимый порт" + RESET);

    // Получаем имя пользователя
    id = intf.get_username();
    if (id.empty())
        throw client_error(std::string(RED) + "[" + method + "] Не указано имя пользователя" + RESET);

    std::cout << BOLD << GREEN
              << "[INFO] [" << method << "] Запуск клиента: "
              << "IP=" << serv_ip << ", Port=" << port
              << ", User=" << id << RESET << std::endl;

    start();
    connect_to_server();

    std::chrono::milliseconds tick(5);
    while (true)
    {
        uint32_t net_int;
        int n = recv(sock, &net_int, sizeof(net_int), MSG_DONTWAIT);
        if (n == 0) break;  // Сервер закрыл соединение
        if (n > 0) {
            interval = ntohl(net_int);
            std::cout << CYAN
                      << "[INFO] [" << method << "] Новый интервал: "
                      << interval << " ms" << RESET << std::endl;
        }

        std::this_thread::sleep_for(tick);
        uint32_t rnd = htonl(generate_random_uint32());
        if (send(sock, &rnd, sizeof(rnd), 0) < 0) {
            throw client_error(std::string(RED) + "[" + method + "] Ошибка отправки рандомного числа" + RESET);
        }

        std::cout << BOLD << GREEN
                  << "[SEND] [" << method << "] Отправлено число: 0x"
                  << std::hex << rnd << std::dec << RESET << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }

    close_sock();
    std::cout << BOLD << YELLOW
              << "[INFO] [" << method << "] Клиент завершил работу" << RESET << std::endl;
}

void client::start()
{
    const std::string method = "client::start";
    std::signal(SIGPIPE, SIG_IGN);

    std::cout << BOLD << GREEN
              << "[INFO] [" << method << "] Создание сокета..." << RESET << std::endl;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        throw client_error(std::string(RED) + "[" + method + "] Не удалось создать сокет" + RESET);

    std::cout << CYAN
              << "[INFO] [" << method << "] Сокет создан" << RESET << std::endl;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(port);
    if (inet_pton(AF_INET, serv_ip, &serverAddr.sin_addr) <= 0)
        throw client_error(std::string(RED) + "[" + method + "] Ошибка преобразования IP" + RESET);

    std::cout << CYAN
              << "[INFO] [" << method << "] Адрес сервера: "
              << serv_ip << ":" << port << RESET << std::endl;
}

void client::connect_to_server()
{
    const std::string method = "client::connect_to_server";
    std::cout << BOLD << GREEN
              << "[INFO] [" << method << "] Подключаемся к серверу..." << RESET << std::endl;

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        close_sock();
        throw client_error(std::string(RED) + "[" + method + "] Ошибка подключения к серверу" + RESET);
    }

    std::cout << CYAN
              << "[INFO] [" << method << "] Подключение установлено" << RESET << std::endl;
    std::cout << BOLD << GREEN
              << "[SEND] [" << method << "] Отправляем ID: " << id << RESET << std::endl;
    send_data(id);
}

std::string client::recv_data()
{
    const std::string method = "client::recv_data";
    timeout.tv_sec  = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    std::cout << BOLD << GREEN
              << "[INFO] [" << method << "] Ожидание сообщения от сервера..." << RESET << std::endl;
    int rc;
    while (true)
    {
        buffer = std::make_unique<char[]>(buflen);
        rc = recv(sock, buffer.get(), buflen, MSG_PEEK);

        if (rc == 0) {
            close_sock();
            throw client_error(std::string(RED) + "[" + method + "] Соединение закрыто" + RESET);
        }
        if (rc < 0) {
            close_sock();
            throw client_error(std::string(RED) + "[" + method + "] Ошибка recv()" + RESET);
        }
        if (static_cast<size_t>(rc) < buflen) break;

        buflen *= 2;
        std::cout << YELLOW
                  << "[INFO] [" << method << "] Буфер увеличен до "
                  << buflen << " байт" << RESET << std::endl;
    }

    // Читаем сообщение полностью
    std::string msg(buffer.get(), rc);
    if (recv(sock, nullptr, rc, MSG_TRUNC) < 0) {
        close_sock();
        throw client_error(std::string(RED) + "[" + method + "] Ошибка MSG_TRUNC" + RESET);
    }

    std::cout << BOLD << GREEN
              << "[RECV] [" << method << "] Получено сообщение: "
              << msg << " (" << msg.size() << " байт)" << RESET << std::endl;
    return msg;
}

void client::close_sock()
{
    const std::string method = "client::close_sock";
    std::cout << BOLD << YELLOW
              << "[INFO] [" << method << "] Закрытие сокета..." << RESET << std::endl;

    if (close(sock) == 0)
        std::cout << CYAN << "[INFO] [" << method << "] Сокет закрыт" << RESET << std::endl;
    else
        std::cerr << RED << "[ERROR] [" << method << "] Не удалось закрыть сокет" << RESET << std::endl;
}

void client::send_data(std::string data)
{
    const std::string method = "client::send_data";
    std::cout << BOLD << GREEN
              << "[INFO] [" << method << "] Отправка данных: \"" << data
              << "\" (" << data.size() << " байт)" << RESET << std::endl;

    if (send(sock, data.c_str(), data.size(), 0) < 0) {
        close_sock();
        throw client_error(std::string(RED) + "[" + method + "] Ошибка send()" + RESET);
    }

    std::cout << CYAN
              << "[SEND] [" << method << "] Данные успешно отправлены" << RESET << std::endl;
}
