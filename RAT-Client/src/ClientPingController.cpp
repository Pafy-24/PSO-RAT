#include "ClientPingController.hpp"
#include "ClientManager.hpp"
#include <thread>
#include <chrono>
#include <iostream>

namespace Client {

ClientPingController::ClientPingController(std::shared_ptr<Utils::TCPSocket> tcpSocket)
    : ClientController(std::move(tcpSocket)), udpSocket_(std::make_unique<Utils::UDPSocket>()) {}

ClientPingController::~ClientPingController() = default;

bool ClientPingController::pingUntilResponse(const std::string &name,
                                            const sf::IpAddress &serverIp,
                                            unsigned short udpPort,
                                            unsigned short tcpPort,
                                            int maxAttempts,
                                            int intervalMs) {
    if (!udpSocket_) return false;
    for (int i = 0; i < maxAttempts; ++i) {
        std::string ping = "ping";
        if (!udpSocket_->sendTo(ping, serverIp, udpPort)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            continue;
        }

    std::string resp;
    sf::IpAddress sender(0u);
    unsigned short senderPort = 0;
        if (udpSocket_->receive(resp, sender, senderPort)) {
            if (resp == "pong") {
                // server responded, establish TCP connection
                if (ClientManager::getInstance()->connectTo(name, serverIp, tcpPort)) {
                    return true;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
    return false;
}

} // namespace Client
