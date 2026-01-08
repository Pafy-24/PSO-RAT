#include "Utils/TCPSocket.hpp"
#include <SFML/Network/Packet.hpp>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

namespace Utils {

TCPSocket::TCPSocket() {
    socket_.setBlocking(true);
}

TCPSocket::~TCPSocket() {}

bool TCPSocket::connect(const sf::IpAddress &address, unsigned short port) {
    return socket_.connect(address, port) == sf::Socket::Status::Done;
}

bool TCPSocket::send(const std::string &message) {
    sf::Packet packet;
    packet << message;
    return socket_.send(packet) == sf::Socket::Status::Done;
}

bool TCPSocket::receive(std::string &out) {
    sf::Packet packet;
    if (socket_.receive(packet) != sf::Socket::Status::Done) return false;
    packet >> out;
    return true;
}

bool TCPSocket::listen(unsigned short port) {
    sf::Socket::Status status = listener_.listen(port);
    return status == sf::Socket::Status::Done;
}

std::unique_ptr<TCPSocket> TCPSocket::accept() {
    std::unique_ptr<TCPSocket> client = std::make_unique<TCPSocket>();
    sf::Socket::Status status = listener_.accept(client->socket_);
    if (status == sf::Socket::Status::Done) {
        return client;
    }
    return nullptr;
}

bool TCPSocket::bind(unsigned short port) {
    if (!listen(port)) return false;
    listener_.setBlocking(false);  // Non-blocking for accept()
    return true;
}

void TCPSocket::close() {
    socket_.disconnect();
    listener_.close();
}

void TCPSocket::setBlocking(bool blocking) {
    socket_.setBlocking(blocking);
}

std::string TCPSocket::getRemoteAddress() const {
    auto addr = socket_.getRemoteAddress();
    if (addr.has_value()) {
        return addr.value().toString();
    }
    return "unknown";
}

} 
