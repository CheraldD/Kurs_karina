#include "communicator.h"

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

// Устанавливает соединение с клиентом и инициализирует параметры
int communicator::connect_to_cl()
{
    std::string method_name = "connect_to_cl";

    // Перевод сокета в режим прослушивания
    if (listen(serverSocket, 10) != 0)
    {
        throw critical_error(RED + method_name + " | Сервер не встал на прослушку" + RESET);
    }

    std::cout << GREEN << "[INFO] [" << method_name << "] Сервер слушает входящие соединения..." << RESET << std::endl;

    // Принятие подключения от клиента
    addr_size = sizeof(clientAddr);
    clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addr_size);

    if (clientSocket < 0)
    {
        throw critical_error(RED + method_name + " | Ошибка при принятии соединения" + RESET);
    }

    // Получение ID клиента
    client_id = recv_data(method_name + " | Ошибка при приеме ID клиента");
    if (client_id.empty()) {
        throw critical_error(RED + method_name + " | ID клиента не был получен" + RESET);
    }

    // Получение IP-адреса клиента
    char cl_ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &(clientAddr.sin_addr), cl_ip, INET_ADDRSTRLEN)) {
        throw critical_error(RED + method_name + " | Не удалось получить IP клиента" + RESET);
    }
    client_ip = std::string(cl_ip);
    client_port = ntohs(clientAddr.sin_port);

    // Вывод информации о подключённом клиенте
    std::cout << CYAN << "[INFO] [" << method_name << "] Подключён новый клиент:" << RESET << std::endl;
    std::cout << "    → IP: " << client_ip
              << "\n    → Порт: " << client_port
              << "\n    → ID: " << client_id << std::endl;

    return 0;
}

// Приём данных от клиента с обработкой ошибок
std::string communicator::recv_data(std::string error_message) {
    char buffer[256];
    int bytes_received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
        std::cerr << RED << "[ERROR] " << error_message << RESET << std::endl;
        return "";
    }

    buffer[bytes_received] = '\0';
    return std::string(buffer);
}

// Конструктор: инициализация параметров порта, seed и размера буфера
communicator::communicator(uint port,uint s,uint b)
{
    p = port;
    seed=s;
    buff_size=b;
}

// Главный цикл работы сервера: запуск, обработка подключений
void communicator::work()
{
    const std::string method_name = "work";
    std::cout << GREEN << "[INFO] [" << method_name << "] Сервер запущен и ожидает подключения клиентов..." << RESET << std::endl;

    try {
        start(); // Инициализация сокета
    } catch (const critical_error& e) {
        std::cerr << RED << "[FATAL] [" << method_name << "] " << e.what() << RESET << std::endl;
        return;
    }

    while (true)
    {
        try {
            int result = connect_to_cl(); // Ожидание подключения
            if (result == 0)
            {
                std::cout << GREEN << "[INFO] [" << method_name << "] Принято новое подключение. Запуск потоков клиента." << RESET << std::endl;

                // Подготовка буфера и флагов
                overflowed.store(false);
                output_done = false;

                {
                    std::lock_guard<std::mutex> lg(buffer_mutex);
                    while (!buffer_byte.empty()) buffer_byte.pop();
                }

                // Запуск потоков обработки и вывода данных
                std::thread client_thread(&communicator::handle_client, this);
                std::thread output_thr(&communicator::output_thread, this);

                client_thread.join();
                output_thr.join();

                std::cout << GREEN << "[INFO] [" << method_name << "] Потоки клиента завершены. Ожидание нового подключения..." << RESET << std::endl;
            }
        }
        catch (const critical_error& e) {
            std::cerr << RED << "[ERROR] [" << method_name << "] " << e.what() << "\nПродолжаем ожидание новых клиентов..." << RESET << std::endl;
            continue;
        }
    }
}

// Поток приёма данных от клиента, с контролем переполнения буфера
void communicator::handle_client() {
    interval = 50;
    session_start = std::chrono::steady_clock::now(); 
    send_interval(interval); // Отправка интервала клиенту

    while (true) {
        uint32_t net_data;
        int bytes = recv(clientSocket, &net_data, sizeof(net_data), 0);
        if (bytes <= 0) {
            return; // Завершение при разрыве соединения
        }

        uint32_t data = ntohl(net_data); // Преобразование в хост-байт порядок

        {
            std::lock_guard<std::mutex> lg(buffer_mutex);

            if (overflowed.load()) {
                interval *= 2;
                send_interval(interval);
                continue;
            }

            if (buffer_byte.size() >= buff_size) {
                overflowed.store(true);
                interval *= 2;
                send_interval(interval);
                continue;
            }

            buffer_byte.push(data); // Сохраняем в буфер
            buffer_cv.notify_one();
        }
    }
}

// Поток вывода данных из буфера с задержкой
void communicator::output_thread() {
    std::unique_lock<std::mutex> lk(buffer_mutex);

    while (true) {
        buffer_cv.wait(lk, [this] {
            return !buffer_byte.empty() || (overflowed.load() && buffer_byte.empty());
        });

        if (!buffer_byte.empty()) {
            uint32_t data = buffer_byte.front();
            buffer_byte.pop();

            lk.unlock();

            random_delay(seed); // Искусственная задержка вывода
            std::cout << CYAN << "[RECV] Получено значение: 0x"
                      << std::hex << std::setw(8)
                      << std::setfill('0') << data
                      << " (" << std::dec << data << ")" << RESET << std::endl;

            lk.lock();
        }

        // Если переполнение произошло и буфер опустел — завершаем
        if (overflowed.load() && buffer_byte.empty()) {
            auto session_end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(session_end - session_start).count();
            std::cout << YELLOW << "[INFO] [output_thread] Буфер опустошён после переполнения. Завершаем..." << RESET << std::endl;
            std::cout << YELLOW << "[INFO] [session] Длительность сессии клиента: " << duration << " сек." << RESET << std::endl;
            lk.unlock();
            close_sock();
            return;
        }
    }
}

// Задержка перед выводом, с использованием генератора случайных чисел
void communicator::random_delay(int seed) {
    static std::mt19937 gen(seed);
    std::uniform_int_distribution<> dist(min_delay_ms, max_delay_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
}

// Инициализация и привязка сокета
void communicator::start()
{
    const std::string method_name = "start";

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        throw critical_error(std::string(RED) + "[" + method_name + "] Сокет не был создан" + RESET);
    }

    std::cout << GREEN << "[INFO] [" << method_name << "] Сокет успешно создан" << RESET << std::endl;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(p);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        throw critical_error(std::string(RED) + "[" + method_name + "] Ошибка при привязке сокета" + RESET);
    }

    std::cout << GREEN << "[INFO] [" << method_name << "] Сокет привязан к порту " << p << RESET << std::endl;
}

// Отправка текущего интервала задержки клиенту
void communicator::send_interval(uint32_t interval) {
    uint32_t net_interval = htonl(interval);
    int result = send(clientSocket, &net_interval, sizeof(net_interval), MSG_NOSIGNAL);

    if (result == -1) {
        std::cerr << RED << "[ERROR] Failed to send interval: " << strerror(errno) << RESET << std::endl;
        return;
    }

    std::cout << GREEN << "[INFO] [send_interval] Отправлен интервал клиенту: 0x"
              << std::hex << std::setw(8) << std::setfill('0') << interval
              << " (" << std::dec << interval << " мс)" << RESET << std::endl;
}

// Закрытие соединения с клиентом
void communicator::close_sock()
{
    const std::string method_name = "close_sock";
    std::cout << YELLOW << "[INFO] [" << method_name << "] Соединение с клиентом (ID: "
              << client_id << ", сокет: " << clientSocket << ") разорвано." << RESET << std::endl;
    close(clientSocket);
    interval = 50;
}
