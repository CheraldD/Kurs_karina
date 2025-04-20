#include "ui.h"
#include <boost/program_options.hpp>
namespace po = boost::program_options;
UI::UI(int argc, char *argv[])
{
    desc.add_options()
        ("help,h", "Помощь\nВсе параметры ниже являются обязательными")
        ("serv_ip,s", po::value<std::vector<std::string>>()->multitoken(), "Айпи сервера")
        ("username, u", po::value<std::vector<std::string>>()->multitoken(), "Имя пользователя")
        ("port,p", po::value<std::vector<uint>>()->multitoken(), "Порт сервера");
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help") or !vm.count("serv_ip") or !vm.count("port")or !vm.count("username")) {
        std::cout << desc << std::endl;
        exit(0);
    }
    po::notify(vm);
}
std::string UI::get_username(){
    if (vm.count("username") and !vm["username"].as<std::vector<std::string>>().empty())
    {
        const std::vector<std::string> &username = vm["username"].as<std::vector<std::string>>();
        return username.back();
    }
    else
    {
        std::cout << desc << std::endl;
        debugger.show_error_information("Ошибка в get_username()", "Неопределенное значение имени пользователя", "Неопределенная ошибка");
        return "";
    }
}
uint UI::get_port()
{
    if (vm.count("port") and !vm["port"].as<std::vector<uint>>().empty())
    {
        const std::vector<uint> &ports = vm["port"].as<std::vector<uint>>();
        if (ports.back() < 1024)
        {
            std::cout << desc << std::endl;
            return 1;
        }
        if (ports.back() > 65535)
        {
            std::cout << desc << std::endl;
            return 1;
        }

        return ports.back();
    }
    else
    {
        std::cout << desc << std::endl;
        return 1;
    }
}

std::string UI::get_serv_ip()
{
    struct in_addr addr;

    if (vm.count("serv_ip") and !vm["serv_ip"].as<std::vector<std::string>>().empty())
    {
        const std::vector<std::string> &ip_s = vm["serv_ip"].as<std::vector<std::string>>();
        if (inet_pton(AF_INET, ip_s.back().c_str(), &addr) == 0)
        {
            std::cout << desc << std::endl;
            return "";
        }

        return ip_s.back();
    }
    else
    {
        std::cout << desc << std::endl;
        return "";
    }
}