#include "ServerController.hpp"
#include "IController.hpp"
#include "ServerManager.hpp"
#include "ServerFileController.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>

using json = nlohmann::json;

namespace Server {

ServerController::ServerController(Utils::TCPSocket *socket) : socket_(socket) {}
ServerController::ServerController(ServerManager *manager, const std::string &deviceName, Utils::TCPSocket *socket)
    : IController(manager), socket_(socket), manager_(manager), deviceName_(deviceName) {}

ServerController::~ServerController() { stop(); }

bool ServerController::isValid() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return socket_ != nullptr;
}

bool ServerController::sendJson(const json &obj) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!socket_) return false;
    try {
        std::string s = obj.dump();
        if (s.size() > 100 * 1024 * 1024) {
            if (manager_) manager_->pushLog(deviceName_ + ": outgoing message too large");
            return false;
        }
        return socket_->send(s);
    } catch (const std::exception &e) {
        if (manager_) manager_->pushLog(deviceName_ + ": JSON dump error: " + std::string(e.what()));
        return false;
    } catch (...) {
        if (manager_) manager_->pushLog(deviceName_ + ": unknown JSON dump error");
        return false;
    }
}

bool ServerController::receiveJson(json &out) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!socket_) return false;
    std::string s;
    if (!socket_->receive(s)) return false;
    if (s.empty()) return false;
    if (s.size() > 100 * 1024 * 1024) {
        if (manager_) manager_->pushLog(deviceName_ + ": message too large: " + std::to_string(s.size()));
        return false;
    }
    try {
        out = json::parse(s);
    } catch (const std::exception &e) {
        if (manager_) manager_->pushLog(deviceName_ + ": JSON parse error: " + std::string(e.what()));
        return false;
    } catch (...) {
        if (manager_) manager_->pushLog(deviceName_ + ": unknown JSON parse error");
        return false;
    }
    return true;
}

void ServerController::handle(const nlohmann::json &packet) {
    // ServerController handles client-to-server communication
    // Individual packet processing is done in recvThread_ routing
    (void)packet;
}

void ServerController::start() {
    if (running_) return;
    running_ = true;
    if (socket_) {
        socket_->setBlocking(false);
    } else {
        running_ = false;
        return;
    }

    
    pingThread_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            json j;
            j["controller"] = "ping";
            j["type"] = "ping";
            j["ts"] = (long)std::chrono::system_clock::now().time_since_epoch().count();
            if (!sendJson(j)) {
                if (manager_) {
                    manager_->pushLog(deviceName_ + ": failed to send ping");
                }
            }
            for (int i = 0; i < 300 && running_; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });

    
    recvThread_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            json in;
            if (receiveJson(in)) {
                if (manager_) {
                    // Log received message
                    try {
                        std::string prefix = deviceName_;
                        if (in.contains("controller") && in["controller"].is_string()) {
                            prefix += " [" + in["controller"].get<std::string>() + "]";
                        }
                        manager_->pushLog(prefix + ": " + in.dump());
                    } catch (...) {
                        manager_->pushLog(deviceName_ + ": [invalid JSON]");
                    }
                    
                    // Route to appropriate controller
                    if (in.contains("controller") && in["controller"].is_string()) {
                        std::string ctrlHandle = in["controller"].get<std::string>();
                        in["clientName"] = deviceName_;
                        
                        IController* handler = manager_->getControllerByHandle(ctrlHandle);
                        if (handler) {
                            handler->handle(in);  // Route to controller (bash, file, etc)
                        }
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
    });
}

void ServerController::stop() {
    if (!running_) return;
    running_ = false;
    if (pingThread_ && pingThread_->joinable()) pingThread_->join();
    if (recvThread_ && recvThread_->joinable()) recvThread_->join();
    if (socket_) {
        socket_->setBlocking(true);
    }
}

} 
