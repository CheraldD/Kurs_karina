#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <memory>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <mutex>
#include <random>
#include <chrono>
#include <thread>
#include <limits>
#include "error.h"
#include <cryptopp/cryptlib.h>
#include <cryptopp/hex.h>
#include <cryptopp/files.h>
#include <cryptopp/sha.h> 
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>
#include <queue>
#include <iomanip>
#include <atomic>
#include <condition_variable>
class server
{    
private:
    std::chrono::steady_clock::time_point session_start;
    std::atomic<bool> overflowed{false};
    std::condition_variable buffer_cv;
    std::mutex buffer_mutex;
    bool output_done = false;   
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_size;
    size_t buflen = 1024;
    std::unique_ptr<char[]> buffer{new char[buflen]};
    uint p;
    int seed;
    uint buff_size;
    uint32_t interval;
    int client_port;
    int min_delay_ms = 100;
    int max_delay_ms = 1000;
    std::string client_ip;
    std::string client_id;
public:
    std::queue<uint32_t> buffer_byte;
    int serverSocket,clientSocket;
    std::string cl_id, log_location;
    timeval timeout{};
    server(uint port,int s,uint b);
    int connection();
    void transfer_data(const std::string &header, const std::string &data);
    void update_interval(uint32_t serv_interval);
    void update_interval(std::string hollow);
    std::string recieve(std::string messg);
    void close_socket(); 
    void run();
    void steady();
    void random_delay(int seed);
    void work_with_client();
    void output_thread();
};
