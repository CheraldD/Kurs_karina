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
class communicator
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
    uint seed;
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
    communicator(uint port,uint s,uint b);
    int connect_to_cl();
    void send_data(std::string data, std::string msg);
    void send_interval(uint32_t serv_interval);
    std::string recv_data(std::string messg);
    void close_sock(); 
    void work();
    void start();
    void random_delay(int seed);
    void handle_client();
    void output_thread();
    std::string hash_gen(std::string &password);
};
