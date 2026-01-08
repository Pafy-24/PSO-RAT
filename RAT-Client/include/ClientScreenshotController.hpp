#ifndef CLIENT_SCREENSHOT_CONTROLLER_HPP
#define CLIENT_SCREENSHOT_CONTROLLER_HPP

#include "IClientController.hpp"
#include <nlohmann/json.hpp>

namespace Client {

class ClientScreenshotController : public IClientController {
public:
    ClientScreenshotController() = default;
    ~ClientScreenshotController() override = default;
    
    std::string getHandle() const override { return "screenshot"; }
    nlohmann::json handleCommand(const nlohmann::json &cmd) override;
    
private:
    nlohmann::json takeScreenshot();
    std::string base64_encode(const std::string &input);
};

} // namespace Client

#endif
