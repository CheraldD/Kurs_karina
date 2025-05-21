#pragma once
#include <iostream>
#include <string>
#include <cstring>
#include <memory>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <random>
#include <chrono>
#include <thread>
#include <limits>
#include <iomanip>
#include <cstdint>
#include "ui.h"
#include <csignal>
#include "error.h"
#include <atomic>
class client {
private:
    timeval timeout{};
    socklen_t addr_size;
    size_t buflen = 1024;
    std::unique_ptr<char[]> buffer{new char[buflen]};
    struct sockaddr_in serverAddr;
    
    uint32_t generate_random_uint32();

public:
    u_int32_t interval=0;
    const char* serv_ip;
    int sock;
    uint port;
    std::string id;
    std::string recieve();
    void connection();
    void transfer_data(const std::string &header, const std::string &data);
    void close_socket();
    void steady();
    void run(UI &intf);
};