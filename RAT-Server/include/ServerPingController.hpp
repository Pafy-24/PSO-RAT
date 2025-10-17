#pragma once

#include "ServerController.hpp"
#include "Utils/UDPSocket.hpp"
#include <memory>

namespace Server {

class ServerPingController : public ServerController {
public:
    explicit ServerPingController(Utils::TCPSocket *sock);
    ~ServerPingController();

    // Listen for UDP pings on udpPort and respond with "pong".
    // Returns true if listener successfully started.
    bool startUdpResponder(unsigned short udpPort);

private:
    std::unique_ptr<Utils::UDPSocket> udpSocket_;
    bool running_ = false;
};

} // namespace Server
