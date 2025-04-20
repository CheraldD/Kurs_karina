#include "client.h"
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
    const std::string method_name = "client::work";
    serv_ip = intf.get_serv_ip().c_str();
    port = intf.get_port();
    id = intf.get_username();
    std::cout << "[INFO] " << method_name << " | Начало работы клиента." << std::endl;
    start();
    connect_to_server();
    std::chrono::milliseconds duration(5);
    while (true)
    {
        uint32_t new_interval;
            int bytes_received = recv(sock, &new_interval, sizeof(new_interval), MSG_DONTWAIT);
            if (bytes_received == 0){
                break;
            }
            if (bytes_received > 0) {
                interval = ntohl(new_interval);
                std::cout << "New interval: " << interval << "ms\n";
            }
        std::this_thread::sleep_for(duration);
        uint32_t data=htonl(generate_random_uint32());
        send(sock, &data, sizeof(data), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
    close_sock();
    std::cout << "[INFO] " << method_name << " | Клиент завершил работу." << std::endl;
    exit(1);
}
void client::start()
{
    signal(SIGPIPE, SIG_IGN);
    std::cout << "[INFO] Начало создания сокета..." << std::endl;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "[ERROR] Не удалось создать сокет!" << std::endl;
        return;
    }
    std::cout << "[INFO] Сокет успешно создан" << std::endl;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    std::cout << "[INFO] Настройка адреса сервера: " << serv_ip << ":" << port << std::endl;
    inet_pton(AF_INET, serv_ip, &serverAddr.sin_addr);
    std::cout << "[INFO] Адрес сервера успешно настроен" << std::endl;
}

void client::connect_to_server()
{
    std::cout << "[INFO] Получаем информацию о локальном сокете..." << std::endl;
    sockaddr_in localAddr{};
    socklen_t addrLen = sizeof(localAddr);
    if (getsockname(sock, (struct sockaddr *)&localAddr, &addrLen) < 0)
    {
        std::cerr << "[ERROR] Ошибка получения информации о сокете" << std::endl;
        return;
    }
    std::cout << "[INFO] Локальный адрес сокета получен: " << inet_ntoa(localAddr.sin_addr) << std::endl;
    std::cout << "[INFO] Пытаемся подключиться к серверу..." << std::endl;
    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        close_sock();
        std::cerr << "[ERROR] Ошибка подключения к серверу. Проверьте IP или порт." << std::endl;
        return;
    }
    std::cout << "[INFO] Клиент успешно подключился к серверу" << std::endl;
    std::cout << "[INFO] Отправляем идентификатор клиента: " << id << std::endl;
    send_data(id);
}
std::string client::recv_data()
{
    const std::string method_name = "recv_data";
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    std::chrono::milliseconds duration(10);
    int rc = 0;
    std::cout << "[INFO] " << method_name << " | Ожидаем данные от сервера..." << std::endl;
    while (true)
    {
        buffer = std::unique_ptr<char[]>(new char[buflen]);
        std::this_thread::sleep_for(duration);
        rc = recv(sock, buffer.get(), buflen, MSG_PEEK);

        if (rc == 0)
        {
            std::cerr << "[ERROR] " << method_name << " | Принято 0 байт от сервера. Закрываем сокет." << std::endl;
            close_sock();
            return "";
        }
        else if (rc < 0)
        {
            std::cerr << "[ERROR] " << method_name << " | Ошибка при получении данных: recv() вернуло " << rc << std::endl;
            close_sock();
            return "";
        }

        if (rc < buflen)
            break;
        buflen *= 2;
        std::cout << "[INFO] " << method_name << " | Увеличиваем буфер до " << buflen << " байт." << std::endl;
    }
    std::string msg(buffer.get(), rc);
    std::this_thread::sleep_for(duration);
    if (recv(sock, nullptr, rc, MSG_TRUNC) < 0)
    {
        std::cerr << "[ERROR] " << method_name << " | Ошибка при подтверждении получения данных (MSG_TRUNC)." << std::endl;
        close_sock();
        return "";
    }

    std::cout << "[INFO] " << method_name << " | Данные успешно получены от сервера. Размер данных: " << rc << " байт." << std::endl;

    return msg;
}
void client::close_sock()
{
    std::cout << "[INFO] Закрытие сокета клиента..." << std::endl;

    if (close(sock) == 0)
    {
        std::cout << "[INFO] Сокет клиента успешно закрыт" << std::endl;
    }
    else
    {
        std::cerr << "[ERROR] Ошибка при закрытии сокета клиента" << std::endl;
    }
}
void client::send_data(std::string data)
{
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    std::chrono::milliseconds duration(10);
    std::unique_ptr<char[]> temp{new char[data.length() + 1]};
    strcpy(temp.get(), data.c_str());
    buffer = std::move(temp);
    std::this_thread::sleep_for(duration);
    int sb = send(sock, buffer.get(), data.length(), 0);

    if (sb < 0)
    {
        std::cerr << "[ERROR] Ошибка отправки данных строкового типа" << std::endl;
        close_sock();
        return;
    }
    std::cout << "[INFO] Отправлены данные строкового типа: \"" << data << "\" (Размер: " << data.length() << " байт)" << std::endl;
}