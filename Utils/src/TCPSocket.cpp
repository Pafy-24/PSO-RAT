#include "Utils/TCPSocket.hpp"
#include <SFML/Network/Packet.hpp>

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
    return listener_.listen(port) == sf::Socket::Status::Done;
}

std::unique_ptr<TCPSocket> TCPSocket::accept() {
    std::unique_ptr<TCPSocket> client = std::make_unique<TCPSocket>();
    if (listener_.accept(client->socket_) != sf::Socket::Status::Done) return nullptr;
    return client;
}

bool TCPSocket::bind(unsigned short port) {
    // Bind for TCP is equivalent to listening on the port for incoming connections
    return listen(port);
}

void TCPSocket::close() {
    socket_.disconnect();
    // listener_ has no explicit close in SFML 3, but can be left for destructor
}

} // namespace Utils
