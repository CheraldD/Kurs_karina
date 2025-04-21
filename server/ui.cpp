#include "ui.h"
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// Конструктор UI: парсит аргументы командной строки и проверяет обязательные параметры
UI::UI(int argc, char* argv[]) {
    // Описание допустимых опций
    desc.add_options()
        ("help,h", "Помощь")  
        ("seed,s", po::value<std::vector<int>>()->multitoken(), "Точка старта для генератора ПСП")
        ("buff_size,b", po::value<std::vector<uint>>()->multitoken(), "Размер буфера для приема байт от клиента")
        ("Port,p", po::value<std::vector<uint>>()->multitoken(), "Порт сервера"); 

    try {
        // Парсинг аргументов командной строки
        po::store(po::parse_command_line(argc, argv, desc), vm);

        // Если указан флаг помощи — вывести справку и завершить
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            exit(0);
        }

        // Проверка наличия обязательных параметров
        if (!vm.count("Port") || !vm.count("seed") || !vm.count("buff_size")) {
            throw critical_error("Отсутствуют обязательные параметры: --Port, --seed или --buff_size.");
        }

        // Применение опций
        po::notify(vm);
    } catch (po::error& e) {
        // Ошибка синтаксиса при вводе параметров
        throw critical_error(std::string("Ошибка парсинга аргументов: ") + e.what());
    } catch (critical_error& e) {
        // Критическая ошибка конфигурации — вывод и завершение
        std::cout << "Критическая ошибка: " << e.what() << std::endl;
        exit(1);
    }
}

// Получение порта сервера, проверка диапазона
uint UI::get_port() {
    if (vm.count("Port") && !vm["Port"].as<std::vector<uint>>().empty()) {
        const std::vector<uint>& ports = vm["Port"].as<std::vector<uint>>();
        uint port = ports.back();

        // Проверка допустимого диапазона портов
        if (port < 1024 || port > 65535) {
            throw critical_error("Неверное значение порта. Допустимый диапазон: 1024-65535.");
        }

        return port;
    } else {
        // Параметр отсутствует или пуст
        throw critical_error("Параметр --Port не задан или список пуст.");
    }
}

// Получение размера буфера
uint UI::get_buff_size() {
    if (vm.count("buff_size") && !vm["buff_size"].as<std::vector<uint>>().empty()) {
        return vm["buff_size"].as<std::vector<uint>>().back();
    } else {
        // Параметр отсутствует или пуст
        throw critical_error("Параметр --buff_size не задан или список пуст.");
    }
}

// Получение значения seed для генератора
int UI::get_seed() {
    if (vm.count("seed") && !vm["seed"].as<std::vector<int>>().empty()) {
        return vm["seed"].as<std::vector<int>>().back();
    } else {
        // Параметр отсутствует или пуст
        throw critical_error("Параметр --seed не задан или список пуст.");
    }
}
