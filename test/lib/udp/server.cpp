//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <cerrno>

#include <ext/doctest/doctest.h>
#include <lib/udp/server.hpp>
#include <utils/libc_error.hpp>

static Socket FailUdp6(int err)
{
    throw LibError("Socket could not be created:", err);
}

TEST_CASE("lib/udp/server: a host without IPv6 runs IPv4-only")
{
    udp::UdpServer server{[] { return FailUdp6(EAFNOSUPPORT); }};
    CHECK_FALSE(server.HasIpv6());

    uint8_t byte = 0;
    CHECK_NOTHROW(server.Send(InetAddress{"127.0.0.1", 4997}, &byte, 1));
    CHECK_THROWS_WITH(server.Send(InetAddress{"::1", 4997}, &byte, 1), "UdpServer::Send failure: No IP socket found");
}

TEST_CASE("lib/udp/server: an IPv6 socket failure other than a missing address family is not swallowed")
{
    for (int err : {EMFILE, ENFILE, ENOBUFS, ENOMEM})
    {
        CAPTURE(err);
        try
        {
            udp::UdpServer server{[err] { return FailUdp6(err); }};
            FAIL("constructor did not throw");
        }
        catch (const LibError &e)
        {
            CHECK(e.error() == err);
        }
    }
}
