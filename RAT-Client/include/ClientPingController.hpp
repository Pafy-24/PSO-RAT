#pragma once

#include "IClientController.hpp"
#include "Utils/UDPSocket.hpp"
#include <memory>
#include <string>
#include <SFML/Network/IpAddress.hpp>

namespace Client {

class ClientPingController : public IClientController {
public:
    ClientPingController();
    ~ClientPingController() override;

    bool discoverServer(const sf::IpAddress &serverIp,
                       unsigned short udpPort,
                       int maxAttempts = 10,
                       int intervalMs = 500);
    
    nlohmann::json handleCommand(const nlohmann::json &request) override;
    std::string getHandle() const override { return "ping"; }

private:
    std::unique_ptr<Utils::UDPSocket> udpSocket_;
};

} 
