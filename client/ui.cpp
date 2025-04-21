#include "ui.h"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

UI::UI(int argc, char *argv[])
{
    // Описание параметров командной строки
    desc.add_options()
        ("help,h", "Помощь\nВсе параметры ниже являются обязательными")
        ("serv_ip,s", po::value<std::vector<std::string>>()->multitoken(), "Айпи сервера")
        ("username,u", po::value<std::vector<std::string>>()->multitoken(), "Имя пользователя")
        ("port,p", po::value<std::vector<uint>>()->multitoken(), "Порт сервера");

    // Парсинг аргументов командной строки
    po::store(po::parse_command_line(argc, argv, desc), vm);

    // Если не заданы обязательные параметры или запрошена помощь — выводим справку
    if (vm.count("help") || !vm.count("serv_ip") || !vm.count("port") || !vm.count("username")) {
        std::cout << desc << std::endl;
        exit(0);
    }

    // Применяем параметры после парсинга
    po::notify(vm);
}

// Получение имени пользователя
std::string UI::get_username(){
    if (vm.count("username") && !vm["username"].as<std::vector<std::string>>().empty())
    {
        const std::vector<std::string> &username = vm["username"].as<std::vector<std::string>>();
        return username.back(); // Возвращаем последнее указанное имя
    }
    else
    {
        std::cout << desc << std::endl;
        debugger.show_error_information("Ошибка в get_username()", "Неопределенное значение имени пользователя", "Неопределенная ошибка");
        return "";
    }
}

// Получение порта сервера
uint UI::get_port()
{
    if (vm.count("port") && !vm["port"].as<std::vector<uint>>().empty())
    {
        const std::vector<uint> &ports = vm["port"].as<std::vector<uint>>();
        if (ports.back() < 1024) // Зарезервированные порты
        {
            std::cout << desc << std::endl;
            return 1;
        }
        if (ports.back() > 65535) // Верхний предел порта
        {
            std::cout << desc << std::endl;
            return 1;
        }

        return ports.back(); // Возвращаем последний указанный порт
    }
    else
    {
        std::cout << desc << std::endl;
        return 1;
    }
}

// Получение IP-адреса сервера
std::string UI::get_serv_ip()
{
    struct in_addr addr;

    if (vm.count("serv_ip") && !vm["serv_ip"].as<std::vector<std::string>>().empty())
    {
        const std::vector<std::string> &ip_s = vm["serv_ip"].as<std::vector<std::string>>();
        
        // Проверка корректности IP
        if (inet_pton(AF_INET, ip_s.back().c_str(), &addr) == 0)
        {
            std::cout << desc << std::endl;
            return "";
        }

        return ip_s.back(); // Возвращаем последний указанный IP
    }
    else
    {
        std::cout << desc << std::endl;
        return "";
    }
}
