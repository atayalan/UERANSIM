//
// This file is a part of UERANSIM project.
// Copyright (c) 2023 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "task.hpp"

#include "codec.hpp"
#include "policy.hpp"
#include "snapshot.hpp"

#include <gnb/gtp/task.hpp>
#include <gnb/ngap/task.hpp>
#include <gnb/rls/task.hpp>
#include <gnb/rrc/task.hpp>
#include <gnb/sctp/task.hpp>

#include <utils/common.hpp>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <optional>
#include <vector>

namespace nr::gnb
{

namespace
{

std::vector<uint8_t> MakeTargetCellContainer(uint32_t gnbId)
{
    return {static_cast<uint8_t>((gnbId >> 24U) & 0xffU), static_cast<uint8_t>((gnbId >> 16U) & 0xffU),
            static_cast<uint8_t>((gnbId >> 8U) & 0xffU), static_cast<uint8_t>(gnbId & 0xffU)};
}

} // namespace

XnapTask::XnapTask(TaskBase *base) : m_base{base}
{
    m_logger = base->logBase->makeUniqueLogger("xnap");

    int ctxId = 1;
    for (const auto &config : base->config->xn.neighbors)
    {
        XnNeighborContext neighbor{};
        neighbor.gnbId = config.gnbId;
        neighbor.ctxId = ctxId;
        neighbor.sctpClientId = CLIENT_ID_BASE + ctxId;
        neighbor.address = config.address;
        neighbor.port = config.port;
        m_clientToGnb[neighbor.sctpClientId] = neighbor.gnbId;
        m_neighbors.emplace(neighbor.gnbId, neighbor);
        ++ctxId;
    }
}

Json XnapTask::statusJson() const
{
    int connected = 0;
    for (const auto &item : m_neighbors)
    {
        if (item.second.state == EXnNeighborState::CONNECTED)
            ++connected;
    }

    return Json::Obj({
        {"enabled", m_base->config->xn.enabled},
        {"bind-ip", m_base->config->xn.bindIp},
        {"port", static_cast<int>(m_base->config->xn.port)},
        {"configured-neighbors", static_cast<int>(m_neighbors.size())},
        {"connected-neighbors", connected},
    });
}

Json XnapTask::neighborsJson() const
{
    Json result = Json::Arr({});
    for (const auto &item : m_neighbors)
        result.push(ToJson(item.second));
    return result;
}

bool XnapTask::requestHandover(int ueId, uint32_t targetGnbId, std::string &error)
{
    auto neighborIt = m_neighbors.find(targetGnbId);
    if (neighborIt == m_neighbors.end())
    {
        error = "Target gNB is not configured as an Xn neighbor";
        return false;
    }
    auto &neighbor = neighborIt->second;
    if (neighbor.state != EXnNeighborState::CONNECTED)
    {
        error = "Target Xn neighbor is not connected";
        return false;
    }
    if (m_handovers.count(ueId) != 0)
    {
        error = "A handover is already active for this UE";
        return false;
    }

    // A new attempt supersedes any late release still expected for the previous one.
    m_droppedSource.erase(ueId);

    HandoverContext context{};
    context.ueId = ueId;
    context.peerGnbId = targetGnbId;
    context.peerClientId = neighbor.sctpClientId;
    context.sourceUeXnapId = static_cast<uint32_t>(ueId);
    context.sourceSide = true;
    context.startedMs = utils::CurrentTimeMillis();
    context.state = EHandoverState::PREPARING;

    xnap::HandoverData data{};
    data.sourceUeXnapId = context.sourceUeXnapId;
    auto ngapIt = m_base->ngapTask->m_ueCtx.find(ueId);
    if (ngapIt == m_base->ngapTask->m_ueCtx.end() || ngapIt->second == nullptr)
    {
        error = "Source NGAP UE context does not exist";
        return false;
    }
    if (ngapIt->second->amfUeNgapId <= 0)
    {
        error = "The UE has no AMF UE NGAP ID yet";
        return false;
    }
    data.amfUeNgapId = static_cast<uint64_t>(ngapIt->second->amfUeNgapId);

    if (m_base->gtpTask == nullptr)
    {
        error = "GTP task is not available";
        return false;
    }

    XnHandoverNgapSnapshot snapshot{};
    snapshot.sti = m_base->rlsTask->udpTask()->stiOf(ueId);
    if (snapshot.sti == 0)
    {
        error = "The radio link does not know this UE";
        return false;
    }
    snapshot.ueAmbr = ngapIt->second->ueAmbr;
    snapshot.securityCapabilities = ngapIt->second->securityCapabilities;
    snapshot.pduSessions = m_base->gtpTask->handoverSessionSnapshot(ueId);
    for (const auto &session : snapshot.pduSessions)
    {
        XnForwardingEndpoint forwarding{};
        forwarding.psi = session.psi;
        forwarding.sessionType = session.sessionType;
        forwarding.qosFlowIds = session.qosFlowIds;
        data.forwardingEndpoints.push_back(std::move(forwarding));
    }
    auto serializedSnapshot = SerializeHandoverSnapshot(snapshot);
    if (!serializedSnapshot.has_value())
    {
        error = "Could not serialize Xn handover snapshot";
        return false;
    }
    data.transparentContainer = std::move(serializedSnapshot.value());

    // Xn neighbors in UERANSIM are intra-PLMN/intra-TA by design. Build the target CGI
    // from the configured peer gNB ID while retaining the local PLMN/TAC and ID length.
    GnbConfig targetConfig = *m_base->config;
    const int cellBits = 36 - static_cast<int>(targetConfig.gnbIdLength);
    targetConfig.nci = (static_cast<uint64_t>(targetGnbId) << cellBits);

    auto encoded = xnap::EncodeHandoverRequest(data, targetConfig);
    if (encoded.size() == 0)
    {
        error = "Could not encode Xn Handover Request";
        return false;
    }

    m_handovers.emplace(ueId, context);
    sendBuffer(neighbor.sctpClientId, std::move(encoded));
    m_logger->info("Xn handover preparation started for UE[%d] toward gNB[%u]", ueId, targetGnbId);
    return true;
}

bool XnapTask::isInitiator(const XnNeighborContext &neighbor) const
{
    return m_base->config->getGnbId() < neighbor.gnbId;
}

void XnapTask::onStart()
{
    auto listen = std::make_unique<NmGnbSctp>(NmGnbSctp::LISTEN_REQUEST);
    listen->listenerId = LISTENER_ID;
    listen->localAddress = m_base->config->xn.bindIp;
    listen->localPort = m_base->config->xn.port;
    listen->ppid = sctp::PayloadProtocolId::XNAP;
    listen->associatedTask = this;
    m_base->sctpTask->push(std::move(listen));

    for (auto &item : m_neighbors)
    {
        if (isInitiator(item.second))
            connectNeighbor(item.second);
    }

    setTimer(TIMER_ID_RECONNECT, RECONNECT_PERIOD_MS);
}

void XnapTask::onLoop()
{
    auto msg = take();
    if (!msg)
        return;

    switch (msg->msgType)
    {
    case NtsMessageType::GNB_SCTP: {
        auto &w = dynamic_cast<NmGnbSctp &>(*msg);
        switch (w.present)
        {
        case NmGnbSctp::CONNECTION_ACCEPTED:
            handleConnectionAccepted(w.clientId, w.remoteAddress);
            break;
        case NmGnbSctp::CONNECTION_FAILURE:
            handleConnectionFailure(w.clientId, w.error);
            break;
        case NmGnbSctp::ASSOCIATION_SETUP:
            handleAssociationSetup(w.clientId, w.associationId, w.inStreams, w.outStreams);
            break;
        case NmGnbSctp::ASSOCIATION_SHUTDOWN:
            handleAssociationShutdown(w.clientId);
            break;
        case NmGnbSctp::RECEIVE_MESSAGE:
            handleSctpMessage(w.clientId, w.stream, w.buffer);
            break;
        default:
            m_logger->unhandledNts(*msg);
            break;
        }
        break;
    }
    case NtsMessageType::TIMER_EXPIRED: {
        auto &w = dynamic_cast<NmTimerExpired &>(*msg);
        if (w.timerId == TIMER_ID_RECONNECT)
        {
            retryDisconnectedNeighbors();
            expireHandovers();
            setTimer(TIMER_ID_RECONNECT, RECONNECT_PERIOD_MS);
        }
        break;
    }
    case NtsMessageType::GNB_RRC_TO_XNAP: {
        auto &w = dynamic_cast<NmGnbRrcToXnap &>(*msg);
        switch (w.present)
        {
        case NmGnbRrcToXnap::HANDOVER_CONFIRM:
            handleRrcHandoverConfirm(w.ueId);
            break;
        case NmGnbRrcToXnap::HANDOVER_COMMAND_FAILED:
            handleRrcHandoverCommandFailed(w.ueId);
            break;
        }
        break;
    }
    case NtsMessageType::GNB_NGAP_TO_XNAP: {
        auto &w = dynamic_cast<NmGnbNgapToXnap &>(*msg);
        handleNgapHandoverResult(w);
        break;
    }
    case NtsMessageType::GNB_GTP_TO_XNAP: {
        auto &w = dynamic_cast<NmGnbGtpToXnap &>(*msg);
        handleGtpHandoverResult(w);
        break;
    }
    default:
        m_logger->unhandledNts(*msg);
        break;
    }
}

void XnapTask::onQuit()
{
    for (const auto &item : m_clientToGnb)
        closeClient(item.first);
    for (const auto &item : m_pendingInbound)
        closeClient(item.first);
    m_pendingInbound.clear();
}

void XnapTask::connectNeighbor(XnNeighborContext &neighbor)
{
    if (neighbor.state == EXnNeighborState::SCTP_CONNECTING || neighbor.state == EXnNeighborState::XN_SETUP_PENDING ||
        neighbor.state == EXnNeighborState::CONNECTED)
        return;

    neighbor.state = EXnNeighborState::SCTP_CONNECTING;
    neighbor.association = {};

    auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::CONNECTION_REQUEST);
    msg->clientId = neighbor.sctpClientId;
    msg->localAddress = m_base->config->xn.bindIp;
    msg->localPort = 0;
    msg->remoteAddress = neighbor.address;
    msg->remotePort = neighbor.port;
    msg->ppid = sctp::PayloadProtocolId::XNAP;
    msg->associatedTask = this;
    m_base->sctpTask->push(std::move(msg));

    m_logger->info("Connecting Xn neighbor gNB[%u] at %s:%d", neighbor.gnbId, neighbor.address.c_str(), neighbor.port);
}

void XnapTask::retryDisconnectedNeighbors()
{
    for (auto &item : m_neighbors)
    {
        auto &neighbor = item.second;
        if (isInitiator(neighbor) &&
            (neighbor.state == EXnNeighborState::DISCONNECTED || neighbor.state == EXnNeighborState::FAILED))
            connectNeighbor(neighbor);
    }
}

void XnapTask::handleConnectionAccepted(int clientId, const std::string &remoteAddress)
{
    if (clientId <= 0)
        return;

    // Only a configured neighbor may open an Xn association, and only a bounded number may wait for Xn Setup.
    const bool fromNeighbor = std::any_of(m_neighbors.begin(), m_neighbors.end(), [&](const auto &item) {
        return xnap::SameHostAddress(remoteAddress, item.second.address);
    });
    if (!fromNeighbor || m_pendingInbound.size() >= MAX_PENDING_INBOUND)
    {
        m_logger->warn("Rejecting inbound Xn SCTP connection from %s: %s", remoteAddress.c_str(),
                       fromNeighbor ? "too many connections wait for Xn Setup" : "not a configured Xn neighbor");
        closeClient(clientId);
        return;
    }

    m_pendingInbound.try_emplace(clientId, SctpAssociation{});
    m_inboundAddress[clientId] = remoteAddress;
    m_logger->debug("Accepted inbound Xn SCTP connection clientId[%d] from %s", clientId, remoteAddress.c_str());
}

void XnapTask::handleConnectionFailure(int clientId, const std::string &error)
{
    auto mapIt = m_clientToGnb.find(clientId);
    if (mapIt == m_clientToGnb.end())
        return;
    auto neighborIt = m_neighbors.find(mapIt->second);
    if (neighborIt == m_neighbors.end())
        return;
    neighborIt->second.state = EXnNeighborState::FAILED;
    m_logger->warn("Xn SCTP connection to gNB[%u] failed: %s", neighborIt->second.gnbId, error.c_str());
}

void XnapTask::handleAssociationSetup(int clientId, int associationId, int inStreams, int outStreams)
{
    SctpAssociation association{};
    association.associationId = associationId;
    association.inStreams = inStreams;
    association.outStreams = outStreams;

    auto mapIt = m_clientToGnb.find(clientId);
    if (mapIt != m_clientToGnb.end())
    {
        auto neighborIt = m_neighbors.find(mapIt->second);
        if (neighborIt == m_neighbors.end())
            return;

        auto &neighbor = neighborIt->second;
        neighbor.association = association;
        neighbor.nextStream = 0;
        neighbor.state = EXnNeighborState::XN_SETUP_PENDING;
        sendBuffer(clientId, xnap::EncodeSetupRequest(*m_base->config));
        m_logger->debug("Xn SCTP association established with gNB[%u], sending Xn Setup Request", neighbor.gnbId);
        return;
    }

    auto pending = m_pendingInbound.find(clientId);
    if (pending != m_pendingInbound.end())
        pending->second = association;
}

void XnapTask::handleAssociationShutdown(int clientId)
{
    std::vector<int> affected;
    for (const auto &item : m_handovers)
    {
        // A target in PATH_SWITCH_PENDING waits for the AMF, not for the peer: the UE is attached here and the answer
        // still has to be acted on (the release towards the source then uses the neighbor's current association).
        if (item.second.peerClientId == clientId &&
            !xnap::SurvivesAssociationShutdown(item.second.sourceSide, item.second.state))
            affected.push_back(item.first);
    }
    for (int ueId : affected)
        abortHandover(ueId, "Xn association shutdown");

    auto mapIt = m_clientToGnb.find(clientId);
    if (mapIt != m_clientToGnb.end())
    {
        auto neighborIt = m_neighbors.find(mapIt->second);
        if (neighborIt != m_neighbors.end())
        {
            auto &neighbor = neighborIt->second;
            neighbor.state = EXnNeighborState::DISCONNECTED;
            neighbor.association = {};
            m_logger->warn("Xn association with gNB[%u] was shut down", neighbor.gnbId);
            closeClient(clientId);
        }
    }
    else
    {
        m_pendingInbound.erase(clientId);
        m_inboundAddress.erase(clientId);
        closeClient(clientId);
    }
}

void XnapTask::handleSctpMessage(int clientId, uint16_t stream, const UniqueBuffer &buffer)
{
    (void)stream;
    auto decoded = xnap::Decode(buffer.data(), buffer.size());
    if (!decoded.has_value())
    {
        m_logger->warn("Failed to decode XnAP PDU from clientId[%d]", clientId);
        return;
    }

    if (decoded->type == xnap::MessageType::XN_SETUP_REQUEST)
    {
        auto neighborIt = m_neighbors.find(decoded->peerGnbId);
        auto pendingIt = m_pendingInbound.find(clientId);
        if (neighborIt == m_neighbors.end() || pendingIt == m_pendingInbound.end())
        {
            m_logger->warn("Rejecting Xn Setup from unconfigured gNB[%u]", decoded->peerGnbId);
            closeClient(clientId);
            m_pendingInbound.erase(clientId);
            m_inboundAddress.erase(clientId);
            return;
        }

        auto &neighbor = neighborIt->second;

        // The gNB id in the Setup Request is only a claim: the connection must come from the neighbor's address.
        const auto addressIt = m_inboundAddress.find(clientId);
        if (addressIt == m_inboundAddress.end() || !xnap::SameHostAddress(addressIt->second, neighbor.address))
        {
            m_logger->warn("Rejecting Xn Setup claiming gNB[%u] from %s: not the configured neighbor address",
                           decoded->peerGnbId, addressIt == m_inboundAddress.end() ? "?" : addressIt->second.c_str());
            closeClient(clientId);
            m_pendingInbound.erase(clientId);
            m_inboundAddress.erase(clientId);
            return;
        }

        // A live Xn link is never replaced by a new association; the old one must shut down first. The gNB with the lower
        // id is the initiator (isInitiator), so while its own outbound attempt is under way it ignores an inbound one.
        if (neighbor.state == EXnNeighborState::CONNECTED ||
            (isInitiator(neighbor) && neighbor.state != EXnNeighborState::DISCONNECTED &&
             neighbor.state != EXnNeighborState::FAILED))
        {
            m_logger->warn("Dropping duplicate inbound Xn association from gNB[%u]", decoded->peerGnbId);
            closeClient(clientId);
            m_pendingInbound.erase(clientId);
            m_inboundAddress.erase(clientId);
            return;
        }

        m_clientToGnb.erase(neighbor.sctpClientId);
        neighbor.sctpClientId = clientId;
        neighbor.association = pendingIt->second;
        neighbor.nextStream = 0;
        neighbor.state = EXnNeighborState::CONNECTED;
        m_clientToGnb[clientId] = neighbor.gnbId;
        m_pendingInbound.erase(pendingIt);
        m_inboundAddress.erase(clientId);

        sendBuffer(clientId, xnap::EncodeSetupResponse(*m_base->config));
        m_logger->info("Xn Setup complete with gNB[%u] (inbound)", neighbor.gnbId);
        flushPendingReleases(neighbor.gnbId);
        return;
    }

    auto mapIt = m_clientToGnb.find(clientId);
    if (mapIt == m_clientToGnb.end())
        return;
    auto neighborIt = m_neighbors.find(mapIt->second);
    if (neighborIt == m_neighbors.end())
        return;
    auto &neighbor = neighborIt->second;

    if (decoded->type == xnap::MessageType::XN_SETUP_RESPONSE)
    {
        if (neighbor.state != EXnNeighborState::XN_SETUP_PENDING)
        {
            m_logger->warn("Ignoring unsolicited Xn Setup Response from gNB[%u]", neighbor.gnbId);
            return;
        }
        if (decoded->peerGnbId != neighbor.gnbId)
        {
            m_logger->err("Xn Setup peer mismatch: configured gNB[%u], received gNB[%u]", neighbor.gnbId,
                          decoded->peerGnbId);
            neighbor.state = EXnNeighborState::FAILED;
            closeClient(clientId);
            return;
        }
        neighbor.state = EXnNeighborState::CONNECTED;
        m_logger->info("Xn Setup complete with gNB[%u] (outbound)", neighbor.gnbId);
        flushPendingReleases(neighbor.gnbId);
    }
    else if (decoded->type == xnap::MessageType::XN_SETUP_FAILURE)
    {
        neighbor.state = EXnNeighborState::FAILED;
        m_logger->warn("Xn Setup failed with gNB[%u]", neighbor.gnbId);
        closeClient(clientId);
    }
    else if (decoded->type == xnap::MessageType::HANDOVER_REQUEST)
    {
        handleHandoverRequest(clientId, *decoded);
    }
    else if (decoded->type == xnap::MessageType::HANDOVER_REQUEST_ACK)
    {
        handleHandoverAck(clientId, *decoded);
    }
    else if (decoded->type == xnap::MessageType::HANDOVER_PREPARATION_FAILURE)
    {
        handleHandoverFailure(clientId, *decoded);
    }
    else if (decoded->type == xnap::MessageType::HANDOVER_CANCEL)
    {
        handleHandoverCancel(clientId, *decoded);
    }
    else if (decoded->type == xnap::MessageType::UE_CONTEXT_RELEASE)
    {
        handleUeContextRelease(clientId, *decoded);
    }
    else
    {
        m_logger->debug("XnAP PDU received from gNB[%u] but procedure is not implemented yet", neighbor.gnbId);
    }
}

void XnapTask::handleHandoverRequest(int clientId, const xnap::DecodedMessage &decoded)
{
    if (!decoded.handover.has_value())
        return;

    const auto &request = decoded.handover.value();
    if (request.sourceUeXnapId == 0 || request.sourceUeXnapId > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        !request.amfUeNgapId.has_value() || request.amfUeNgapId.value() == 0 ||
        request.amfUeNgapId.value() > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
    {
        xnap::HandoverData failure{};
        failure.sourceUeXnapId = request.sourceUeXnapId;
        sendBuffer(clientId, xnap::EncodeHandoverPreparationFailure(failure));
        m_logger->warn("Rejecting Xn Handover Request with invalid UE identifiers");
        return;
    }
    auto snapshot = DeserializeHandoverSnapshot(request.transparentContainer);
    if (snapshot == nullptr)
    {
        xnap::HandoverData failure{};
        failure.sourceUeXnapId = request.sourceUeXnapId;
        sendBuffer(clientId, xnap::EncodeHandoverPreparationFailure(failure));
        m_logger->warn("Rejecting Xn Handover Request from source UE[%u]: malformed handover snapshot",
                       request.sourceUeXnapId);
        return;
    }

    // UE ids are allocated per gNB by the radio link, so the id the source used is not the id this gNB gave the UE. The
    // sti is the same on every gNB; use it to find the local id, which every task of this gNB (RRC, GTP, NGAP) uses.
    const int ueId = snapshot->sti == 0 ? 0 : m_base->rlsTask->udpTask()->ueIdOf(snapshot->sti);
    if (ueId == 0)
    {
        xnap::HandoverData failure{};
        failure.sourceUeXnapId = request.sourceUeXnapId;
        sendBuffer(clientId, xnap::EncodeHandoverPreparationFailure(failure));
        m_logger->warn("Rejecting Xn Handover Request from source UE[%u]: the UE is not in radio range of this gNB",
                       request.sourceUeXnapId);
        return;
    }
    if (m_handovers.count(ueId) != 0)
    {
        xnap::HandoverData failure{};
        failure.sourceUeXnapId = request.sourceUeXnapId;
        sendBuffer(clientId, xnap::EncodeHandoverPreparationFailure(failure));
        return;
    }

    auto mapIt = m_clientToGnb.find(clientId);
    if (mapIt == m_clientToGnb.end())
        return;

    HandoverContext context{};
    context.ueId = ueId;
    context.peerGnbId = mapIt->second;
    context.peerClientId = clientId;
    context.sourceUeXnapId = request.sourceUeXnapId;
    // The target XnAP ID is the local UE id, so the RRC handover confirm, which carries the local id, finds this context.
    context.targetUeXnapId = static_cast<uint32_t>(ueId);
    context.sourceSide = false;
    context.forwardingEndpoints = request.forwardingEndpoints;
    context.state = EHandoverState::PREPARING;
    context.startedMs = utils::CurrentTimeMillis();
    m_handovers.emplace(ueId, context);

    auto prepare = std::make_unique<NmGnbXnapToNgap>(NmGnbXnapToNgap::PREPARE_HANDOVER_TARGET);
    prepare->ueId = ueId;
    if (request.amfUeNgapId.has_value())
        prepare->amfUeNgapId = static_cast<int64_t>(request.amfUeNgapId.value());
    prepare->handoverSnapshot = std::move(snapshot);
    m_base->ngapTask->push(std::move(prepare));
    m_logger->info("Xn Handover Request for UE[%d] from gNB[%u] forwarded to NGAP target preparation", ueId,
                   context.peerGnbId);
}

void XnapTask::handleHandoverAck(int clientId, const xnap::DecodedMessage &decoded)
{
    if (!decoded.handover.has_value())
        return;
    const auto &ack = decoded.handover.value();
    if (ack.sourceUeXnapId > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        return;
    int ueId = static_cast<int>(ack.sourceUeXnapId);
    auto it = m_handovers.find(ueId);
    if (it == m_handovers.end() || !it->second.sourceSide || it->second.peerClientId != clientId ||
        it->second.state != EHandoverState::PREPARING)
        return;

    auto &context = it->second;

    // The target names where this gNB must send the downlink data of the UE. Only the configured neighbor can get here,
    // but a bad address would still turn this gNB into a sender towards an arbitrary host.
    const bool endpointsUsable = xnap::AckUsable(ack.targetUeXnapId, ack.forwardingEndpoints);
    if (!endpointsUsable)
    {
        xnap::HandoverData handoverCancel{};
        handoverCancel.sourceUeXnapId = context.sourceUeXnapId;
        handoverCancel.targetUeXnapId = ack.targetUeXnapId;
        sendBuffer(clientId, xnap::EncodeHandoverCancel(handoverCancel));
        m_logger->warn("Xn Handover Request Ack for UE[%d] has no target UE id or an unusable forwarding address, handover dropped", ueId);
        abortHandover(ueId, "unusable Handover Request Ack");
        return;
    }

    context.targetUeXnapId = ack.targetUeXnapId;
    context.forwardingEndpoints = ack.forwardingEndpoints;
    if (context.forwardingEndpoints.empty())
    {
        sendSourceHandoverCommand(context);
        return;
    }

    auto activate = std::make_unique<NmGnbXnapToGtp>(NmGnbXnapToGtp::ACTIVATE_SOURCE_FORWARDING);
    activate->ueId = ueId;
    activate->endpoints = context.forwardingEndpoints;
    m_base->gtpTask->push(std::move(activate));
    m_logger->info("Xn Handover Request acknowledged for UE[%d], activating Xn-U forwarding", ueId);
}

void XnapTask::handleHandoverFailure(int clientId, const xnap::DecodedMessage &decoded)
{
    if (!decoded.handover.has_value())
        return;
    if (decoded.handover->sourceUeXnapId > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        return;
    int ueId = static_cast<int>(decoded.handover->sourceUeXnapId);
    auto it = m_handovers.find(ueId);
    if (it == m_handovers.end() || !it->second.sourceSide || it->second.peerClientId != clientId)
        return;
    if (!xnap::PreparationFailureAccepted(it->second.state))
    {
        m_logger->warn("Ignoring Xn Handover Preparation Failure for UE[%d]: the handover command was already sent",
                       ueId);
        return;
    }
    abortHandover(ueId, "Xn handover preparation failed");
}

void XnapTask::handleHandoverCancel(int clientId, const xnap::DecodedMessage &decoded)
{
    if (!decoded.handover.has_value() ||
        decoded.handover->sourceUeXnapId > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        return;

    // The context is keyed by the local UE id; the source names the UE by its own id.
    auto it = std::find_if(m_handovers.begin(), m_handovers.end(), [&](const auto &item) {
        const auto &candidate = item.second;
        return !candidate.sourceSide && candidate.peerClientId == clientId &&
               candidate.sourceUeXnapId == decoded.handover->sourceUeXnapId;
    });
    if (it == m_handovers.end())
        return;
    const int ueId = it->first;

    if (!xnap::CancelAccepted(it->second.state, decoded.handover->targetUeXnapId, it->second.targetUeXnapId))
    {
        m_logger->warn("Ignoring Xn Handover Cancel for UE[%d]: the UE is moving to this gNB or the target UE id is wrong",
                       ueId);
        return;
    }

    m_logger->warn("Xn Handover Cancel received for target UE[%d] from gNB[%u]", ueId, it->second.peerGnbId);
    abortHandover(ueId, "Xn Handover Cancel");
}

void XnapTask::handleUeContextRelease(int clientId, const xnap::DecodedMessage &decoded)
{
    if (!decoded.handover.has_value())
        return;
    if (decoded.handover->sourceUeXnapId > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        return;
    int ueId = static_cast<int>(decoded.handover->sourceUeXnapId);
    auto it = m_handovers.find(ueId);
    if (it == m_handovers.end())
    {
        // A release can arrive after the source dropped its record (timeout, link flap) while the UE did move.
        const auto dropped = m_droppedSource.find(ueId);
        const auto peer = m_clientToGnb.find(clientId);
        if (dropped != m_droppedSource.end() && peer != m_clientToGnb.end() &&
            xnap::LateReleaseAccepted(dropped->second.peerGnbId, dropped->second.targetUeXnapId, peer->second,
                                      decoded.handover->targetUeXnapId))
        {
            m_droppedSource.erase(dropped);
            m_logger->info("Late Xn UE Context Release received for source UE[%d]", ueId);
            releaseSourceUe(ueId);
        }
        return;
    }
    if (!it->second.sourceSide || it->second.peerClientId != clientId)
        return;
    // Only the release that answers this gNB's handover command is valid: the source UE is deleted by it. The release
    // has to name the target UE id the target gave in its acknowledgement.
    if (!xnap::ReleaseAccepted(it->second.state, decoded.handover->targetUeXnapId, it->second.targetUeXnapId))
    {
        m_logger->warn("Ignoring Xn UE Context Release for UE[%d]: no matching handover command is outstanding", ueId);
        return;
    }
    m_logger->info("Xn UE Context Release received for source UE[%d]", ueId);
    m_handovers.erase(it);
    m_droppedSource.erase(ueId);
    releaseSourceUe(ueId);
}

void XnapTask::handleRrcHandoverConfirm(int ueId)
{
    auto it = m_handovers.find(ueId);
    if (it == m_handovers.end() || it->second.sourceSide)
    {
        m_logger->warn("Unexpected RRC handover confirm for UE[%d]", ueId);
        return;
    }
    if (it->second.state != EHandoverState::TARGET_PREPARED)
    {
        m_logger->warn("Unexpected RRC handover confirm for UE[%d] in Xn state[%d]", ueId,
                       static_cast<int>(it->second.state));
        return;
    }

    auto pathSwitch = std::make_unique<NmGnbXnapToNgap>(NmGnbXnapToNgap::PATH_SWITCH_REQUEST);
    pathSwitch->ueId = ueId;
    m_base->ngapTask->push(std::move(pathSwitch));
    enterPathSwitchPending(it->second);
    m_logger->info("UE[%d] reached target gNB[%u]; Path Switch Request started", ueId,
                   m_base->config->getGnbId());
}

void XnapTask::handleNgapHandoverResult(const NmGnbNgapToXnap &msg)
{
    auto it = m_handovers.find(msg.ueId);
    if (it == m_handovers.end())
    {
        m_logger->warn("Unexpected NGAP-to-Xn handover result for UE[%d]", msg.ueId);
        if (msg.present == NmGnbNgapToXnap::TARGET_PREPARE_ACK)
        {
            // The handover was dropped while NGAP was preparing the target: remove what NGAP just created.
            auto release = std::make_unique<NmGnbXnapToNgap>(NmGnbXnapToNgap::RELEASE_HANDOVER_SOURCE);
            release->ueId = msg.ueId;
            m_base->ngapTask->push(std::move(release));
        }
        else if (msg.present == NmGnbNgapToXnap::PATH_SWITCH_FAILURE)
        {
            // The AMF rejected a Path Switch whose Xn record was already dropped (timeout): the UE has no core path, so
            // the target releases it as it would have with the record.
            auto release = std::make_unique<NmGnbXnapToNgap>(NmGnbXnapToNgap::RELEASE_FAILED_TARGET);
            release->ueId = msg.ueId;
            m_base->ngapTask->push(std::move(release));
            m_logger->warn("Late Path Switch failure for UE[%d]: the UE was released", msg.ueId);
        }
        else if (msg.present == NmGnbNgapToXnap::PATH_SWITCH_ACK)
        {
            m_logger->warn("Late Path Switch acknowledge for UE[%d]: the Xn record was dropped, so the source gNB is not "
                           "told to release its context",
                           msg.ueId);
        }
        return;
    }

    auto &context = it->second;
    switch (msg.present)
    {
    case NmGnbNgapToXnap::TARGET_PREPARE_ACK: {
        if (context.sourceSide || context.state != EHandoverState::PREPARING)
        {
            m_logger->warn("Unexpected target prepare ACK for UE[%d]", msg.ueId);
            return;
        }

        context.targetPreparationComplete = true;
        if (context.forwardingEndpoints.empty())
        {
            if (!sendTargetHandoverAck(context, {}))
                abortHandover(msg.ueId, "Xn Handover Request Ack could not be sent");
            break;
        }
        const auto targetSessions = m_base->gtpTask->handoverSessionSnapshot(msg.ueId);
        const bool allTargetSessionsProposed =
            targetSessions.size() == context.forwardingEndpoints.size() &&
            std::all_of(targetSessions.begin(), targetSessions.end(), [&](const auto &session) {
                return std::find_if(context.forwardingEndpoints.begin(), context.forwardingEndpoints.end(),
                                    [&](const auto &proposal) {
                                        return proposal.psi == session.psi && !proposal.qosFlowIds.empty();
                                    }) != context.forwardingEndpoints.end();
            });
        if (!allTargetSessionsProposed)
        {
            xnap::HandoverData failure{};
            failure.sourceUeXnapId = context.sourceUeXnapId;
            sendBuffer(context.peerClientId, xnap::EncodeHandoverPreparationFailure(failure));
            m_logger->warn("Target PDU sessions do not match Xn-U forwarding proposal for UE[%d]", msg.ueId);
            abortHandover(msg.ueId, "forwarding proposal mismatch");
            break;
        }
        auto allocate = std::make_unique<NmGnbXnapToGtp>(NmGnbXnapToGtp::ALLOCATE_TARGET_FORWARDING);
        allocate->ueId = msg.ueId;
        m_base->gtpTask->push(std::move(allocate));
        m_logger->info("Target NGAP preparation complete for UE[%d], allocating Xn-U forwarding", msg.ueId);
        break;
    }
    case NmGnbNgapToXnap::TARGET_PREPARE_FAILURE: {
        // A preparation can only fail before this gNB sent its Ack; a failure report after it belongs to nothing.
        if (context.sourceSide || context.state != EHandoverState::PREPARING)
        {
            m_logger->warn("Ignoring target prepare failure for UE[%d]: no preparation is outstanding", msg.ueId);
            return;
        }
        xnap::HandoverData failure{};
        failure.sourceUeXnapId = context.sourceUeXnapId;
        sendBuffer(context.peerClientId, xnap::EncodeHandoverPreparationFailure(failure));
        m_logger->warn("Target NGAP preparation failed for UE[%d]: %s", msg.ueId, msg.cause.c_str());
        abortHandover(msg.ueId, "target NGAP preparation failed");
        break;
    }
    case NmGnbNgapToXnap::PATH_SWITCH_ACK: {
        if (!xnap::PathSwitchAnswerAccepted(context.sourceSide, context.state))
        {
            m_logger->warn("Unexpected Path Switch ACK for UE[%d]", msg.ueId);
            return;
        }
        // The association the handover started on may have been replaced while the AMF answered: use the neighbor's
        // current one.
        const auto neighborIt = m_neighbors.find(context.peerGnbId);
        const bool linkUp = neighborIt != m_neighbors.end() && neighborIt->second.state == EXnNeighborState::CONNECTED;
        if (linkUp)
        {
            xnap::HandoverData release{};
            release.sourceUeXnapId = context.sourceUeXnapId;
            release.targetUeXnapId = context.targetUeXnapId;
            sendBuffer(neighborIt->second.sctpClientId, xnap::EncodeUeContextRelease(release));
            m_logger->info("Path Switch completed for UE[%d]; sent Xn UE Context Release to source gNB[%u]", msg.ueId,
                           context.peerGnbId);
        }
        else
        {
            m_pendingReleases.push_back({context.peerGnbId, context.sourceUeXnapId, context.targetUeXnapId,
                                         utils::CurrentTimeMillis() + DROPPED_SOURCE_MEMORY_MS});
            m_logger->warn("Path Switch completed for UE[%d] but the Xn link to source gNB[%u] is down: the UE Context "
                           "Release is sent when the link is up again",
                           msg.ueId, context.peerGnbId);
        }
        m_handovers.erase(it);
        break;
    }
    case NmGnbNgapToXnap::PATH_SWITCH_UNUSABLE:
        if (xnap::PathSwitchAnswerAccepted(context.sourceSide, context.state))
        {
            // The AMF was asked to release the UE; its release command removes the contexts. Only the record goes.
            m_logger->warn("Path Switch acknowledge for UE[%d] is unusable: %s", msg.ueId, msg.cause.c_str());
            abortHandover(msg.ueId, "Path Switch acknowledge unusable");
        }
        break;
    case NmGnbNgapToXnap::PATH_SWITCH_FAILURE:
        if (xnap::PathSwitchAnswerAccepted(context.sourceSide, context.state))
        {
            m_logger->warn("Path Switch failed for UE[%d]: %s", msg.ueId, msg.cause.c_str());
            abortHandover(msg.ueId, "Path Switch failed", xnap::AbortCause::PATH_SWITCH_FAILED);
        }
        break;
    }
}

void XnapTask::handleGtpHandoverResult(const NmGnbGtpToXnap &msg)
{
    if (msg.present == NmGnbGtpToXnap::TARGET_SESSIONS_READY)
    {
        NmGnbNgapToXnap ready{NmGnbNgapToXnap::TARGET_PREPARE_ACK};
        ready.ueId = msg.ueId;
        handleNgapHandoverResult(ready);
        return;
    }
    auto it = m_handovers.find(msg.ueId);
    if (it == m_handovers.end())
        return;
    auto &context = it->second;

    if (msg.present == NmGnbGtpToXnap::TARGET_FORWARDING_ALLOCATED)
    {
        if (context.sourceSide || !context.targetPreparationComplete || context.state != EHandoverState::PREPARING)
            return;

        std::vector<XnForwardingEndpoint> accepted;
        for (const auto &allocated : msg.endpoints)
        {
            auto proposal = std::find_if(context.forwardingEndpoints.begin(), context.forwardingEndpoints.end(),
                                         [&](const auto &item) { return item.psi == allocated.psi; });
            if (proposal == context.forwardingEndpoints.end())
                continue;
            XnForwardingEndpoint endpoint = allocated;
            endpoint.qosFlowIds.erase(
                std::remove_if(endpoint.qosFlowIds.begin(), endpoint.qosFlowIds.end(), [&](int qfi) {
                    return std::find(proposal->qosFlowIds.begin(), proposal->qosFlowIds.end(), qfi) ==
                           proposal->qosFlowIds.end();
                }),
                endpoint.qosFlowIds.end());
            if (!endpoint.qosFlowIds.empty())
                accepted.push_back(std::move(endpoint));
        }
        if (accepted.size() != context.forwardingEndpoints.size() || !sendTargetHandoverAck(context, accepted))
            abortHandover(msg.ueId, "target Xn-U forwarding could not be offered");
        return;
    }

    if (msg.present == NmGnbGtpToXnap::SOURCE_FORWARDING_READY)
    {
        if (!context.sourceSide || context.state != EHandoverState::PREPARING)
            return;
        sendSourceHandoverCommand(context);
        return;
    }

    if (msg.present == NmGnbGtpToXnap::FORWARDING_FAILURE)
    {
        if (context.sourceSide && context.targetUeXnapId != 0)
        {
            xnap::HandoverData handoverCancel{};
            handoverCancel.sourceUeXnapId = context.sourceUeXnapId;
            handoverCancel.targetUeXnapId = context.targetUeXnapId;
            sendBuffer(context.peerClientId, xnap::EncodeHandoverCancel(handoverCancel));
        }
        else if (!context.sourceSide)
        {
            xnap::HandoverData failure{};
            failure.sourceUeXnapId = context.sourceUeXnapId;
            sendBuffer(context.peerClientId, xnap::EncodeHandoverPreparationFailure(failure));
        }
        m_logger->warn("Xn-U forwarding failed for UE[%d]: %s", msg.ueId, msg.cause.c_str());
        abortHandover(msg.ueId, "Xn-U forwarding failed");
    }
}

bool XnapTask::sendTargetHandoverAck(HandoverContext &context, const std::vector<XnForwardingEndpoint> &endpoints)
{
    xnap::HandoverData ack{};
    ack.sourceUeXnapId = context.sourceUeXnapId;
    ack.targetUeXnapId = context.targetUeXnapId;
    ack.transparentContainer = MakeTargetCellContainer(m_base->config->getGnbId());
    ack.forwardingEndpoints = endpoints;
    auto encoded = xnap::EncodeHandoverRequestAck(ack);
    if (encoded.size() == 0)
    {
        xnap::HandoverData failure{};
        failure.sourceUeXnapId = context.sourceUeXnapId;
        sendBuffer(context.peerClientId, xnap::EncodeHandoverPreparationFailure(failure));
        m_logger->err("Could not encode Xn Handover Request Ack for UE[%d]", context.ueId);
        return false;
    }
    context.forwardingEndpoints = endpoints;
    sendBuffer(context.peerClientId, std::move(encoded));
    context.state = EHandoverState::TARGET_PREPARED;
    m_logger->info("Target ready for UE[%d], sent Xn Handover Request Ack with %zu forwarding endpoint(s)",
                   context.ueId, endpoints.size());
    return true;
}

void XnapTask::sendSourceHandoverCommand(HandoverContext &context)
{
    context.state = EHandoverState::COMMAND_SENT;
    OctetString targetCellContainer{};
    targetCellContainer.appendOctet4(static_cast<octet4>(context.peerGnbId));
    auto msg = std::make_unique<NmGnbXnapToRrc>(NmGnbXnapToRrc::SEND_HANDOVER_COMMAND);
    msg->ueId = context.ueId;
    msg->targetCellContainer = std::move(targetCellContainer);
    m_base->rrcTask->push(std::move(msg));
    m_logger->info("Xn-U forwarding ready for UE[%d], sending RRC handover command", context.ueId);
}

void XnapTask::sendBuffer(int clientId, UniqueBuffer &&buffer)
{
    if (buffer.size() == 0)
        return;
    auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::SEND_MESSAGE);
    msg->clientId = clientId;
    msg->stream = 0;
    msg->buffer = std::move(buffer);
    m_base->sctpTask->push(std::move(msg));
}

void XnapTask::closeClient(int clientId)
{
    auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::CONNECTION_CLOSE);
    msg->clientId = clientId;
    m_base->sctpTask->push(std::move(msg));
}


void XnapTask::abortHandover(int ueId, const char *reason, xnap::AbortCause cause)
{
    auto it = m_handovers.find(ueId);
    if (it == m_handovers.end())
        return;
    const HandoverContext context = it->second;
    m_handovers.erase(it);

    // The UE has been told to move: the target may still complete the handover, so remember it for a late release.
    if (context.sourceSide && context.state == EHandoverState::COMMAND_SENT && context.targetUeXnapId != 0)
    {
        m_droppedSource[ueId] = {context.peerGnbId, context.targetUeXnapId,
                                 utils::CurrentTimeMillis() + DROPPED_SOURCE_MEMORY_MS};
    }

    auto cancel = std::make_unique<NmGnbXnapToGtp>(NmGnbXnapToGtp::CANCEL_FORWARDING);
    cancel->ueId = ueId;
    m_base->gtpTask->push(std::move(cancel));

    // On the target, NGAP created a UE context and GTP the PDU sessions once preparation completed. What dropping the
    // handover does with them depends on how far it got (see xnap::TargetReleaseOnAbort): before the Path Switch the
    // context is released locally, because a context left behind makes every later handover of this UE fail as
    // "already exists"; once the UE is attached the AMF is involved.
    std::optional<NmGnbXnapToNgap::PR> release;
    switch (xnap::TargetReleaseOnAbort(context.sourceSide, context.state, context.targetPreparationComplete, cause))
    {
    case xnap::TargetRelease::NONE:
        break;
    case xnap::TargetRelease::CONTEXT:
        release = NmGnbXnapToNgap::RELEASE_HANDOVER_SOURCE;
        break;
    case xnap::TargetRelease::ATTACHED_UE:
        release = NmGnbXnapToNgap::RELEASE_FAILED_TARGET;
        break;
    case xnap::TargetRelease::ASK_AMF:
        release = NmGnbXnapToNgap::REQUEST_UE_RELEASE;
        break;
    }
    if (release.has_value())
    {
        auto message = std::make_unique<NmGnbXnapToNgap>(*release);
        message->ueId = ueId;
        m_base->ngapTask->push(std::move(message));
    }
    m_logger->warn("Handover of UE[%d] dropped: %s", ueId, reason);
}

void XnapTask::expireHandovers()
{
    const int64_t now = utils::CurrentTimeMillis();
    std::vector<int> expired;
    for (const auto &item : m_handovers)
    {
        if (xnap::HandoverTimedOut(item.second.state, item.second.startedMs, item.second.pathSwitchStartedMs, now,
                                   HANDOVER_TIMEOUT_MS, PATH_SWITCH_TIMEOUT_MS))
            expired.push_back(item.first);
    }
    for (int ueId : expired)
    {
        const auto it = m_handovers.find(ueId);
        if (it != m_handovers.end() && xnap::CancelTargetOnTimeout(it->second.sourceSide, it->second.state) &&
            m_clientToGnb.count(it->second.peerClientId) != 0)
        {
            // The target may still hold a prepared context; do not leave it to its own timeout.
            xnap::HandoverData handoverCancel{};
            handoverCancel.sourceUeXnapId = it->second.sourceUeXnapId;
            handoverCancel.targetUeXnapId = it->second.targetUeXnapId;
            sendBuffer(it->second.peerClientId, xnap::EncodeHandoverCancel(handoverCancel));
        }
        abortHandover(ueId, "handover timed out", xnap::AbortCause::TIMED_OUT);
    }

    for (auto it = m_droppedSource.begin(); it != m_droppedSource.end();)
        it = it->second.expiresMs < now ? m_droppedSource.erase(it) : std::next(it);

    m_pendingReleases.erase(std::remove_if(m_pendingReleases.begin(), m_pendingReleases.end(),
                                           [now](const PendingRelease &item) { return item.expiresMs < now; }),
                            m_pendingReleases.end());
}

void XnapTask::enterPathSwitchPending(HandoverContext &context)
{
    context.state = EHandoverState::PATH_SWITCH_PENDING;
    context.pathSwitchStartedMs = utils::CurrentTimeMillis();
}

void XnapTask::flushPendingReleases(uint32_t peerGnbId)
{
    const auto neighborIt = m_neighbors.find(peerGnbId);
    if (neighborIt == m_neighbors.end() || neighborIt->second.state != EXnNeighborState::CONNECTED)
        return;

    for (auto it = m_pendingReleases.begin(); it != m_pendingReleases.end();)
    {
        if (it->peerGnbId != peerGnbId)
        {
            ++it;
            continue;
        }
        xnap::HandoverData release{};
        release.sourceUeXnapId = it->sourceUeXnapId;
        release.targetUeXnapId = it->targetUeXnapId;
        sendBuffer(neighborIt->second.sctpClientId, xnap::EncodeUeContextRelease(release));
        m_logger->info("Sent the postponed Xn UE Context Release for source UE[%u] to gNB[%u]", it->sourceUeXnapId,
                       peerGnbId);
        it = m_pendingReleases.erase(it);
    }
}

void XnapTask::releaseSourceUe(int ueId)
{
    auto ngapRelease = std::make_unique<NmGnbXnapToNgap>(NmGnbXnapToNgap::RELEASE_HANDOVER_SOURCE);
    ngapRelease->ueId = ueId;
    m_base->ngapTask->push(std::move(ngapRelease));

    auto rrcRelease = std::make_unique<NmGnbXnapToRrc>(NmGnbXnapToRrc::RELEASE_HANDOVER_SOURCE);
    rrcRelease->ueId = ueId;
    m_base->rrcTask->push(std::move(rrcRelease));
}

void XnapTask::handleRrcHandoverCommandFailed(int ueId)
{
    auto it = m_handovers.find(ueId);
    if (it == m_handovers.end() || !it->second.sourceSide || it->second.state != EHandoverState::COMMAND_SENT)
        return;

    // The UE never got the command, so the target must not wait for it.
    xnap::HandoverData handoverCancel{};
    handoverCancel.sourceUeXnapId = it->second.sourceUeXnapId;
    handoverCancel.targetUeXnapId = it->second.targetUeXnapId;
    sendBuffer(it->second.peerClientId, xnap::EncodeHandoverCancel(handoverCancel));
    abortHandover(ueId, "RRC handover command could not be sent to the UE");
    // The UE never got the command, so no late release is expected for it.
    m_droppedSource.erase(ueId);
}

} // namespace nr::gnb
