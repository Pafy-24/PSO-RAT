#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace Server {

class ServerManager;

class IController {
public:
    explicit IController(ServerManager * = nullptr) {}
    virtual ~IController() = default;

    
    virtual void start() {}
    virtual void stop() {}

    
    virtual bool sendJson(const nlohmann::json &) { return false; }
    virtual bool receiveJson(nlohmann::json &) { return false; }
    
    virtual bool handleJson(const nlohmann::json & ) { return false; }
    
    virtual std::string getHandle() const { return "unknown"; }
};

} 
