//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <ext/doctest/doctest.h>
#include <gnb/xnap/policy.hpp>

using nr::gnb::xnap::IsUsableForwardingAddress;
using nr::gnb::xnap::SameHostAddress;

TEST_CASE("SameHostAddress compares numeric addresses only")
{
    CHECK(SameHostAddress("192.168.10.3", "192.168.10.3"));
    CHECK_FALSE(SameHostAddress("192.168.10.3", "192.168.10.4"));
    CHECK(SameHostAddress("::ffff:192.168.10.3", "192.168.10.3"));
    CHECK(SameHostAddress("fd00::1", "FD00:0:0:0:0:0:0:1"));
    CHECK_FALSE(SameHostAddress("fd00::1", "fd00::2"));
    // Names are never resolved, so a name can not stand in for an address.
    CHECK_FALSE(SameHostAddress("localhost", "localhost"));
    CHECK_FALSE(SameHostAddress("", ""));
    CHECK_FALSE(SameHostAddress("192.168.10.3", "gnb2.example"));
    // An IPv6 address that merely ends in the IPv4 octets is not that IPv4 address, and ::1 is not 127.0.0.1.
    CHECK_FALSE(SameHostAddress("::c0a8:0a03", "192.168.10.3"));
    CHECK_FALSE(SameHostAddress("::1", "127.0.0.1"));
    // A scoped address is not a plain numeric address.
    CHECK_FALSE(SameHostAddress("fe80::1%eth0", "fe80::1%eth0"));
}

TEST_CASE("IsUsableForwardingAddress accepts only unicast IPv4")
{
    CHECK(IsUsableForwardingAddress(OctetString::FromHex("c0a81e03")));  // 192.168.30.3
    CHECK(IsUsableForwardingAddress(OctetString::FromHex("0a000001")));  // 10.0.0.1
    CHECK_FALSE(IsUsableForwardingAddress(OctetString::FromHex("00000000")));
    CHECK_FALSE(IsUsableForwardingAddress(OctetString::FromHex("7f000001")));
    CHECK_FALSE(IsUsableForwardingAddress(OctetString::FromHex("e0000001"))); // multicast
    CHECK_FALSE(IsUsableForwardingAddress(OctetString::FromHex("ffffffff"))); // broadcast
    CHECK_FALSE(IsUsableForwardingAddress(OctetString::FromHex("c0a81e")));   // too short
    CHECK_FALSE(IsUsableForwardingAddress(OctetString::FromHex("20010db8000000000000000000000001"))); // IPv6
    CHECK_FALSE(IsUsableForwardingAddress(OctetString{}));
}

using nr::gnb::XnForwardingEndpoint;
using nr::gnb::xnap::AckUsable;
using nr::gnb::xnap::CancelAccepted;
using nr::gnb::xnap::EHandoverState;
using nr::gnb::xnap::CancelTargetOnTimeout;
using nr::gnb::xnap::HandoverExpired;
using nr::gnb::xnap::HandoverTimedOut;
using nr::gnb::xnap::LateReleaseAccepted;
using nr::gnb::xnap::PreparationFailureAccepted;
using nr::gnb::xnap::ReleaseAccepted;
using nr::gnb::xnap::AbortCause;
using nr::gnb::xnap::PathSwitchAnswerAccepted;
using nr::gnb::xnap::SurvivesAssociationShutdown;
using nr::gnb::xnap::TargetRelease;
using nr::gnb::xnap::TargetReleaseOnAbort;

namespace
{

XnForwardingEndpoint MakeEndpoint(uint32_t teid, const char *addressHex)
{
    XnForwardingEndpoint endpoint{};
    endpoint.psi = 1;
    endpoint.tunnel.teid = teid;
    endpoint.tunnel.address = OctetString::FromHex(addressHex);
    endpoint.qosFlowIds = {1};
    return endpoint;
}

} // namespace

TEST_CASE("CancelAccepted undoes only a preparation")
{
    constexpr uint32_t OWN = 7;
    SUBCASE("before the Ack, an absent or matching target id is accepted")
    {
        CHECK(CancelAccepted(EHandoverState::PREPARING, 0, OWN));
        CHECK(CancelAccepted(EHandoverState::PREPARING, OWN, OWN));
        CHECK_FALSE(CancelAccepted(EHandoverState::PREPARING, OWN + 1, OWN));
        // No id was given by this gNB yet: only an absent id matches.
        CHECK(CancelAccepted(EHandoverState::PREPARING, 0, 0));
        CHECK_FALSE(CancelAccepted(EHandoverState::PREPARING, 5, 0));
    }
    SUBCASE("after the Ack the target id must match, and 0 is not a wildcard")
    {
        CHECK(CancelAccepted(EHandoverState::TARGET_PREPARED, OWN, OWN));
        CHECK_FALSE(CancelAccepted(EHandoverState::TARGET_PREPARED, 0, OWN));
        CHECK_FALSE(CancelAccepted(EHandoverState::TARGET_PREPARED, OWN + 1, OWN));
        CHECK_FALSE(CancelAccepted(EHandoverState::TARGET_PREPARED, 0, 0));
    }
    SUBCASE("once the UE reached the target, a Cancel is ignored")
    {
        CHECK_FALSE(CancelAccepted(EHandoverState::PATH_SWITCH_PENDING, OWN, OWN));
        CHECK_FALSE(CancelAccepted(EHandoverState::PATH_SWITCH_PENDING, 0, OWN));
        CHECK_FALSE(CancelAccepted(EHandoverState::COMMAND_SENT, OWN, OWN));
    }
}

TEST_CASE("ReleaseAccepted needs the outstanding command and the target id")
{
    CHECK(ReleaseAccepted(EHandoverState::COMMAND_SENT, 9, 9));
    CHECK_FALSE(ReleaseAccepted(EHandoverState::COMMAND_SENT, 8, 9));
    CHECK_FALSE(ReleaseAccepted(EHandoverState::COMMAND_SENT, 0, 9));
    // A target id of 0 was never given, so even a release naming 0 must not match it.
    CHECK_FALSE(ReleaseAccepted(EHandoverState::COMMAND_SENT, 0, 0));
    CHECK_FALSE(ReleaseAccepted(EHandoverState::PREPARING, 9, 9));
    CHECK_FALSE(ReleaseAccepted(EHandoverState::TARGET_PREPARED, 9, 9));
    CHECK_FALSE(ReleaseAccepted(EHandoverState::PATH_SWITCH_PENDING, 9, 9));
}

TEST_CASE("AckUsable rejects a missing target id and unusable endpoints")
{
    std::vector<XnForwardingEndpoint> good;
    good.push_back(MakeEndpoint(0x1000, "c0a81e03"));
    CHECK(AckUsable(5, good));
    // Xn-U forwarding is optional: an Ack that offers no endpoint is valid, the handover then runs without forwarding.
    CHECK(AckUsable(5, {}));
    CHECK_FALSE(AckUsable(0, good));
    CHECK_FALSE(AckUsable(0, {}));

    std::vector<XnForwardingEndpoint> noTeid;
    noTeid.push_back(MakeEndpoint(0, "c0a81e03"));
    CHECK_FALSE(AckUsable(5, noTeid));

    std::vector<XnForwardingEndpoint> loopback;
    loopback.push_back(MakeEndpoint(0x1000, "7f000001"));
    CHECK_FALSE(AckUsable(5, loopback));

    // One bad endpoint among good ones makes the whole Ack unusable.
    std::vector<XnForwardingEndpoint> mixed;
    mixed.push_back(MakeEndpoint(0x1000, "c0a81e03"));
    mixed.push_back(MakeEndpoint(0x1001, "ffffffff"));
    CHECK_FALSE(AckUsable(5, mixed));
}

TEST_CASE("HandoverExpired compares the age with the timeout")
{
    CHECK_FALSE(HandoverExpired(1000, 1000, 15000));
    CHECK_FALSE(HandoverExpired(1000, 16000, 15000)); // exactly the timeout
    CHECK(HandoverExpired(1000, 16001, 15000));
}

TEST_CASE("PreparationFailureAccepted only while the source waits for the preparation")
{
    CHECK(PreparationFailureAccepted(EHandoverState::PREPARING));
    CHECK_FALSE(PreparationFailureAccepted(EHandoverState::COMMAND_SENT));
    CHECK_FALSE(PreparationFailureAccepted(EHandoverState::TARGET_PREPARED));
    CHECK_FALSE(PreparationFailureAccepted(EHandoverState::PATH_SWITCH_PENDING));
}

TEST_CASE("LateReleaseAccepted needs the same neighbor and a real target UE id")
{
    CHECK(LateReleaseAccepted(2, 9, 2, 9));
    CHECK_FALSE(LateReleaseAccepted(2, 9, 3, 9)); // another neighbor
    CHECK_FALSE(LateReleaseAccepted(2, 9, 2, 8)); // another target UE id
    CHECK_FALSE(LateReleaseAccepted(2, 0, 2, 0)); // the dropped handover never got a target id
    CHECK_FALSE(LateReleaseAccepted(2, 9, 2, 0));
}

TEST_CASE("TargetReleaseOnAbort keeps an attached UE unless the AMF said so or never answered")
{
    // Before the Path Switch the prepared context is released locally, whatever the cause.
    for (auto state : {EHandoverState::PREPARING, EHandoverState::TARGET_PREPARED})
        for (auto cause : {AbortCause::DROPPED, AbortCause::PATH_SWITCH_FAILED, AbortCause::TIMED_OUT})
            CHECK(TargetReleaseOnAbort(false, state, true, cause) == TargetRelease::CONTEXT);
    // Nothing was created by NGAP yet.
    CHECK(TargetReleaseOnAbort(false, EHandoverState::PREPARING, false, AbortCause::DROPPED) == TargetRelease::NONE);
    // The source never created a target context.
    CHECK(TargetReleaseOnAbort(true, EHandoverState::COMMAND_SENT, true, AbortCause::TIMED_OUT) == TargetRelease::NONE);
    // The UE is attached (PATH_SWITCH_PENDING): a lost Xn link or an unusable Ack only drops the record, a rejected Path
    // Switch releases the attached UE, a Path Switch the AMF never answered asks the AMF to release it.
    CHECK(TargetReleaseOnAbort(false, EHandoverState::PATH_SWITCH_PENDING, true, AbortCause::DROPPED) ==
          TargetRelease::NONE);
    CHECK(TargetReleaseOnAbort(false, EHandoverState::PATH_SWITCH_PENDING, true, AbortCause::PATH_SWITCH_FAILED) ==
          TargetRelease::ATTACHED_UE);
    CHECK(TargetReleaseOnAbort(false, EHandoverState::PATH_SWITCH_PENDING, true, AbortCause::TIMED_OUT) ==
          TargetRelease::ASK_AMF);
}

TEST_CASE("PathSwitchAnswerAccepted only for a target waiting for the AMF")
{
    CHECK(PathSwitchAnswerAccepted(false, EHandoverState::PATH_SWITCH_PENDING));
    CHECK_FALSE(PathSwitchAnswerAccepted(true, EHandoverState::PATH_SWITCH_PENDING));
    // A stale answer must not touch a newer handover of the same UE.
    CHECK_FALSE(PathSwitchAnswerAccepted(false, EHandoverState::PREPARING));
    CHECK_FALSE(PathSwitchAnswerAccepted(false, EHandoverState::TARGET_PREPARED));
    CHECK_FALSE(PathSwitchAnswerAccepted(false, EHandoverState::COMMAND_SENT));
}

TEST_CASE("SurvivesAssociationShutdown only for a target waiting for the AMF")
{
    CHECK(SurvivesAssociationShutdown(false, EHandoverState::PATH_SWITCH_PENDING));
    CHECK_FALSE(SurvivesAssociationShutdown(true, EHandoverState::PATH_SWITCH_PENDING));
    CHECK_FALSE(SurvivesAssociationShutdown(false, EHandoverState::PREPARING));
    CHECK_FALSE(SurvivesAssociationShutdown(false, EHandoverState::TARGET_PREPARED));
    CHECK_FALSE(SurvivesAssociationShutdown(true, EHandoverState::COMMAND_SENT));
}

TEST_CASE("CancelTargetOnTimeout only before the handover command")
{
    CHECK(CancelTargetOnTimeout(true, EHandoverState::PREPARING));
    CHECK_FALSE(CancelTargetOnTimeout(true, EHandoverState::COMMAND_SENT));
    CHECK_FALSE(CancelTargetOnTimeout(false, EHandoverState::PREPARING));
    CHECK_FALSE(CancelTargetOnTimeout(false, EHandoverState::TARGET_PREPARED));
}

TEST_CASE("HandoverTimedOut gives the AMF longer once the Path Switch is under way")
{
    constexpr int64_t SHORT = 15000;
    constexpr int64_t LONG = 60000;
    // Started at 1000, path switch at 10000.
    CHECK_FALSE(HandoverTimedOut(EHandoverState::TARGET_PREPARED, 1000, 10000, 16000, SHORT, LONG));
    CHECK(HandoverTimedOut(EHandoverState::TARGET_PREPARED, 1000, 10000, 16001, SHORT, LONG));
    CHECK(HandoverTimedOut(EHandoverState::COMMAND_SENT, 1000, 0, 16001, SHORT, LONG));
    // PATH_SWITCH_PENDING counts from the path switch, with the long timeout.
    CHECK_FALSE(HandoverTimedOut(EHandoverState::PATH_SWITCH_PENDING, 1000, 10000, 60000, SHORT, LONG));
    CHECK_FALSE(HandoverTimedOut(EHandoverState::PATH_SWITCH_PENDING, 1000, 10000, 70000, SHORT, LONG));
    CHECK(HandoverTimedOut(EHandoverState::PATH_SWITCH_PENDING, 1000, 10000, 70001, SHORT, LONG));
}
