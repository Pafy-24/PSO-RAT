#pragma once

#include "ServerController.hpp"
#include <string>
#include <memory>
#include <thread>
#include <sys/types.h>

namespace Server {

class ServerManager;

class ServerCommandController : public ServerController {
public:
    explicit ServerCommandController(ServerManager *manager);
    ~ServerCommandController();

    
    void start() override;
    void stop() override;

private:
    ServerManager *manager_;
    bool running_ = false;
    std::unique_ptr<std::thread> stdinThread_;

    // helper methods
    pid_t spawnTerminal(const std::string &innerCmd);
    std::string processLine(const std::string &line);
    // optional stdin thread when running interactively
    void stdinLoop();

    // per-command handlers
    std::string handleList();
    std::string handleChoose(const std::string &name);
    std::string handleKill(const std::string &name);
    std::string handleBash(const std::string &name, const std::string &cmd);
    std::string handleQuit();
    std::string handleShowLogs();
};

} 
