#include "ClientManager.hpp"
#include <iostream>

namespace Client {

std::shared_ptr<ClientManager> ClientManager::getInstance() {
    if (!ClientManager::instance) {
        // lock the initMutex to ensure only one thread initializes the instance
        // this is the "lock" step in double-checked locking
        std::lock_guard<std::mutex> lock(ClientManager::initMutex);
        // check again while holding the lock to avoid race with other threads
        if (!ClientManager::instance) ClientManager::instance.reset(new ClientManager());
    }
    return ClientManager::instance;
}

ClientManager::ClientManager() = default;
ClientManager::~ClientManager() {
    std::lock_guard<std::mutex> lock(mtx_);
    // close and clear all sockets
    if (socket_) socket_->close();
    socket_.reset();
    controllers_.clear();
}

bool ClientManager::connectTo(const std::string &name, const sf::IpAddress &address, unsigned short port) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (socket_) return false; // already connected
    socket_ = std::make_shared<Utils::TCPSocket>();
    if (!socket_->connect(address, port)) {
        socket_.reset();
        return false;
    }
    // Create controller that holds a shared_ptr to the socket
    controllers_.emplace(std::piecewise_construct,
                         std::forward_as_tuple(name),
                         std::forward_as_tuple(socket_));
    return true;
}

void ClientManager::disconnect(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx_);
    // close the single socket and remove controller entry
    if (socket_) {
        socket_->close();
        socket_.reset();
    }
    controllers_.erase(name);
}

bool ClientManager::isConnected(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    return socket_ != nullptr && controllers_.find(name) != controllers_.end();
}

ClientController *ClientManager::getController(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = controllers_.find(name);
    if (it == controllers_.end()) return nullptr;
    return &it->second;
}

int ClientManager::run(const sf::IpAddress &address, unsigned short port) {
    const std::string name = "main";
    if (!connectTo(name, address, port)) return 1;
    ClientController *ctrl = getController(name);
    if (!ctrl) return 2;

    nlohmann::json out;
    out["msg"] = "Hello from RAT-Client";
    if (!ctrl->sendJson(out)) return 3;

    nlohmann::json in;
    if (!ctrl->receiveJson(in)) return 4;
    std::cout << "Server: " << in.dump() << std::endl;

    disconnect(name);
    return 0;
}

} // namespace Client
