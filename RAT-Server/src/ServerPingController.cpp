#include "ServerPingController.hpp"
#include "ServerManager.hpp"
#include <thread>
#include <chrono>

namespace Server {

ServerPingController::ServerPingController(ServerManager *manager) 
    : IController(manager), manager_(manager), udpSocket_(std::make_unique<Utils::UDPSocket>()) {}

ServerPingController::~ServerPingController() { 
    stop(); 
}

void ServerPingController::handle(const nlohmann::json &packet) {
    // PingController handles UDP discovery, not TCP packets
    (void)packet;
}

void ServerPingController::start() {
    if (running_) return;
    running_ = true;
}

void ServerPingController::stop() {
    if (!running_) return;
    running_ = false;
    if (udpThread_ && udpThread_->joinable()) {
        udpThread_->join();
    }
}

bool ServerPingController::startUdpResponder(unsigned short udpPort) {
    if (!udpSocket_) return false;
    
    udpSocket_->setBlocking(false);
    
    if (!udpSocket_->bind(udpPort)) {
        if (manager_) {
            manager_->pushLog("Failed to bind UDP port " + std::to_string(udpPort) + " for discovery");
        }
        return false;
    }
    
    running_ = true;
    
    udpThread_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            std::string msg;
            sf::IpAddress sender(0u);
            unsigned short port = 0;
            
            if (udpSocket_->receive(msg, sender, port)) {
                if (msg == "ping") {
                    udpSocket_->sendTo(std::string("pong"), sender, port);
                    
                    if (manager_) {
                        manager_->pushLog("UDP discovery ping from " + sender.toString());
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
    
    if (manager_) {
        manager_->pushLog("UDP discovery responder started on port " + std::to_string(udpPort));
    }
    
    return true;
}

} 
