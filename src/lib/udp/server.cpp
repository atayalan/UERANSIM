//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "server.hpp"

#include <cerrno>
#include <cstring>
#include <utils/common.hpp>
#include <utils/libc_error.hpp>

namespace udp
{

UdpServer::UdpServer() : UdpServer(Socket::CreateUdp6)
{
}

UdpServer::UdpServer(const std::function<Socket()> &createUdp6) : sockets{Socket::CreateUdp4()}
{
    try
    {
        sockets.push_back(createUdp6());
    }
    catch (const LibError &e)
    {
        // A host booted with ipv6.disable=1 fails with EAFNOSUPPORT; run IPv4-only there, since Send() looks up a
        // socket of the peer's IP version. Any other failure (out of fds or memory) is not a host without IPv6 and
        // must not silently drop IPv6, so it still fails here.
        if (e.error() == EAFNOSUPPORT)
            return;
        // Socket does not close itself and the destructor does not run for a constructor that throws.
        for (auto &s : sockets)
            s.close();
        throw;
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

bool UdpServer::HasIpv6() const
{
    for (const Socket &s : sockets)
        if (s.hasFd() && s.getIpVersion() == 6)
            return true;
    return false;
}

UdpServer::~UdpServer()
{
    for (auto &s : sockets)
        s.close();
}

} // namespace udp
