//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "policy.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include <arpa/inet.h>

namespace nr::gnb::xnap
{

namespace
{

// Parses a numeric IPv4 or IPv6 address into the 16-octet IPv6 form (IPv4 becomes IPv4-mapped).
bool ParseAddress(const std::string &text, std::array<uint8_t, 16> &out)
{
    in6_addr v6{};
    if (inet_pton(AF_INET6, text.c_str(), &v6) == 1)
    {
        std::memcpy(out.data(), &v6, out.size());
        return true;
    }
    in_addr v4{};
    if (inet_pton(AF_INET, text.c_str(), &v4) == 1)
    {
        out.fill(0);
        out[10] = 0xff;
        out[11] = 0xff;
        std::memcpy(out.data() + 12, &v4, sizeof(v4));
        return true;
    }
    return false;
}

} // namespace

bool SameHostAddress(const std::string &first, const std::string &second)
{
    std::array<uint8_t, 16> a{};
    std::array<uint8_t, 16> b{};
    return ParseAddress(first, a) && ParseAddress(second, b) && a == b;
}

bool IsUsableForwardingAddress(const OctetString &address)
{
    if (address.length() != 4)
        return false;
    const int first = address.get(0);
    if (first == 0 || first == 127)
        return false;
    // 224.0.0.0/4 multicast and 240.0.0.0/4 reserved, which includes the limited broadcast 255.255.255.255.
    return first < 224;
}

bool CancelAccepted(EHandoverState state, uint32_t receivedTargetUeXnapId, uint32_t ownTargetUeXnapId)
{
    if (state == EHandoverState::PREPARING)
        return receivedTargetUeXnapId == 0 || receivedTargetUeXnapId == ownTargetUeXnapId;
    if (state == EHandoverState::TARGET_PREPARED)
        return ownTargetUeXnapId != 0 && receivedTargetUeXnapId == ownTargetUeXnapId;
    return false;
}

bool ReleaseAccepted(EHandoverState state, uint32_t receivedTargetUeXnapId, uint32_t ownTargetUeXnapId)
{
    return state == EHandoverState::COMMAND_SENT && ownTargetUeXnapId != 0 &&
           receivedTargetUeXnapId == ownTargetUeXnapId;
}

bool AckUsable(uint32_t targetUeXnapId, const std::vector<XnForwardingEndpoint> &endpoints)
{
    return targetUeXnapId != 0 && std::all_of(endpoints.begin(), endpoints.end(), [](const auto &endpoint) {
               return endpoint.tunnel.teid != 0 && IsUsableForwardingAddress(endpoint.tunnel.address);
           });
}

bool PreparationFailureAccepted(EHandoverState state)
{
    return state == EHandoverState::PREPARING;
}

bool LateReleaseAccepted(uint32_t droppedPeerGnbId, uint32_t droppedTargetUeXnapId, uint32_t senderGnbId,
                         uint32_t receivedTargetUeXnapId)
{
    return droppedTargetUeXnapId != 0 && senderGnbId == droppedPeerGnbId &&
           receivedTargetUeXnapId == droppedTargetUeXnapId;
}

TargetRelease TargetReleaseOnAbort(bool sourceSide, EHandoverState state, bool targetPreparationComplete,
                                   AbortCause cause)
{
    if (sourceSide || !targetPreparationComplete)
        return TargetRelease::NONE;
    if (state != EHandoverState::PATH_SWITCH_PENDING)
        return TargetRelease::CONTEXT;
    switch (cause)
    {
    case AbortCause::PATH_SWITCH_FAILED:
        return TargetRelease::ATTACHED_UE;
    case AbortCause::TIMED_OUT:
        return TargetRelease::ASK_AMF;
    case AbortCause::DROPPED:
        break;
    }
    return TargetRelease::NONE;
}

bool PathSwitchAnswerAccepted(bool sourceSide, EHandoverState state)
{
    return !sourceSide && state == EHandoverState::PATH_SWITCH_PENDING;
}

bool SurvivesAssociationShutdown(bool sourceSide, EHandoverState state)
{
    return !sourceSide && state == EHandoverState::PATH_SWITCH_PENDING;
}

bool CancelTargetOnTimeout(bool sourceSide, EHandoverState state)
{
    return sourceSide && state == EHandoverState::PREPARING;
}

bool HandoverTimedOut(EHandoverState state, int64_t startedMs, int64_t pathSwitchStartedMs, int64_t nowMs,
                      int64_t timeoutMs, int64_t pathSwitchTimeoutMs)
{
    if (state == EHandoverState::PATH_SWITCH_PENDING)
        return HandoverExpired(pathSwitchStartedMs, nowMs, pathSwitchTimeoutMs);
    return HandoverExpired(startedMs, nowMs, timeoutMs);
}

bool HandoverExpired(int64_t startedMs, int64_t nowMs, int64_t timeoutMs)
{
    return nowMs - startedMs > timeoutMs;
}

} // namespace nr::gnb::xnap
