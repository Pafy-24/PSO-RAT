#pragma once

#include "ServerController.hpp"
#include <string>
#include <memory>
#include <thread>

namespace Server {

class ServerManager;

class ServerCommandController : public ServerController {
public:
    explicit ServerCommandController(ServerManager *manager);
    ~ServerCommandController();

    // start command input thread (reads commands from stdin)
    void start() override;
    void stop() override;

private:
    ServerManager *manager_;
    bool running_ = false;
    std::unique_ptr<std::thread> thread_;
    // thread for reading from stdin (interactive terminal)
    std::unique_ptr<std::thread> stdinThread_;
};

} // namespace Server
