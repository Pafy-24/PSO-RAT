#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace Server {

class ServerManager;

class IController {
public:
    explicit IController(ServerManager * = nullptr) {}
    virtual ~IController() = default;

    // lifecycle
    virtual void start() {}
    virtual void stop() {}

    // optional JSON send/receive for client controllers
    virtual bool sendJson(const nlohmann::json &) { return false; }
    virtual bool receiveJson(nlohmann::json &) { return false; }
    // handle a JSON command directed to this controller (from another controller or from client)
    // Return true if handled.
    virtual bool handleJson(const nlohmann::json & /*params*/) { return false; }
};

} // namespace Server
