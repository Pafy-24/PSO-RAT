#pragma once

#include "IClientController.hpp"
#include <string>

namespace Client {

class ClientKillController : public IClientController {
public:
    ClientKillController() = default;
    ~ClientKillController() override = default;
    
    nlohmann::json handleCommand(const nlohmann::json &request) override;
    std::string getHandle() const override { return "kill"; }
};

}
