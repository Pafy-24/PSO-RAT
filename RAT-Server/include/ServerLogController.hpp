#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include "IController.hpp"

namespace Server {

class ServerManager;

class ServerLogController : public IController {
public:
    explicit ServerLogController(ServerManager *manager);
    ~ServerLogController();

    
    void start() override;
    
    void stop() override;
    
    std::string showLogs(const std::string &pathToLogFile);
    void tailLogs(const std::string &pathToLogFile, int numLines = 50);
    
    void handle(const nlohmann::json &packet) override;
    bool handleJson(const nlohmann::json &params) override;
    
    std::string getHandle() const override { return "log"; }

private:
    ServerManager *manager_;
    bool running_ = false;
    std::unique_ptr<std::thread> thread_;
};

} 
