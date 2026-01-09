#include "Utils/TCPSocket.hpp"
#include <SFML/Network/Packet.hpp>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

namespace Utils {

TCPSocket::TCPSocket() {
    socket.setBlocking(true);
}

TCPSocket::~TCPSocket() {}

bool TCPSocket::connect(const sf::IpAddress &address, unsigned short port) {
    return socket.connect(address, port) == sf::Socket::Status::Done;
}

bool TCPSocket::send(const std::string &message) {
    sf::Packet packet;
    packet << message;
    return socket.send(packet) == sf::Socket::Status::Done;
}

bool TCPSocket::receive(std::string &out) {
    sf::Packet packet;
    if (socket.receive(packet) != sf::Socket::Status::Done) return false;
    packet >> out;
    return true;
}

bool TCPSocket::listen(unsigned short port) {
    sf::Socket::Status status = listener.listen(port);
    return status == sf::Socket::Status::Done;
}

std::unique_ptr<TCPSocket> TCPSocket::accept() {
    std::unique_ptr<TCPSocket> client = std::make_unique<TCPSocket>();
    sf::Socket::Status status = listener.accept(client->socket);
    if (status == sf::Socket::Status::Done) {
        return client;
    }
    return nullptr;
}

bool TCPSocket::bind(unsigned short port) {
    if (!listen(port)) return false;
    listener.setBlocking(false);  
    return true;
}

void TCPSocket::close() {
    socket.disconnect();
    listener.close();
}

void TCPSocket::setBlocking(bool blocking) {
    socket.setBlocking(blocking);
}

std::string TCPSocket::getRemoteAddress() const {
    auto addr = socket.getRemoteAddress();
    if (addr.has_value()) {
        return addr.value().toString();
    }
    return "unknown";
}

} 
