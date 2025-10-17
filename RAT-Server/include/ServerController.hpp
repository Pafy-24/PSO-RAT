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
    // allow manager/device name to be set for controllers that are owned by ServerManager
    explicit ServerController(ServerManager *manager, const std::string &deviceName, Utils::TCPSocket *socket);
    ~ServerController();

    ServerController(const ServerController &) = delete;
    ServerController &operator=(const ServerController &) = delete;
    ServerController(ServerController &&) = default;
    ServerController &operator=(ServerController &&) = default;

    bool isValid() const;

    bool sendJson(const nlohmann::json &obj) override;
    bool receiveJson(nlohmann::json &out) override;
    // lifecycle for per-client controller: start background threads
    void start() override;
    void stop() override;

private:
    Utils::TCPSocket *socket_;
    mutable std::mutex mtx_;
    // manager and device name for server-owned controllers
    ServerManager *manager_ = nullptr;
    std::string deviceName_;

    // background threads for keepalive ping and receive loop
    std::unique_ptr<std::thread> pingThread_;
    std::unique_ptr<std::thread> recvThread_;
    std::atomic<bool> running_ = false;
};

} // namespace Server
