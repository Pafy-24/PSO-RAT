#pragma once

#include "ClientController.hpp"
#include "Utils/UDPSocket.hpp"
#include <memory>
#include <string>

namespace Client {

class ClientPingController : public ClientController {
public:
    explicit ClientPingController(std::shared_ptr<Utils::TCPSocket> tcpSocket);
    ~ClientPingController();

    
    
    
    bool pingUntilResponse(const std::string &name,
                           const sf::IpAddress &serverIp,
                           unsigned short udpPort,
                           unsigned short tcpPort,
                           int maxAttempts = 10,
                           int intervalMs = 500);

private:
    std::unique_ptr<Utils::UDPSocket> udpSocket_;
};

} 
