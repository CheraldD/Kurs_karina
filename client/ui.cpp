#pragma once  // Защита от повторного включения заголовка
#include "ui.h"
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// Конструктор: настраивает и парсит опции командной строки
UI::UI(int argc, char *argv[])
{
    // Описание доступных опций
    desc.add_options()
        ("help,h",    "Помощь\nВсе параметры ниже являются обязательными")
        ("serv_ip,s", po::value<std::vector<std::string>>()->multitoken(), "IP адрес сервера")
        ("username,u",po::value<std::vector<std::string>>()->multitoken(), "Имя пользователя")
        ("port,p",    po::value<std::vector<uint>>()->multitoken(), "Порт сервера");

    // Чтение и хранение введённых опций
    po::store(po::parse_command_line(argc, argv, desc), vm);

    // При запросе помощи или отсутствии обязательных опций выводим справку и выходим
    if (vm.count("help") || !vm.count("serv_ip") || !vm.count("port") || !vm.count("username")) {
        std::cout << desc << std::endl;
        exit(0);
    }

    // Фиксируем значения опций в vm
    po::notify(vm);
}

// Возвращает указанное имя пользователя или пустую строку при ошибке
std::string UI::get_username(){
    // Проверяем, что ключ есть и список не пуст
    if (vm.count("username") && !vm["username"].as<std::vector<std::string>>().empty()) {
        auto &names = vm["username"].as<std::vector<std::string>>();
        return names.back();  // последнее значение
    }
    // Ошибка: выводим справку
    std::cout << desc << std::endl;
    return "";
}

// Возвращает порт сервера (1 при ошибке диапазона или отсутствии)
uint UI::get_port() {
    if (vm.count("port") && !vm["port"].as<std::vector<uint>>().empty()) {
        auto &ports = vm["port"].as<std::vector<uint>>();
        uint p = ports.back();            
        if (p < 1024 || p > 65535) {      
            std::cout << desc << std::endl;  
            return 1;                    
        }
        return p;                        
    }
    // Ошибка: нет порта
    std::cout << desc << std::endl;
    return 1;
}

// Возвращает IP сервера или пустую строку при некорректном формате
std::string UI::get_serv_ip() {
    if (vm.count("serv_ip") && !vm["serv_ip"].as<std::vector<std::string>>().empty()) {
        auto &ips = vm["serv_ip"].as<std::vector<std::string>>();
        struct in_addr addr;
        // Проверяем корректность формата IPv4
        if (inet_pton(AF_INET, ips.back().c_str(), &addr) != 1) {
            std::cout << desc << std::endl;
            return "";
        }
        return ips.back();  // возвращаем последний IP
    }
    // Ошибка: IP не задан
    std::cout << desc << std::endl;
    return "";
}
