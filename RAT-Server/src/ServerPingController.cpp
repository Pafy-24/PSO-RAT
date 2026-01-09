#include "ServerPingController.hpp"
#include "ServerManager.hpp"
#include <thread>
#include <chrono>

namespace Server {

ServerPingController::ServerPingController(ServerManager *manager) 
    : IController(manager), manager(manager), udpSocket(std::make_unique<Utils::UDPSocket>()) {}

ServerPingController::~ServerPingController() { 
    stop(); 
}

void ServerPingController::handle(const nlohmann::json &packet) {
    
    (void)packet;
}

void ServerPingController::start() {
    if (running) return;
    running = true;
    
}

void ServerPingController::stop() {
    running = false;
    if (udpThread && udpThread->joinable()) {
        udpThread->join();
    }
}

bool ServerPingController::startUdpResponder(unsigned short udpPort) {
    if (!udpSocket) return false;
    
    udpSocket->setBlocking(false);
    
    if (!udpSocket->bind(udpPort)) {
        if (manager) {
            manager->pushLog("Failed to bind UDP port " + std::to_string(udpPort) + " for discovery");
        }
        return false;
    }
    
    running = true;
    
    udpThread = std::make_unique<std::thread>([this]() {
        while (running) {
            std::string msg;
            sf::IpAddress sender(0u);
            unsigned short port = 0;
            
            if (udpSocket->receive(msg, sender, port)) {
                if (msg == "ping") {
                    udpSocket->sendTo(std::string("pong"), sender, port);
                    
                    if (manager) {
                        manager->pushLog("UDP discovery ping from " + sender.toString());
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
    
    if (manager) {
        manager->pushLog("UDP discovery responder started on port " + std::to_string(udpPort));
    }
    
    return true;
}

} 
