#pragma once
#include <string>
#include <iostream>
class ApplicationProtocol {
public:
    ApplicationProtocol( std::string header,  std::string data);
    ApplicationProtocol( std::string header,
                         std::string clientID,
                         std::string messageID,
                         std::string data);

    std::string pack() ;
    static ApplicationProtocol unpack( std::string message);
    ApplicationProtocol(){};
     std::string header() ;
     std::string clientID() ;
     std::string messageID() ;
     std::string data() ;
    static std::string generateID();
private:
    std::string header_;
    std::string clientID_;
    std::string messageID_;
    std::string data_;

    
};
