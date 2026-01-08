#pragma once

#include "IClientController.hpp"
#include <string>

namespace Client {

class ClientBashController : public IClientController {
public:
    ClientBashController() = default;
    ~ClientBashController() override = default;
    
    nlohmann::json handleCommand(const nlohmann::json &request) override;
    std::string getHandle() const override { return "bash"; }
};

}
