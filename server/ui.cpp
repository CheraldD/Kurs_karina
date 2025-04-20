#include "ui.h"
#include <boost/program_options.hpp>
namespace po = boost::program_options;
UI::UI(int argc, char* argv[]) {
    desc.add_options()
        ("help,h", "Помощь")  
        //("Log_loc,l", po::value<std::vector<std::string>>()->multitoken(), "Путь для log файла")  
        ("seed,s",po::value<std::vector<int>>()->multitoken(),"Точка старта для генератора псп")
        ("buff_size,b",po::value<std::vector<uint>>()->multitoken(),"Размер буффера для приема байт от клиента")
        ("Port,p", po::value<std::vector<uint>>()->multitoken(), "Порт сервера"); 

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help") || /*!vm.count("Log_loc") ||*/ !vm.count("Port") ||!vm.count("seed")||!vm.count("buff_size")) {
            std::cout << desc << std::endl;
            exit(0); 
        }
        po::notify(vm);
    } catch (po::error& e) {
        std::cout << e.what() << std::endl;
    }
    catch(critical_error &e){
        std::cout << "Критическая ошибка: " << e.what() << std::endl;
    }
}
uint UI::get_port() {
    if (vm.count("Port")) {
        const std::vector<uint>& ports = vm["Port"].as<std::vector<uint>>();
        if (ports.back() < 1024) {
            throw critical_error("Выбран системный порт");
        }

        return ports.back();
    } else {
        return 1;
    }
}
uint UI::get_buff_size() {
    if (vm.count("buff_size")) {
        const std::vector<uint>& b_size = vm["buff_size"].as<std::vector<uint>>();
        return b_size.back();
    } else {
        return 1;
    }
}
int UI::get_seed() {
    if (vm.count("seed")) {
        const std::vector<int>& seeds = vm["seed"].as<std::vector<int>>();
        return seeds.back();
    } else {
        return 1;
    }
}