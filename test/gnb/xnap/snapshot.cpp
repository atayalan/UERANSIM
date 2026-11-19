//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <cstdint>
#include <vector>

#include <ext/doctest/doctest.h>
#include <gnb/xnap/snapshot.hpp>

namespace
{

nr::gnb::XnHandoverNgapSnapshot makeSnapshot(uint64_t sti)
{
    nr::gnb::XnHandoverNgapSnapshot snapshot{};
    snapshot.sti = sti;
    snapshot.ueAmbr.dlAmbr = 1000;
    snapshot.ueAmbr.ulAmbr = 2000;
    snapshot.securityCapabilities.valid = true;
    snapshot.securityCapabilities.nrEncryptionAlgorithms = 0xE000;

    nr::gnb::XnHandoverPduSessionSnapshot ipv4{};
    ipv4.psi = 1;
    ipv4.sessionType = PduSessionType::IPv4;
    ipv4.upTunnel.teid = 0x11223344;
    ipv4.upTunnel.address = OctetString::FromHex("c0a81e0a");
    ipv4.qosFlowIds = {1};

    nr::gnb::XnHandoverPduSessionSnapshot ethernet{};
    ethernet.psi = 2;
    ethernet.sessionType = PduSessionType::ETHERNET;
    ethernet.upTunnel.teid = 0x55667788;
    ethernet.upTunnel.address = OctetString::FromHex("c0a81e0b");
    ethernet.qosFlowIds = {1, 5};

    snapshot.pduSessions.push_back(std::move(ipv4));
    snapshot.pduSessions.push_back(std::move(ethernet));
    return snapshot;
}

} // namespace

TEST_CASE("gnb/xnap/snapshot: the UE's RLS identity survives the round trip")
{
    auto bytes = nr::gnb::SerializeHandoverSnapshot(makeSnapshot(0x0123456789ABCDEFull));
    REQUIRE(bytes.has_value());

    auto decoded = nr::gnb::DeserializeHandoverSnapshot(*bytes);
    REQUIRE(decoded != nullptr);
    CHECK(decoded->sti == 0x0123456789ABCDEFull);
}

TEST_CASE("gnb/xnap/snapshot: PDU sessions of both types survive the round trip")
{
    auto bytes = nr::gnb::SerializeHandoverSnapshot(makeSnapshot(7));
    REQUIRE(bytes.has_value());

    auto decoded = nr::gnb::DeserializeHandoverSnapshot(*bytes);
    REQUIRE(decoded != nullptr);
    REQUIRE(decoded->pduSessions.size() == 2);
    CHECK(decoded->pduSessions[0].psi == 1);
    CHECK(decoded->pduSessions[0].sessionType == PduSessionType::IPv4);
    CHECK(decoded->pduSessions[1].psi == 2);
    CHECK(decoded->pduSessions[1].sessionType == PduSessionType::ETHERNET);
    CHECK(decoded->pduSessions[1].upTunnel.teid == 0x55667788);
    CHECK(decoded->pduSessions[1].qosFlowIds == std::vector<int>{1, 5});
    CHECK(decoded->ueAmbr.ulAmbr == 2000);
    CHECK(decoded->securityCapabilities.valid);
}

TEST_CASE("gnb/xnap/snapshot: an unknown identity is carried as zero and not invented")
{
    auto bytes = nr::gnb::SerializeHandoverSnapshot(makeSnapshot(0));
    REQUIRE(bytes.has_value());

    auto decoded = nr::gnb::DeserializeHandoverSnapshot(*bytes);
    REQUIRE(decoded != nullptr);
    CHECK(decoded->sti == 0);
}

TEST_CASE("gnb/xnap/snapshot: a truncated or extended snapshot is rejected")
{
    auto bytes = nr::gnb::SerializeHandoverSnapshot(makeSnapshot(7));
    REQUIRE(bytes.has_value());

    auto truncated = *bytes;
    truncated.pop_back();
    CHECK(nr::gnb::DeserializeHandoverSnapshot(truncated) == nullptr);

    auto extended = *bytes;
    extended.push_back(0);
    CHECK(nr::gnb::DeserializeHandoverSnapshot(extended) == nullptr);
}

TEST_CASE("gnb/xnap/snapshot: a snapshot of the older layout without the identity is rejected")
{
    auto bytes = nr::gnb::SerializeHandoverSnapshot(makeSnapshot(7));
    REQUIRE(bytes.has_value());

    auto older = *bytes;
    older[4] = 1; // version byte after the 4-byte magic
    CHECK(nr::gnb::DeserializeHandoverSnapshot(older) == nullptr);
}
