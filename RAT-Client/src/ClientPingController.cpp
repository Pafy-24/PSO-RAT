#include "ClientPingController.hpp"
#include <thread>
#include <chrono>

namespace Client {

ClientPingController::ClientPingController() 
    : udpSocket_(std::make_unique<Utils::UDPSocket>()) {
    if (udpSocket_) {
        udpSocket_->setBlocking(false);
    }
}

ClientPingController::~ClientPingController() = default;

nlohmann::json ClientPingController::handleCommand(const nlohmann::json &request) {
    (void)request;
    nlohmann::json reply;
    reply["controller"] = "ping";
    reply["out"] = "Ping controller is for internal discovery only";
    reply["ret"] = -1;
    return reply;
}

bool ClientPingController::discoverServer(const sf::IpAddress &serverIp,
                                         unsigned short udpPort,
                                         int maxAttempts,
                                         int intervalMs) {
    if (!udpSocket_) return false;
    
    for (int i = 0; i < maxAttempts; ++i) {
        // Trimite ping UDP
        std::string ping = "ping";
        udpSocket_->sendTo(ping, serverIp, udpPort);
        
        // Așteaptă răspuns
        std::string resp;
        sf::IpAddress sender(0u);
        unsigned short senderPort = 0;
        
        // Încearcă să primească răspuns timp de intervalMs
        auto startTime = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - startTime).count() < intervalMs) {
            if (udpSocket_->receive(resp, sender, senderPort)) {
                if (resp == "pong") {
                    return true; // Server găsit!
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    return false;
}

} 
