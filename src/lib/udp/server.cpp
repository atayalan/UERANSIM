//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "server.hpp"

#include <cstring>
#include <utils/common.hpp>
#include <utils/libc_error.hpp>

namespace udp
{

UdpServer::UdpServer() : sockets{Socket::CreateUdp4()}
{
    // A host booted with ipv6.disable=1 cannot create AF_INET6 sockets; run IPv4-only there
    // instead of aborting, since Send() already looks up a socket of the peer's IP version.
    try
    {
        sockets.push_back(Socket::CreateUdp6());
    }
    catch (const LibError &)
    {
    }
}

UdpServer::UdpServer(const std::string &address, uint16_t port) : sockets{Socket::CreateAndBindUdp({address, port})}
{
}

int UdpServer::Receive(uint8_t *buffer, size_t bufferSize, int timeoutMs, InetAddress &outPeerAddress) const
{
    auto socket = Socket::Select(sockets, {}, timeoutMs);
    if (!socket.hasFd())
        return 0;
    return socket.receive(buffer, bufferSize, timeoutMs, outPeerAddress);
}

void UdpServer::Send(const InetAddress &address, const uint8_t *buffer, size_t bufferSize) const
{
    int version = address.getIpVersion();
    if (version != 4 && version != 6)
        throw std::runtime_error{"UdpServer::Send failure: Invalid IP version"};

    for (const Socket &s : sockets)
    {
        if (s.hasFd() && s.getIpVersion() == version)
        {
            s.send(address, buffer, bufferSize);
            return;
        }
    }

    throw std::runtime_error{"UdpServer::Send failure: No IP socket found"};
}

UdpServer::~UdpServer()
{
    for (auto &s : sockets)
        s.close();
}

} // namespace udp
