#pragma once

#include "Utils/Socket.hpp"
#include <SFML/Network/UdpSocket.hpp>
#include <SFML/Network/IpAddress.hpp>
#include <string>

namespace Utils {

class UDPSocket : public Socket {
public:
    UDPSocket();
    ~UDPSocket();

    bool bind(unsigned short port);
    bool sendTo(const std::string &message, const sf::IpAddress &address, unsigned short port);
    bool receive(std::string &out, sf::IpAddress &sender, unsigned short &port);

    void setBlocking(bool blocking);
    void close() override;

private:
    sf::UdpSocket socket_;
};

} 
