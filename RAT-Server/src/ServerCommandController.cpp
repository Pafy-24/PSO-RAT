#include "ServerCommandController.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <sstream>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/TcpListener.hpp>

namespace Server {

ServerCommandController::ServerCommandController(ServerManager *manager) : ServerController(nullptr), manager_(manager) {}
ServerCommandController::~ServerCommandController() { stop(); }

void ServerCommandController::start() {
    if (running_) return;
    running_ = true;
    // listen for an admin TCP connection on server port + 1
    thread_ = std::make_unique<std::thread>([this]() {
        unsigned short adminPort = 0;
        if (manager_) adminPort = manager_->port() + 1;
        if (adminPort == 1) {
            running_ = false;
            return;
        }
        sf::TcpListener listener;
    if (listener.listen(adminPort) != sf::Socket::Status::Done) {
            // failed to open admin listener
            running_ = false;
            return;
        }
        // accept a single admin connection
        sf::TcpSocket adminSocket;
    if (listener.accept(adminSocket) != sf::Socket::Status::Done) {
            running_ = false;
            return;
        }

        // Use adminSocket as input/output stream
        while (running_) {
            // read a line
            char buf[1024] = {0};
            std::size_t received = 0;
            sf::Socket::Status st = adminSocket.receive(buf, sizeof(buf)-1, received);
            if (st != sf::Socket::Status::Done || received == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            std::string line(buf, received);
            // trim CRLF
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;
            std::string reply;
            if (cmd == "list") {
                auto clients = manager_->listClients();
                for (auto &c : clients) reply += c + "\n";
            } else if (cmd == "choose") {
                std::string name; iss >> name;
                reply = manager_->hasClient(name) ? std::string("Chosen ") + name + "\n" : "No such client\n";
            } else if (cmd == "kill") {
                std::string name; iss >> name;
                if (manager_->removeClient(name)) reply = "Killed " + name + "\n";
                else reply = "No such client\n";
            } else if (cmd == "bash") {
                std::string name; iss >> name;
                auto ctrl = manager_->getController(name);
                if (!ctrl) { reply = "No such client\n"; }
                else {
                    reply = "Entering bash (non-interactive): sending command...\n";
                    // next token is the command (rest of line)
                    std::string rest;
                    std::getline(iss, rest);
                    if (!rest.empty() && rest.front() == ' ') rest.erase(0,1);
                    nlohmann::json out{ {"cmd", rest} };
                    ctrl->sendJson(out);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    // collect logs
                    std::string lg;
                    while (manager_->popLog(lg)) reply += lg + "\n";
                }
            } else if (cmd == "quit") {
                running_ = false;
                manager_->stop();
                reply = "Quitting\n";
            } else {
                reply = "Unknown command\n";
            }
            if (!reply.empty()) {
                adminSocket.send(reply.c_str(), reply.size());
            }
        }
    });
}

void ServerCommandController::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_ && thread_->joinable()) thread_->join();
}

} // namespace Server
