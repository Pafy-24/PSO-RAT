#pragma once

#include "Utils/Socket.hpp"
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/IpAddress.hpp>
#include <string>
#include <memory>

namespace Utils {

class TCPSocket : public Socket {
public:
    TCPSocket();
    ~TCPSocket();

    bool connect(const sf::IpAddress &address, unsigned short port);
    bool send(const std::string &message);
    bool receive(std::string &out);

    
    void setBlocking(bool blocking);

    
    bool listen(unsigned short port);
    
    bool bind(unsigned short port) override;
    std::unique_ptr<TCPSocket> accept();

    void close() override;

private:
    sf::TcpSocket socket_;
    sf::TcpListener listener_;
};

} 
