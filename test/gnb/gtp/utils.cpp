//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <ext/doctest/doctest.h>
#include <gnb/gtp/utils.hpp>

TEST_CASE("gnb/gtp/utils: a session is found by DL TEID and by UE and PSI until it is removed")
{
    nr::gnb::PduSessionTree tree;
    const auto session = nr::gnb::MakeSessionResInd(42, 7);

    tree.insert(session, 1001);
    CHECK_EQ(tree.findByDownTeid(1001), session);
    CHECK_EQ(tree.findBySessionId(42, 7), session);

    std::vector<uint64_t> sessions;
    tree.enumerateByUe(42, sessions);
    CHECK_EQ(sessions.size(), static_cast<size_t>(1));
    CHECK_EQ(sessions.front(), session);

    tree.remove(session, 1001);
    CHECK_EQ(tree.findByDownTeid(1001), static_cast<uint64_t>(0));
    CHECK_EQ(tree.findBySessionId(42, 7), static_cast<uint64_t>(0));
}

TEST_CASE("gnb/gtp/utils: an Ethernet session passes every uplink frame, an IPv4 session only IPv4 packets")
{
    // A destination MAC starting with 0x60 or 0x45 would look like IPv6 or IPv4 to a check of the first nibble.
    const OctetString ipv4 = OctetString::FromHex("4500001c000000004001");
    const OctetString ipv6 = OctetString::FromHex("6000000000083a40");
    const OctetString broadcastFrame = OctetString::FromHex("ffffffffffff0a0b0c0d0e0f0806");
    const OctetString ipv6LookingMac = OctetString::FromHex("600000000001aabbccddeeff0800");
    const OctetString empty{};

    CHECK(nr::gnb::IsUplinkPduAllowed(PduSessionType::IPv4, ipv4));
    CHECK_FALSE(nr::gnb::IsUplinkPduAllowed(PduSessionType::IPv4, ipv6));
    CHECK_FALSE(nr::gnb::IsUplinkPduAllowed(PduSessionType::IPv4, broadcastFrame));
    CHECK_FALSE(nr::gnb::IsUplinkPduAllowed(PduSessionType::IPv4, empty));

    CHECK(nr::gnb::IsUplinkPduAllowed(PduSessionType::ETHERNET, ipv4));
    CHECK(nr::gnb::IsUplinkPduAllowed(PduSessionType::ETHERNET, ipv6));
    CHECK(nr::gnb::IsUplinkPduAllowed(PduSessionType::ETHERNET, broadcastFrame));
    CHECK(nr::gnb::IsUplinkPduAllowed(PduSessionType::ETHERNET, ipv6LookingMac));
    CHECK_FALSE(nr::gnb::IsUplinkPduAllowed(PduSessionType::ETHERNET, empty));
}
