#include "communicator.h"

int communicator::connect_to_cl()
{
    std::string method_name="connect_to_cl";
    if (listen(serverSocket, 10) != 0)
    {
        throw critical_error("Сервер не встал на прослушку");
    }
    addr_size = sizeof(clientAddr);
    clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addr_size);
    if (clientSocket < 0)
    {
        std::cerr << "[ERROR] [" << "] Ошибка при принятии соединения!" << std::endl;
        return -1;
    }
    client_id = recv_data(method_name + " | Ошибка при приеме ID клиента");
    char cl_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), cl_ip, INET_ADDRSTRLEN);
    client_ip=std::string(cl_ip);
    client_port = ntohs(clientAddr.sin_port);
    return 0;
}
communicator::communicator(uint port,uint s,uint b)
{
    p = port;
    seed=s;
    buff_size=b;
}
void communicator::work()
{
    const std::string method_name = "work";
    std::cout << "[INFO] [" << method_name << "] Сервер запущен и ожидает подключения клиентов..." << std::endl;
    start();
    std::thread output_thr(&communicator::output_thread, this);
    output_thr.detach();
    while (true)
    {
        int result = connect_to_cl();

        if (result == 0)
        {
            std::cout << "[INFO] [" << method_name << "] Принято новое подключение. Запуск потока обработки клиента." << std::endl;
            std::thread client_thread(&communicator::handle_client, this);
            client_thread.detach();

        }
        else
        {
            std::cerr << "[ERROR] [" << method_name << "] Ошибка подключения клиента, продолжаем ожидание..." << std::endl;
        }
    }
}
void communicator::random_delay(int seed) {
    static std::mt19937 gen(seed);
    std::uniform_int_distribution<> dist(min_delay_ms, max_delay_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
}
void communicator::handle_client(){
    auto start_time = std::chrono::system_clock::now();
    send_interval(interval);
        while (true) {
            uint32_t data;
            int bytes_received = recv(clientSocket, &data, sizeof(data), 0);
            if (bytes_received <= 0) break;
            data=ntohl(data);
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                if (buffer_byte.size() < buff_size) {
                    buffer_byte.push(data);
                } else {
                    interval *= 2;
                    send_interval(interval);
                }
            }
        }
    auto end_time = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    close_sock();
}
void communicator::output_thread(){
    while (true) {
        if (!buffer_byte.empty()) {
            uint32_t data;
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                data = buffer_byte.front();
                buffer_byte.pop();
            }
            std::cout << "Received: 0x" 
                      << std::hex << std::setw(8) << std::setfill('0') << data<<std::endl;
        }
        random_delay(seed);
    }
}
void communicator::start()
{
    const std::string method_name = "start";
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        std::cerr << "[ERROR] [" << method_name << "] Ошибка при создании сокета" << std::endl;
        throw critical_error("Сокет не был создан");
    }
    std::cout << "[INFO] [" << method_name << "] Сокет создан" << std::endl;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(p);          
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "[ERROR] [" << method_name << "] Ошибка при привязке сокета" << std::endl;
        throw critical_error("Сокет не был привязан");
    }
    std::cout << "[INFO] [" << method_name << "] Сокет привязан" << std::endl;
}
std::string communicator::recv_data(std::string messg)
{
    const std::string method_name = "recv_data";
    timeout.tv_sec = 100;
    timeout.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    int rc = 0;
    size_t peek_buflen = buflen;
    std::vector<char> temp_buffer(peek_buflen);
    std::cout << "[INFO] [" << method_name << "] Начало приема данных от клиента (ID: " << clientSocket << ")" << std::endl;
    while (true)
    {
        rc = recv(clientSocket, temp_buffer.data(), peek_buflen, MSG_PEEK);
        if (rc == 0)
        {
            close_sock();
            std::cerr << "[ERROR] [" << method_name << "] Клиент закрыл соединение (ID: " << clientSocket << ")" << std::endl;
            return "";
        }
        else if (rc < 0)
        {
            close_sock();
            std::cerr << "[ERROR] [" << method_name << "] " << messg << std::endl;
            return "";
        }

        if (static_cast<size_t>(rc) < peek_buflen)
            break;
        peek_buflen *= 2;
        temp_buffer.resize(peek_buflen);
    }

    std::string msg(temp_buffer.data(), rc);
    if (recv(clientSocket, nullptr, rc, MSG_TRUNC) <= 0)
    {
        close_sock();
        std::cerr << "[ERROR] [" << method_name << "] " << messg << std::endl;
        return "";
    }
    std::cout << "[INFO] [" << method_name << "] Строка принята от клиента (ID: " << clientSocket << "): " << msg << std::endl;
    return msg;
}
void communicator::send_data(std::string data, std::string msg)
{
    const std::string method_name = "send_data";
    std::cout << "[INFO] [" << method_name << "] Начало отправки данных клиенту (ID: " << clientSocket << ")" << std::endl;
    std::chrono::milliseconds duration(10);
    std::unique_ptr<char[]> temp{new char[data.length() + 1]};
    strcpy(temp.get(), data.c_str());
    buffer = std::move(temp);
    std::this_thread::sleep_for(duration);
    int sb = send(clientSocket, buffer.get(), data.length(), 0);
    if (sb <= 0)
    {
        std::cerr << "[ERROR] [" << method_name << "] Ошибка отправки данных клиенту (ID: " << clientSocket << ")" << std::endl;
        close_sock();
        return;
    }
    std::cout << "[INFO] [" << method_name << "] Данные успешно отправлены клиенту (ID: " << clientSocket << ")" << std::endl;
}
void communicator::send_interval(uint32_t serv_interval) {
    uint32_t net_interval = htonl(serv_interval);
    std::string data(reinterpret_cast<char*>(&net_interval), sizeof(net_interval));
    send_data(data, "interval update");
}

void communicator::close_sock()
{
    const std::string method_name = "close_sock";
    std::cout << "[INFO] [" << method_name << "] Разорвано соединение с клиентом (ID: " << ")" << std::endl;
    close(clientSocket);
    std::time_t now = std::time(nullptr);
    char timestamp[100];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
}
std::string communicator::hash_gen(std::string &password)
{
    // Создаем объект для алгоритма хэширования SHA256
    CryptoPP::SHA256 hash;
    std::string hashed_password;

    // Применяем хэширование:
    // StringSource - источник данных (строка с паролем), передаем его в хэш-фильтр
    // HashFilter - фильтрует и хэширует данные через алгоритм SHA256
    // HexEncoder - кодирует результат хэширования в строку в формате шестнадцатеричных символов
    // StringSink - принимает результат в виде строки
    CryptoPP::StringSource(password, true,
                           new CryptoPP::HashFilter(hash,
                                                    new CryptoPP::HexEncoder(
                                                        new CryptoPP::StringSink(hashed_password))));

    // Возвращаем хэшированную строку пароля
    return hashed_password;
}
