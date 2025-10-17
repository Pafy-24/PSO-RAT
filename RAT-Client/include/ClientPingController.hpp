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

    // Send UDP pings to server until it responds. On successful response
    // will call ClientManager::getInstance()->connectTo(name, serverIp, tcpPort).
    // Returns true if connection established.
    bool pingUntilResponse(const std::string &name,
                           const sf::IpAddress &serverIp,
                           unsigned short udpPort,
                           unsigned short tcpPort,
                           int maxAttempts = 10,
                           int intervalMs = 500);

private:
    std::unique_ptr<Utils::UDPSocket> udpSocket_;
};

} // namespace Client
