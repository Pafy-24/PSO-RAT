#include "Utils/UDPSocket.hpp"
#include <SFML/Network/Packet.hpp>
#include <optional>

namespace Utils {

UDPSocket::UDPSocket() {
    socket.setBlocking(true);
}

UDPSocket::~UDPSocket() {}

bool UDPSocket::bind(unsigned short port) {
    return socket.bind(port) == sf::Socket::Status::Done;
}

bool UDPSocket::sendTo(const std::string &message, const sf::IpAddress &address, unsigned short port) {
    sf::Packet packet;
    packet << message;
    return socket.send(packet, address, port) == sf::Socket::Status::Done;
}

bool UDPSocket::receive(std::string &out, sf::IpAddress &sender, unsigned short &port) {
    sf::Packet packet;
    std::optional<sf::IpAddress> remoteAddress;
    if (socket.receive(packet, remoteAddress, port) != sf::Socket::Status::Done) return false;
    if (remoteAddress.has_value()) sender = *remoteAddress;
    packet >> out;
    return true;
}

void UDPSocket::close() {
    socket.unbind();
}

void UDPSocket::setBlocking(bool blocking) {
    socket.setBlocking(blocking);
}

} 
