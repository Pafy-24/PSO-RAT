#include "ServerManager.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <fstream>

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
ServerManager::~ServerManager() {
    std::lock_guard<std::mutex> lock(mtx_);
    stop();
    // close and clear clients and controllers
        for (auto &p : clients_) if (p.second) p.second->close();
        clients_.clear();
        // stop all controllers (auxiliary and client controllers)
        for (auto &p : controllers_) if (p.second) p.second->stop();
        controllers_.clear();
}

bool ServerManager::start(unsigned short port) {
    // acquire the lock to protect running_ and listener_ during start
    std::lock_guard<std::mutex> lock(mtx_);
    if (running_) return false;
    listener_ = std::make_unique<Utils::TCPSocket>();
    if (!listener_->bind(port)) {
        listener_.reset();
        return false;
    }
    port_ = port;
    running_ = true;
    // create controllers
        controllers_.emplace("log", std::make_unique<ServerLogController>(this));
        controllers_.emplace("command", std::make_unique<ServerCommandController>(this));
        if (auto it = controllers_.find("log"); it != controllers_.end()) it->second->start();
        if (auto it = controllers_.find("command"); it != controllers_.end()) it->second->start();
    // attempt to spawn two terminals (logs + admin) for convenience
    spawnTerminalsForAdminAndLogs();
    return true;
}

// Try to open two terminal windows: one tails the log file, another connects via nc to admin port
void ServerManager::spawnTerminalsForAdminAndLogs() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!running_) return;
    if (logPath_.empty()) logPath_ = "/tmp/rat_server.log";
    unsigned short adminPort = port_ + 1;

    // terminal candidates and command templates
    const std::vector<std::pair<std::string, std::string>> emulators = {
        {"gnome-terminal", "gnome-terminal -- bash -lc '%s; exec bash'"},
        {"konsole", "konsole -e bash -lc '%s; exec bash'"},
        {"xfce4-terminal", "xfce4-terminal -e bash -lc '%s; exec bash'"},
        {"xterm", "xterm -e bash -lc '%s; exec bash'"},
        {"alacritty", "alacritty -e bash -lc '%s; exec bash'"},
        {"kitty", "kitty -- bash -lc '%s; exec bash'"}
    };

    auto findEmulator = [&]() -> std::pair<std::string,std::string> {
        for (auto &p : emulators) {
            std::string which = std::string("which ") + p.first + " >/dev/null 2>&1";
            int rc = std::system(which.c_str());
            if (rc == 0) return p; // return {name, template}
        }
        return {"", ""};
    };

    // check for graphical display (X11 or Wayland)
    const char *displayEnv = std::getenv("DISPLAY");
    const char *waylandEnv = std::getenv("WAYLAND_DISPLAY");
    bool hasDisplay = (displayEnv != nullptr) || (waylandEnv != nullptr);

    auto found = findEmulator();
    std::string templateCmd = found.second;
    std::string emulatorName = found.first;
    if (templateCmd.empty() || !hasDisplay) {
        // Try tmux fallback in headless or terminal-only environments
        if (std::system("which tmux >/dev/null 2>&1") == 0) {
            // create a detached tmux session named rat_server
            std::ostringstream s1, s2, s3;
            s1 << "tmux new-session -d -s rat_server 'tail -f " << logPath_ << "'";
            s2 << "tmux split-window -h -t rat_server 'sleep 0.5; nc localhost " << adminPort << "'";
            s3 << "tmux set-option -t rat_server remain-on-exit on";
            int r1 = std::system(s1.str().c_str());
            int r2 = std::system(s2.str().c_str());
            int r3 = std::system(s3.str().c_str());
            if (r1 == 0 && r2 == 0) {
                pushLog("spawnTerminals: created tmux session 'rat_server' with logs and admin panes");
            } else {
                pushLog("spawnTerminals: failed to create tmux session");
            }
            return;
        }
        pushLog("spawnTerminals: no terminal emulator found on PATH and no tmux available");
        return; // nothing available
    }

    // server command: tail -f the log file
    std::ostringstream tailCmd;
    tailCmd << "tail -f " << logPath_;

    // admin command: nc localhost adminPort
    std::ostringstream adminCmd;
    adminCmd << "bash -lc 'sleep 0.5; echo Connecting to admin port " << adminPort << "; nc localhost " << adminPort << "; exec bash'";

    // format and run (use setsid to detach)
    auto runCmd = [&](const std::string &inner) {
        std::string formatted;
        // replace %s in templateCmd with inner
        size_t pos = templateCmd.find("%s");
        if (pos != std::string::npos) {
            formatted = templateCmd.substr(0,pos) + inner + templateCmd.substr(pos+2);
        } else {
            formatted = templateCmd + " " + inner;
        }
        // final command: setsid sh -c '<formatted>' >/dev/null 2>&1 &
        std::string final = "setsid sh -c '" + formatted + "' >/dev/null 2>&1 &";
        int rc = std::system(final.c_str());
        if (rc != 0) {
            pushLog(std::string("spawnTerminals: failed to run: ") + emulatorName + " (rc=" + std::to_string(rc) + ")");
        } else {
            pushLog(std::string("spawnTerminals: launched ") + emulatorName);
        }
    };

    runCmd(tailCmd.str());
    runCmd(adminCmd.str());
}

void ServerManager::stop() {
    // acquire the lock to protect running_ and listener_ during stop
    std::lock_guard<std::mutex> lock(mtx_);
    if (!running_) return;
    if (listener_) listener_->close();
    listener_.reset();
    port_ = 0;
    running_ = false;
}

bool ServerManager::isRunning() const {
    // acquire the lock when reading running_ to ensure thread-safe access
    std::lock_guard<std::mutex> lock(mtx_);
    return running_;
}

int ServerManager::run(unsigned short port) {
    if (!start(port)) return 1;
    // Accept clients until stop() is called
    int clientCount = 0;
    while (isRunning()) {
        auto client = listener_->accept();
        if (!client) {
            // accept failed; small sleep then continue to check running_
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        // assign a device name based on a counter
        const std::string device = "client" + std::to_string(++clientCount);
        Utils::TCPSocket *clientPtr = client.get();
        {
            std::lock_guard<std::mutex> lock(mtx_);
            clients_[device] = std::move(client);
            // create controller bound to manager and device name and start it
            controllers_[device] = std::make_unique<ServerController>(this, device, clientPtr);
            if (auto it = controllers_.find(device); it != controllers_.end()) it->second->start();
        }
    }
    // stopped
    return 0;
}

bool ServerManager::addClient(const std::string &deviceName, std::unique_ptr<Utils::TCPSocket> socket) {
    // acquire lock to protect clients_ map mutation
    std::lock_guard<std::mutex> lock(mtx_);
    if (!socket) return false;
    if (clients_.find(deviceName) != clients_.end()) return false; // already exists
    Utils::TCPSocket *sockPtr = socket.get();
    clients_[deviceName] = std::move(socket);
    // create polymorphic controller that references the socket and start it
    controllers_[deviceName] = std::make_unique<ServerController>(this, deviceName, sockPtr);
    if (auto it = controllers_.find(deviceName); it != controllers_.end()) it->second->start();
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
    controllers_.erase(deviceName);
    return true;
}

Utils::TCPSocket *ServerManager::getClient(const std::string &deviceName) {
    // acquire lock for safe access to clients_ map
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = clients_.find(deviceName);
    if (it == clients_.end()) return nullptr;
    return it->second.get(); // non-owning raw pointer
}

ServerController *ServerManager::getController(const std::string &deviceName) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = controllers_.find(deviceName);
    if (it == controllers_.end()) return nullptr;
    // dynamic cast to ServerController
    return dynamic_cast<ServerController *>(it->second.get());
}

ServerController *ServerManager::getServerController(const std::string &deviceName) {
    return getController(deviceName);
}

void ServerManager::pushLog(const std::string &msg) {
    std::lock_guard<std::mutex> lock(logMtx_);
    logs_.push(msg);
    // also append to log file for external tailing
    if (logPath_.empty()) logPath_ = "/tmp/rat_server.log";
    try {
        std::ofstream ofs(logPath_, std::ios::app);
        if (ofs) ofs << msg << std::endl;
    } catch (...) {}
}

bool ServerManager::popLog(std::string &out) {
    std::lock_guard<std::mutex> lock(logMtx_);
    if (logs_.empty()) return false;
    out = std::move(logs_.front());
    logs_.pop();
    return true;
}

std::vector<std::string> ServerManager::listClients() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> names;
    for (auto &p : clients_) names.push_back(p.first);
    return names;
}

bool ServerManager::hasClient(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx_);
    return clients_.find(name) != clients_.end();
}

} // namespace Server
