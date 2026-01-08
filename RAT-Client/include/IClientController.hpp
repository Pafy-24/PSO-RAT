#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace Client {

class IClientController {
public:
    virtual ~IClientController() = default;
    
    virtual nlohmann::json handleCommand(const nlohmann::json &request) = 0;
    virtual std::string getHandle() const = 0;
};

}
