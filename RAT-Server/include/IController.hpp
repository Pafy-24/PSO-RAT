#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace Server {

class ServerManager;

class IController {
public:
    explicit IController(ServerManager * = nullptr) {}
    virtual ~IController() = default;

    // Lifecycle
    virtual void start() {}
    virtual void stop() {}

    // Packet handling - main interface
    virtual void handle(const nlohmann::json &packet) = 0;
    
    // Communication (for client-facing controllers)
    virtual bool sendJson(const nlohmann::json &) { return false; }
    virtual bool receiveJson(nlohmann::json &) { return false; }
    
    // Legacy compatibility
    virtual bool handleJson(const nlohmann::json &packet) { 
        handle(packet);
        return true;
    }
    
    // Identification
    virtual std::string getHandle() const { return "unknown"; }
};

} 
