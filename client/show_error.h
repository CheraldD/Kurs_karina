#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstring>
#include <stdexcept>
class client_error : public std::runtime_error {
    public:
        client_error(const std::string& s) : std::runtime_error(s) {}
};
class show_error {
    public:
        int show_error_information(std::string function, std::string data, std::string type);
};