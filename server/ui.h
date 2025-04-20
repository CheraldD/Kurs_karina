
#pragma once
#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <vector>
#include "communicator.h"
#include "error.h"

namespace po = boost::program_options;
class UI
{
private:
    uint port;
    std::string base_loc;
public:
    po::options_description desc;
    po::variables_map vm;
    std::string log_loc;
    UI(int argc, char* argv[]);
    uint get_port();
    std::string get_log_loc();
    uint get_buff_size();
    int get_seed();
};