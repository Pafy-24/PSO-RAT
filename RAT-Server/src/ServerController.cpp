#include "ServerController.hpp"
#include "IController.hpp"
#include "ServerManager.hpp"
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
    std::string s = obj.dump();
    return socket_->send(s);
}

bool ServerController::receiveJson(json &out) {
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

void ServerController::start() {
    if (running_) return;
    running_ = true;
    if (socket_) socket_->setBlocking(false);

    
    pingThread_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            json j;
            j["type"] = "ping";
            j["ts"] = (long)std::chrono::system_clock::now().time_since_epoch().count();
            sendJson(j);
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    });

    
    recvThread_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            json in;
            if (receiveJson(in)) {
                
                if (manager_ && in.contains("controller") && in["controller"].is_string()) {
                    std::string target = in["controller"].get<std::string>();
                    nlohmann::json params;
                    if (in.contains("params")) params = in["params"];
                    else params = in;
                    IController *c = nullptr;
                    
                    if (manager_) c = manager_->getServerController(target);
                    if (c) {
                        try {
                            if (!c->handleJson(params)) {
                                manager_->pushLog(deviceName_ + ": controller '" + target + "' did not handle json: " + params.dump());
                            }
                        } catch (...) {
                            manager_->pushLog(deviceName_ + ": exception while handling controller json");
                        }
                    } else {
                        manager_->pushLog(deviceName_ + ": no such controller '" + target + "' for message: " + in.dump());
                    }
                } else {
                    
                    if (manager_) {
                        try {
                            manager_->pushLog(deviceName_ + ": " + in.dump());
                        } catch (...) {}
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
    if (socket_) socket_->setBlocking(true);
}

} 
