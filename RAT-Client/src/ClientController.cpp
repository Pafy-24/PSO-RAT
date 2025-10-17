#include "ClientController.hpp"

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
    std::string s = obj.dump();
    return socket_->send(s);
}

bool ClientController::receiveJson(json &out) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!socket_) return false;
    std::string s;
    if (!socket_->receive(s)) return false;
    try {
        out = json::parse(s);
    } catch (...) {
        return false;
    }
    return true;
}

} // namespace Client
