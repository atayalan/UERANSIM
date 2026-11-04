//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include <functional>
#include <string>
#include <utils/network.hpp>

namespace udp
{

class UdpServer
{
  private:
    std::vector<Socket> sockets;

  public:
    UdpServer();
    // Takes the IPv6 socket factory so tests can simulate a host without IPv6.
    explicit UdpServer(const std::function<Socket()> &createUdp6);
    UdpServer(const std::string &address, uint16_t port);
    ~UdpServer();

    int Receive(uint8_t *buffer, size_t bufferSize, int timeoutMs, InetAddress &outPeerAddress) const;
    void Send(const InetAddress &address, const uint8_t *buffer, size_t bufferSize) const;
    [[nodiscard]] bool HasIpv6() const;
};

} // namespace udp
