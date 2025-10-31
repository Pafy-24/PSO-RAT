#include "ServerPingController.hpp"
#include <thread>
#include <iostream>

namespace Server {

ServerPingController::ServerPingController(Utils::TCPSocket *sock) : ServerController(sock), udpSocket_(std::make_unique<Utils::UDPSocket>()) {}
ServerPingController::~ServerPingController() { running_ = false; }

bool ServerPingController::startUdpResponder(unsigned short udpPort) {
    if (!udpSocket_) return false;
    if (!udpSocket_->bind(udpPort)) return false;
    running_ = true;
    std::thread([this]() {
        while (running_) {
            std::string msg;
            sf::IpAddress sender(0u);
            unsigned short port = 0;
            if (udpSocket_->receive(msg, sender, port)) {
                if (msg == "ping") {
                    udpSocket_->sendTo(std::string("pong"), sender, port);
                }
            }
        }
    }).detach();
    return true;
}

} 
