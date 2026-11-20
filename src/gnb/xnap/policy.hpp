//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <gnb/types.hpp>
#include <utils/octet_string.hpp>

namespace nr::gnb::xnap
{

enum class EHandoverState
{
    PREPARING,
    TARGET_PREPARED,
    COMMAND_SENT,
    PATH_SWITCH_PENDING,
};

// True when both strings are numeric IP addresses of the same host. An IPv4-mapped IPv6 address ("::ffff:a.b.c.d")
// equals the plain IPv4 address. Anything that is not a numeric address (a host name, empty) never matches, because the
// Xn peer check must not depend on name resolution.
bool SameHostAddress(const std::string &first, const std::string &second);

// True when a peer-supplied Xn-U forwarding address may be used as the target of downlink GTP-U: a 4-octet IPv4 address
// that is not unspecified, loopback, multicast or reserved (including broadcast).
bool IsUsableForwardingAddress(const OctetString &address);

// An Xn Handover Cancel can only undo a preparation on the target. Once the UE is being moved to the target (the handover
// command was sent, then the path switch) it must not delete the UE's contexts. Before the target has acknowledged, the
// Cancel may leave the target UE id out (0); after the acknowledgement it has to name the id the target gave.
bool CancelAccepted(EHandoverState state, uint32_t receivedTargetUeXnapId, uint32_t ownTargetUeXnapId);

// An Xn UE Context Release deletes the source UE, so it is valid only while this gNB waits for it (the handover command
// was sent) and when it names the target UE id the target gave in its acknowledgement.
bool ReleaseAccepted(EHandoverState state, uint32_t receivedTargetUeXnapId, uint32_t ownTargetUeXnapId);

// A Handover Request Ack is usable when it names a target UE id and every forwarding endpoint has a TEID and an address
// the source may send downlink data to. An Ack without any endpoint is valid: Xn-U forwarding is optional and the
// handover then runs without it.
bool AckUsable(uint32_t targetUeXnapId, const std::vector<XnForwardingEndpoint> &endpoints);

// An Xn Handover Preparation Failure answers a Handover Request, so the source accepts it only while it still waits for
// the preparation (before it sent the handover command to the UE).
bool PreparationFailureAccepted(EHandoverState state);

// A late Xn UE Context Release for a source handover that was already dropped is honoured only from the neighbor the
// handover was started with, and only with the target UE id that neighbor gave (0 means none was ever given).
bool LateReleaseAccepted(uint32_t droppedPeerGnbId, uint32_t droppedTargetUeXnapId, uint32_t senderGnbId,
                         uint32_t receivedTargetUeXnapId);

// Why a handover is dropped.
enum class AbortCause
{
    DROPPED,            // the peer, the radio or a local failure ended it (cancel, link loss, failure to send)
    PATH_SWITCH_FAILED, // the AMF rejected the Path Switch
    TIMED_OUT,          // the handover took too long
};

// What dropping a handover does with the UE context that was created for it on the target.
enum class TargetRelease
{
    NONE,        // nothing, the Xn record is all there is
    CONTEXT,     // release the NGAP/GTP context locally; the UE never attached and the AMF knows nothing of it
    ATTACHED_UE, // release the NGAP/GTP context and the RRC connection locally (the AMF rejected the Path Switch)
    ASK_AMF,     // ask the AMF to release the UE; its release command removes the contexts
};

// Before the Path Switch Request is out the prepared context is simply released. Once it is out (PATH_SWITCH_PENDING)
// the UE is attached to this gNB and the AMF may already have switched the path, so a lost Xn link or an unusable
// acknowledgement (DROPPED) only drops the Xn record, a rejected Path Switch releases the attached UE, and a Path
// Switch the AMF never answered (TIMED_OUT) asks the AMF to release the UE. Nothing is created on the source.
TargetRelease TargetReleaseOnAbort(bool sourceSide, EHandoverState state, bool targetPreparationComplete,
                                   AbortCause cause);

// A Path Switch answer from NGAP (acknowledge, failure, unusable acknowledgement) belongs only to a target that is
// waiting for it. An answer for a record in another state is stale (a newer handover of the same UE) and is ignored.
bool PathSwitchAnswerAccepted(bool sourceSide, EHandoverState state);

// A target in PATH_SWITCH_PENDING waits for the AMF, not for its peer, so the loss of the Xn association does not end
// its handover.
bool SurvivesAssociationShutdown(bool sourceSide, EHandoverState state);

// On a timeout the source tells the target to cancel while the target may still hold a prepared context for it, that is
// before the handover command went to the UE.
bool CancelTargetOnTimeout(bool sourceSide, EHandoverState state);

// A handover is dropped when it takes too long. Once the Path Switch Request is out (PATH_SWITCH_PENDING) the answer
// depends on the AMF, so the longer pathSwitchTimeoutMs counts from the start of the Path Switch instead.
bool HandoverTimedOut(EHandoverState state, int64_t startedMs, int64_t pathSwitchStartedMs, int64_t nowMs,
                      int64_t timeoutMs, int64_t pathSwitchTimeoutMs);

// True when a handover that started at startedMs is older than timeoutMs.
bool HandoverExpired(int64_t startedMs, int64_t nowMs, int64_t timeoutMs);

} // namespace nr::gnb::xnap
