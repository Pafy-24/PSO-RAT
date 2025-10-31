#pragma once

#include <string>
#include <memory>
#include <thread>

#include <atomic>
#include <chrono>
#include "ServerController.hpp"

namespace Server {

class ServerManager;

class ServerLogController : public ServerController {
public:
    explicit ServerLogController(ServerManager *manager);
    ~ServerLogController();

    
    void start() override;
    
    void stop() override;
    
    
    
    
    std::string showLogs(const std::string &pathToLogFile);

    
    
    bool handleJson(const nlohmann::json &params) override;

private:
    ServerManager *manager_;
    bool running_ = false;
    std::unique_ptr<std::thread> thread_;
};

} 
