#include "ClientManager.hpp"

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
ClientManager::~ClientManager() { disconnect(); }

bool ClientManager::connectTo(const sf::IpAddress &address, unsigned short port) {
    // acquire lock to protect socket_ and internal state modifications
    std::lock_guard<std::mutex> lock(mtx_);
    if (socket_) return false;
    socket_ = std::make_unique<Utils::TCPSocket>();
    if (!socket_->connect(address, port)) {
        socket_.reset();
        return false;
    }
    return true;
}

void ClientManager::disconnect() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (socket_) {
        socket_->close();
        socket_.reset();
    }
}

bool ClientManager::isConnected() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return socket_ != nullptr;
}

} // namespace Client
