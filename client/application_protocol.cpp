#include "application_protocol.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

ApplicationProtocol::ApplicationProtocol( std::string header,  std::string data)
    : header_(header), clientID_(generateID()), messageID_(generateID()), data_(data) {}

ApplicationProtocol::ApplicationProtocol( std::string header,
                                          std::string clientID,
                                          std::string messageID,
                                          std::string data)
    : header_(header), clientID_(clientID), messageID_(messageID), data_(data) {}

std::string ApplicationProtocol::pack()  {
    return header_ + "|clientID:" + clientID_ + "|messageID:" + messageID_ + ":" + data_+"\n";
}

ApplicationProtocol ApplicationProtocol::unpack(std::string message) {
    std::string header, clientID, messageID, data;

    // Найдём первую часть до '|'
    size_t pos1 = message.find('|');
    if (pos1 == std::string::npos)
        return ApplicationProtocol(); // Некорректный формат

    header = message.substr(0, pos1);

    size_t pos2 = message.find('|', pos1 + 1);
    if (pos2 == std::string::npos)
        return ApplicationProtocol();

    // clientID:...
    std::string clientPart = message.substr(pos1 + 1, pos2 - pos1 - 1);
    if (clientPart.rfind("clientID:", 0) != 0)
        return ApplicationProtocol();
    clientID = clientPart.substr(9);

    // messageID:...:data
    std::string msgPart = message.substr(pos2 + 1);
    if (msgPart.rfind("messageID:", 0) != 0)
        return ApplicationProtocol();

    size_t colonPos = msgPart.find(':', 10); // после "messageID:"
    if (colonPos == std::string::npos)
        return ApplicationProtocol();

    messageID = msgPart.substr(10, colonPos - 10);
    data = msgPart.substr(colonPos + 1);
    /*std::cout << "[unpack] Parsed message:" << std::endl;
    std::cout << "  Header    : " << header << std::endl;
    std::cout << "  ClientID  : " << clientID << std::endl;
    std::cout << "  MessageID : " << messageID << std::endl;
    std::cout << "  Data      : " << data << std::endl;*/
    return ApplicationProtocol(header, clientID, messageID, data);
}
 std::string ApplicationProtocol::header()  { return header_; }
 std::string ApplicationProtocol::clientID()  { return clientID_; }
 std::string ApplicationProtocol::messageID()  { return messageID_; }
 std::string ApplicationProtocol::data()  { return data_; }

std::string ApplicationProtocol::generateID() {
    static thread_local std::mt19937_64 rng(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << dist(rng);
    return oss.str();
}
