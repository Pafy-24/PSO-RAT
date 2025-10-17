#include "ServerLogController.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <queue>

namespace Server {

ServerLogController::ServerLogController(ServerManager *manager) : ServerController(nullptr), manager_(manager) {}
ServerLogController::~ServerLogController() { stop(); }

void ServerLogController::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            // pop logs from manager and print them
            std::string log;
            while (manager_->popLog(log)) {
                std::cout << "[LOG] " << log << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
}

void ServerLogController::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_ && thread_->joinable()) thread_->join();
}

} // namespace Server
