#pragma once
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <arpa/inet.h>
namespace po = boost::program_options;

class UI {
public:
    po::options_description desc;
    po::variables_map vm;

    UI(int argc, char* argv[]);
    uint get_port();
    std::string get_serv_ip();
    std::string get_username();
};
