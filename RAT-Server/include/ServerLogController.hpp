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

    // start log thread
    void start() override;
    // stop log thread
    void stop() override;

private:
    ServerManager *manager_;
    bool running_ = false;
    std::unique_ptr<std::thread> thread_;
};

} // namespace Server
