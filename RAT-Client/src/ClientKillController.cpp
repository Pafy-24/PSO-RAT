#include "ClientKillController.hpp"
#include <cstdlib>

namespace Client {

nlohmann::json ClientKillController::handleCommand(const nlohmann::json &request) {
    (void)request;
    nlohmann::json reply;
    reply["controller"] = "kill";
    reply["out"] = "Client terminating";
    reply["ret"] = 0;
    reply["exit"] = true;
    return reply;
}

}
