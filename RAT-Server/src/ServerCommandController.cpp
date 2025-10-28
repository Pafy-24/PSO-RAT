#include "ServerCommandController.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <sstream>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>

namespace Server {

ServerCommandController::ServerCommandController(ServerManager *manager) : ServerController(nullptr), manager_(manager) {}
ServerCommandController::~ServerCommandController() { stop(); }

void ServerCommandController::start() {
    if (running_) return;
    running_ = true;
    // spawn a terminal emulator to run an inner command. Returns child pid > 0 on success,
    // 0 if command executed via system fallback (tmux), -1 on failure.
    auto spawnTerminal = [&](const std::string &innerCmd)->pid_t {
        // prefer gnome-terminal, then konsole, then xterm, then tmux
        auto which_ok = [](const char *name)->bool {
            std::string cmd = std::string("which ") + name + " >/dev/null 2>&1";
            return std::system(cmd.c_str()) == 0;
        };

        std::string prog;
        if (which_ok("gnome-terminal")) prog = "gnome-terminal";
        else if (which_ok("konsole")) prog = "konsole";
        else if (which_ok("xterm")) prog = "xterm";
        else if (which_ok("tmux")) prog = "tmux";
        else return -1;

        if (prog == "tmux") {
            // create detached tmux session
            std::string s = std::string("tmux new-session -d -s rat_server '") + innerCmd + "'";
            int rc = std::system(s.c_str());
            return rc == 0 ? 0 : -1;
        }

        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid == 0) {
            // child: execute the terminal program with appropriate args
            if (prog == "gnome-terminal") {
                // gnome-terminal -- bash -c '<inner>; exec bash'
                execlp("gnome-terminal", "gnome-terminal", "--", "bash", "-c", innerCmd.c_str(), (char *)NULL);
            } else if (prog == "konsole") {
                execlp("konsole", "konsole", "-e", "bash", "-c", innerCmd.c_str(), (char *)NULL);
            } else if (prog == "xterm") {
                execlp("xterm", "xterm", "-e", "bash", "-c", innerCmd.c_str(), (char *)NULL);
            }
            _exit(127);
        }
        // parent: return child pid
        return pid;
    };

    // helper to process a single command line and return a reply string
    auto processLine = [this, &spawnTerminal](const std::string &line)->std::string {
        std::istringstream iss(line);
        std::string cmd; iss >> cmd;
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
                std::string rest;
                std::getline(iss, rest);
                if (!rest.empty() && rest.front() == ' ') rest.erase(0,1);
                nlohmann::json out{ {"cmd", rest} };
                ctrl->sendJson(out);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::string lg;
                while (manager_->popLog(lg)) reply += lg + "\n";
            }
        } else if (cmd == "quit") {
            running_ = false;
            manager_->stop();
            reply = "Quitting\n";
        } else if (cmd == "showlogs") {
            // Launch a terminal that runs the log_script on the server log file
            ServerController *lc = manager_->getServerController("log");
            if (!lc) reply = "No log controller\n";
            else {
                auto *slc = dynamic_cast<ServerLogController *>(lc);
                if (!slc) reply = "Invalid log controller\n";
                else {
                    std::string path = manager_->logPath();
                    if (path.empty()) path = "/tmp/rat_server.log";
                    // build inner command to run log_script and keep terminal open
                    std::string inner = std::string("../build_make/log_script ") + path + std::string("; exec bash");
                    pid_t p = spawnTerminal(inner);
                    if (p == -1) reply = "No terminal emulator available\n";
                    else if (p == 0) reply = "Launched in tmux session\n";
                    else reply = std::string("Launched terminal pid=") + std::to_string(p) + "\n";
                }
            }
        } else {
            reply = "Unknown command\n";
        }
        return reply;
    };


    // listen for an admin TCP connection on server port + 1
    thread_ = std::make_unique<std::thread>([this, processLine]() {
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
            std::string reply = processLine(line);
            if (!reply.empty()) {
                adminSocket.send(reply.c_str(), reply.size());
            }
        }
    });
    // if stdin is a terminal, spawn a thread to read from it and process commands
    if (isatty(STDIN_FILENO)) {
        stdinThread_ = std::make_unique<std::thread>([this, processLine]() {
            std::string line;
            while (running_ && std::getline(std::cin, line)) {
                std::string reply = processLine(line);
                if (!reply.empty()) std::cout << reply << std::flush;
            }
        });
    }

}

void ServerCommandController::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_ && thread_->joinable()) thread_->join();
    if (stdinThread_ && stdinThread_->joinable()) stdinThread_->join();
}

} // namespace Server
