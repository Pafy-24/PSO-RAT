#include "ServerManager.hpp"

namespace Server {


std::shared_ptr<ServerManager> ServerManager::getInstance() {
    if (!ServerManager::instance) {
        // lock the initMutex to ensure only one thread initializes the instance
        // this is the "lock" step in double-checked locking
        std::lock_guard<std::mutex> lock(ServerManager::initMutex);
        // check again while holding the lock to avoid race with other threads
        if (!ServerManager::instance) ServerManager::instance.reset(new ServerManager());
    }
    return ServerManager::instance;
}

ServerManager::ServerManager() = default;
ServerManager::~ServerManager() { stop(); }

bool ServerManager::start(unsigned short port) {
    // acquire the lock to protect running_ and listener_ during start
    std::lock_guard<std::mutex> lock(mtx_);
    if (running_) return false;
    listener_ = std::make_unique<Utils::TCPSocket>();
    if (!listener_->bind(port)) {
        listener_.reset();
        return false;
    }
    running_ = true;
    return true;
}

void ServerManager::stop() {
    // acquire the lock to protect running_ and listener_ during stop
    std::lock_guard<std::mutex> lock(mtx_);
    if (!running_) return;
    if (listener_) listener_->close();
    listener_.reset();
    running_ = false;
}

bool ServerManager::isRunning() const {
    // acquire the lock when reading running_ to ensure thread-safe access
    std::lock_guard<std::mutex> lock(mtx_);
    return running_;
}

bool ServerManager::addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket) {
    // acquire lock to protect clients_ map mutation
    std::lock_guard<std::mutex> lock(mtx_);
    if (!socket) return false;
    if (clients_.find(deviceName) != clients_.end()) return false; // already exists
    clients_[deviceName] = std::move(socket);
    return true;
}

bool ServerManager::removeClient(const std::string &deviceName) {
    // acquire lock to protect clients_ map mutation
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clients_.find(deviceName);
    if (it == clients_.end()) return false;
    // close and erase
    if (it->second) it->second->close();
    clients_.erase(it);
    return true;
}

Utils::TCPSocket *ServerManager::getClient(const std::string &deviceName) {
    // acquire lock for safe access to clients_ map
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clients_.find(deviceName);
    if (it == clients_.end()) return nullptr;
    return it->second.get(); // non-owning raw pointer
}

} // namespace Server
