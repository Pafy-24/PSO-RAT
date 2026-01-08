#pragma once

#include "Utils/TCPSocket.hpp"
#include <nlohmann/json.hpp>
#include "IController.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <string>
#include <thread>
#include <atomic>

namespace Server {

class ServerController : public IController {
public:
    explicit ServerController(Utils::TCPSocket *socket);
    
    explicit ServerController(ServerManager *manager, const std::string &deviceName, Utils::TCPSocket *socket);
    ~ServerController();

    ServerController(const ServerController &) = delete;
    ServerController &operator=(const ServerController &) = delete;
    ServerController(ServerController &&) = default;
    ServerController &operator=(ServerController &&) = default;

    bool isValid() const;

    bool sendJson(const nlohmann::json &obj) override;
    bool receiveJson(nlohmann::json &out) override;
    
    void start() override;
    void stop() override;
    
    std::string getHandle() const override { return "bash"; }

private:
    Utils::TCPSocket *socket_;
    mutable std::mutex mtx_;
    
    ServerManager *manager_ = nullptr;
    std::string deviceName_;

    
    std::unique_ptr<std::thread> pingThread_;
    std::unique_ptr<std::thread> recvThread_;
    std::atomic<bool> running_ = false;
};

} 
