#include "ClientController.hpp"
#include <iostream>

using json = nlohmann::json;

namespace Client {

ClientController::ClientController(std::shared_ptr<Utils::TCPSocket> socket) : socket_(std::move(socket)) {}
ClientController::~ClientController() = default;

bool ClientController::isValid() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return socket_ != nullptr;
}

bool ClientController::sendJson(const json &obj) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!socket_) return false;
    try {
        std::string s = obj.dump();
        if (s.size() > 100 * 1024 * 1024) return false;
        return socket_->send(s);
    } catch (...) {
        return false;
    }
}

bool ClientController::receiveJson(json &out) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!socket_) return false;
    std::string s;
    if (!socket_->receive(s)) return false;
    if (s.empty()) return false;
    if (s.size() > 100 * 1024 * 1024) return false;
    try {
        out = json::parse(s);
    } catch (...) {
        return false;
    }
    return true;
}

nlohmann::json ClientController::handleCommand(const nlohmann::json &request) {
    (void)request;
    nlohmann::json reply;
    reply["controller"] = "tcp";
    reply["out"] = "TCP controller is for internal use only";
    reply["ret"] = -1;
    return reply;
}

} 
