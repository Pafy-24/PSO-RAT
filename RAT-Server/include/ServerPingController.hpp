#pragma once

#include "IController.hpp"
#include "Utils/UDPSocket.hpp"
#include <memory>
#include <thread>
#include <atomic>

namespace Server {

class ServerManager;

class ServerPingController : public IController {
public:
    explicit ServerPingController(ServerManager *manager);
    ~ServerPingController() override;

    bool startUdpResponder(unsigned short udpPort);
    
    void start() override;
    void stop() override;
    
    void handle(const nlohmann::json &packet) override;
    
    std::string getHandle() const override { return "ping"; }

private:
    ServerManager *manager;
    std::unique_ptr<Utils::UDPSocket> udpSocket;
    std::atomic<bool> running{false};
    std::unique_ptr<std::thread> udpThread;
};

} 
