#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <mutex>
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
    
    // Logging functionality
    void pushLog(const std::string &msg);
    bool popLog(std::string &out);
    const std::string &logPath() const { return logPath_; }

private:
    ServerManager *manager_;
    bool running_ = false;
    
    // Log queue and file path
    std::queue<std::string> logs_;
    mutable std::mutex logMtx_;
    std::string logPath_;
};

} 
