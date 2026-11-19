//
// This file is a part of UERANSIM project.
// Copyright (c) 2023 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include "codec.hpp"
#include "policy.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include <gnb/nts.hpp>
#include <gnb/types.hpp>
#include <utils/json.hpp>
#include <utils/logger.hpp>
#include <utils/nts.hpp>

namespace nr::gnb
{

class XnapTask : public NtsTask
{
  private:
    using EHandoverState = xnap::EHandoverState;
    // States the Xn state machine never stored (the context is erased instead) are not modelled.

    struct HandoverContext
    {
        int ueId{};
        uint32_t peerGnbId{};
        int peerClientId{};
        uint32_t sourceUeXnapId{};
        uint32_t targetUeXnapId{};
        bool sourceSide{};
        bool targetPreparationComplete{};
        std::vector<XnForwardingEndpoint> forwardingEndpoints{};
        EHandoverState state{EHandoverState::PREPARING};
        // Time the handover started, used to drop handovers whose peer or UE never answers (until the Path Switch
        // Request is out; then pathSwitchStartedMs counts, because the AMF has to answer).
        int64_t startedMs{};
        // Time the Path Switch Request was sent (PATH_SWITCH_PENDING), see xnap::HandoverTimedOut.
        int64_t pathSwitchStartedMs{};
    };

    static constexpr int LISTENER_ID = 1;
    static constexpr int CLIENT_ID_BASE = 50000;
    static constexpr int TIMER_ID_RECONNECT = 1;
    static constexpr int RECONNECT_PERIOD_MS = 3000;
    // A handover that has not finished by then is dropped, so a stalled peer cannot block the UE for good.
    static constexpr int64_t HANDOVER_TIMEOUT_MS = 15000;
    // The AMF has this long to answer a Path Switch Request before the Xn record of the handover is dropped.
    static constexpr int64_t PATH_SWITCH_TIMEOUT_MS = 60000;
    static_assert(PATH_SWITCH_TIMEOUT_MS > HANDOVER_TIMEOUT_MS, "the AMF gets longer than the preparation");
    // Accepted Xn connections that have not completed Xn Setup yet.
    static constexpr size_t MAX_PENDING_INBOUND = 16;
    // How long a dropped source handover can still be completed by a late UE Context Release.
    static constexpr int64_t DROPPED_SOURCE_MEMORY_MS = 120000;

    TaskBase *m_base;
    std::unique_ptr<Logger> m_logger;
    std::unordered_map<uint32_t, XnNeighborContext> m_neighbors;
    std::unordered_map<int, uint32_t> m_clientToGnb;
    std::unordered_map<int, SctpAssociation> m_pendingInbound;
    // Remote IP address of each pending inbound connection, checked against the configured neighbor at Xn Setup.
    std::unordered_map<int, std::string> m_inboundAddress;
    std::unordered_map<int, HandoverContext> m_handovers;

    // A source handover dropped while its handover command was outstanding. The UE may still have moved to the target,
    // whose UE Context Release can then arrive late; it is honoured (once, from that neighbor, with that target id) so
    // the source context does not leak.
    struct DroppedSourceHandover
    {
        uint32_t peerGnbId{};
        uint32_t targetUeXnapId{};
        int64_t expiresMs{};
    };
    std::unordered_map<int, DroppedSourceHandover> m_droppedSource;

    // An Xn UE Context Release the target could not send because the Xn link to the source was down when the AMF
    // acknowledged the Path Switch. It is sent when the neighbor is connected again, and the source honours it for as
    // long as it keeps its late-release record (DROPPED_SOURCE_MEMORY_MS).
    struct PendingRelease
    {
        uint32_t peerGnbId{};
        uint32_t sourceUeXnapId{};
        uint32_t targetUeXnapId{};
        int64_t expiresMs{};
    };
    std::vector<PendingRelease> m_pendingReleases;

    friend class GnbCmdHandler;

  public:
    explicit XnapTask(TaskBase *base);
    ~XnapTask() override = default;

    [[nodiscard]] Json statusJson() const;
    [[nodiscard]] Json neighborsJson() const;
    bool requestHandover(int ueId, uint32_t targetGnbId, std::string &error);

  protected:
    void onStart() override;
    void onLoop() override;
    void onQuit() override;

  private:
    [[nodiscard]] bool isInitiator(const XnNeighborContext &neighbor) const;
    void connectNeighbor(XnNeighborContext &neighbor);
    void retryDisconnectedNeighbors();
    void handleConnectionAccepted(int clientId, const std::string &remoteAddress);
    void handleConnectionFailure(int clientId, const std::string &error);
    void handleAssociationSetup(int clientId, int associationId, int inStreams, int outStreams);
    void handleAssociationShutdown(int clientId);
    void handleSctpMessage(int clientId, uint16_t stream, const UniqueBuffer &buffer);
    void handleHandoverRequest(int clientId, const xnap::DecodedMessage &decoded);
    void handleHandoverAck(int clientId, const xnap::DecodedMessage &decoded);
    void handleHandoverFailure(int clientId, const xnap::DecodedMessage &decoded);
    void handleHandoverCancel(int clientId, const xnap::DecodedMessage &decoded);
    void handleUeContextRelease(int clientId, const xnap::DecodedMessage &decoded);
    void handleRrcHandoverConfirm(int ueId);
    void handleRrcHandoverCommandFailed(int ueId);
    void handleNgapHandoverResult(const NmGnbNgapToXnap &msg);
    void handleGtpHandoverResult(const NmGnbGtpToXnap &msg);
    bool sendTargetHandoverAck(HandoverContext &context, const std::vector<XnForwardingEndpoint> &endpoints);
    void sendSourceHandoverCommand(HandoverContext &context);
    void sendBuffer(int clientId, UniqueBuffer &&buffer);
    void closeClient(int clientId);
    // Drops the handover of a UE: cancels Xn-U forwarding and, on the target, releases the NGAP/GTP context that was
    // created for it (no AMF signalling), so a later handover of the same UE is not blocked.
    // What happens to a target UE context follows xnap::TargetReleaseOnAbort (the cause matters in PATH_SWITCH_PENDING).
    void abortHandover(int ueId, const char *reason, xnap::AbortCause cause = xnap::AbortCause::DROPPED);
    void expireHandovers();
    // Releases the NGAP, GTP and RRC contexts of a UE that has moved to the target gNB (no AMF signalling).
    void releaseSourceUe(int ueId);
    // Moves a target handover to PATH_SWITCH_PENDING; the state and its timestamp always change together.
    void enterPathSwitchPending(HandoverContext &context);
    // Sends the UE Context Release that could not be sent when the Path Switch was acknowledged, once the neighbor is
    // connected again.
    void flushPendingReleases(uint32_t peerGnbId);
};

} // namespace nr::gnb
