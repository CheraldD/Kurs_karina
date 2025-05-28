#pragma once
#include "ui.h"
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// Инициализируем интерфейс: парсим и проверяем опции
UI::UI(int argc, char* argv[]) {
    // Описываем доступные ключи
    desc.add_options()
        ("help,h", "Показать справку")  
        ("seed,s", po::value<std::vector<int>>()->multitoken(), "Значения seed для генератора")
        ("buff_size,b", po::value<std::vector<uint>>()->multitoken(), "Размер буфера приема")
        ("interval,i", po::value<std::vector<uint>>()->multitoken(), "Начальный интервал, отправляемый клиенту")
        ("min,m", po::value<std::vector<int>>()->multitoken(), "Начальное значение интервала задержки вывода из буффера")
        ("max,M", po::value<std::vector<int>>()->multitoken(), "Конечное значение интервала задержки вывода из буффера")
        ("Port,p", po::value<std::vector<uint>>()->multitoken(), "Порт сервера"); 

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm); // Парсим CLI

        if (vm.count("help")) { // При запросе помощи
            std::cout << desc << std::endl;
            exit(0);
        }

        // Требуем обязательные опции
        if (!vm.count("Port") || !vm.count("seed") || !vm.count("buff_size") || !vm.count("interval")|| !vm.count("min")|| !vm.count("max")) {
            std::cout << desc << std::endl;
            throw server_error("Не заданы: --Port, --seed или --buff_size или --interval или --min или --max");
        }
        po::notify(vm); // Фиксируем значения
    } catch (po::error& e) {
        throw server_error(std::string("Ошибка аргументов: ") + e.what());
    } catch (server_error& e) {
        std::cout << "Критическая ошибка: " << e.what() << std::endl;
        exit(1);
    }
}

// Получить и проверить порт
uint UI::get_port() {
    auto &ports = vm["Port"].as<std::vector<uint>>();
    if (ports.empty()) throw server_error("--Port пуст");
    uint port = ports.back();
    if (port < 1024 || port > 65535)
        throw server_error("Порт вне диапазона 1024-65535");
    return port;
}
uint UI::get_interval() {
    auto &intervals = vm["interval"].as<std::vector<uint>>();
    if (intervals.empty()) throw server_error("--interval пуст");
    uint interval = intervals.back();
    return interval;
}
int UI::get_max() {
    auto &maxs = vm["max"].as<std::vector<int>>();
    if (maxs.empty()) throw server_error("--max пуст");
    int max = maxs.back();
    if (max<0) throw ("Значение интервала меньше 0");
    return max;
}
int UI::get_min() {
    auto &mins = vm["min"].as<std::vector<int>>();
    if (mins.empty()) throw server_error("--min пуст");
    int min = mins.back();
    if (min<0) throw ("Значение интервала меньше 0");
    return min;
}
// Получить размер буфера
uint UI::get_buff_size() {
    auto &v = vm["buff_size"].as<std::vector<uint>>();
    if (v.empty()) throw server_error("--buff_size пуст");
    return v.back();
}

// Получить seed генератора
int UI::get_seed() {
    auto &s = vm["seed"].as<std::vector<int>>();
    if (s.empty()) throw server_error("--seed пуст");
    return s.back();
}
