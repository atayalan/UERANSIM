//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "task.hpp"

#include <gnb/gtp/proto.hpp>
#include <gnb/rls/task.hpp>
#include <gnb/xnap/task.hpp>
#include <utils/common.hpp>
#include <utils/constants.hpp>
#include <utils/libc_error.hpp>

#include <algorithm>
#include <unordered_set>

#include <asn/ngap/ASN_NGAP_QosFlowSetupRequestItem.h>

namespace nr::gnb
{

GtpTask::GtpTask(TaskBase *base)
    : m_base{base}, m_udpServer{}, m_ueContexts{}, m_rateLimiter(std::make_unique<RateLimiter>()), m_pduSessions{},
      m_pduSessionMutex{}, m_sessionTree{}
{
    m_logger = m_base->logBase->makeUniqueLogger("gtp");
}

void GtpTask::onStart()
{
    try
    {
        m_udpServer = new udp::UdpServerTask(m_base->config->gtpIp, cons::GtpPort, this);
        m_udpServer->start();
    }
    catch (const LibError &e)
    {
        m_logger->err("GTP/UDP task could not be created. %s", e.what());
    }
}

void GtpTask::onQuit()
{
    m_udpServer->quit();
    delete m_udpServer;

    m_ueContexts.clear();
}

void GtpTask::onLoop()
{
    auto msg = take();
    if (!msg)
        return;

    switch (msg->msgType)
    {
    case NtsMessageType::GNB_NGAP_TO_GTP: {
        auto &w = dynamic_cast<NmGnbNgapToGtp &>(*msg);
        switch (w.present)
        {
        case NmGnbNgapToGtp::UE_CONTEXT_UPDATE: {
            handleUeContextUpdate(*w.update);
            break;
        }
        case NmGnbNgapToGtp::UE_CONTEXT_RELEASE: {
            handleUeContextDelete(w.ueId);
            break;
        }
        case NmGnbNgapToGtp::SESSION_CREATE: {
            handleSessionCreate(w.resource);
            break;
        }
        case NmGnbNgapToGtp::SESSION_RELEASE: {
            handleSessionRelease(w.ueId, w.psi);
            break;
        }
        case NmGnbNgapToGtp::XN_TARGET_PREPARE_COMPLETE: {
            auto ready = std::make_unique<NmGnbGtpToXnap>(NmGnbGtpToXnap::TARGET_SESSIONS_READY);
            ready->ueId = w.ueId;
            m_base->xnapTask->push(std::move(ready));
            break;
        }
        }
        break;
    }
    case NtsMessageType::GNB_RLS_TO_GTP: {
        auto &w = dynamic_cast<NmGnbRlsToGtp &>(*msg);
        switch (w.present)
        {
        case NmGnbRlsToGtp::DATA_PDU_DELIVERY: {
            handleUplinkData(w.ueId, w.psi, std::move(w.pdu));
            break;
        }
        }
        break;
    }
    case NtsMessageType::GNB_XNAP_TO_GTP: {
        auto &w = dynamic_cast<NmGnbXnapToGtp &>(*msg);
        switch (w.present)
        {
        case NmGnbXnapToGtp::ALLOCATE_TARGET_FORWARDING:
            handleAllocateTargetForwarding(w.ueId);
            break;
        case NmGnbXnapToGtp::ACTIVATE_SOURCE_FORWARDING:
            handleActivateSourceForwarding(w.ueId, w.endpoints);
            break;
        case NmGnbXnapToGtp::CANCEL_FORWARDING:
            handleCancelForwarding(w.ueId);
            break;
        }
        break;
    }
    case NtsMessageType::UDP_SERVER_RECEIVE:
        handleUdpReceive(dynamic_cast<udp::NwUdpServerReceive &>(*msg));
        break;
    default:
        m_logger->unhandledNts(*msg);
        break;
    }
}

void GtpTask::handleUeContextUpdate(const GtpUeContextUpdate &msg)
{
    if (!m_ueContexts.count(msg.ueId))
        m_ueContexts[msg.ueId] = std::make_unique<GtpUeContext>(msg.ueId);

    auto &ue = m_ueContexts[msg.ueId];
    ue->ueAmbr = msg.ueAmbr;

    updateAmbrForUe(ue->ueId);
}

void GtpTask::handleSessionCreate(PduSessionResource *session)
{
    if (!m_ueContexts.count(session->ueId))
    {
        m_logger->err("PDU session resource could not be created, UE context with ID[%d] not found", session->ueId);
        return;
    }

    uint64_t sessionInd = MakeSessionResInd(session->ueId, session->psi);
    uint32_t oldTeid = 0;
    bool replaced = false;
    {
        const std::lock_guard<std::mutex> lock(m_pduSessionMutex);
        auto it = m_pduSessions.find(sessionInd);
        if (it != m_pduSessions.end())
        {
            oldTeid = it->second->downTunnel.teid;
            it->second->downTunnel.teid = session->downTunnel.teid;
            it->second->downTunnel.address = std::move(session->downTunnel.address);
            delete session;
            replaced = true;
        }
        else
        {
            m_pduSessions[sessionInd] = std::unique_ptr<PduSessionResource>(session);
        }
    }

    if (replaced)
    {
        const auto newTeid = m_pduSessions[sessionInd]->downTunnel.teid;
        m_sessionTree.remove(sessionInd, oldTeid);
        m_sessionTree.insert(sessionInd, newTeid);
        m_logger->warn("PDU session [%016llx] already exists - TEID DL replaced %u -> %u",
                       static_cast<unsigned long long>(sessionInd), oldTeid, newTeid);
        return;
    }

    m_sessionTree.insert(sessionInd, session->downTunnel.teid);

    updateAmbrForUe(session->ueId);
    updateAmbrForSession(sessionInd);
}

void GtpTask::handleSessionRelease(int ueId, int psi)
{
    if (!m_ueContexts.count(ueId))
    {
        m_logger->err("PDU session resource could not be released, UE context with ID[%d] not found", ueId);
        return;
    }

    uint64_t sessionInd = MakeSessionResInd(ueId, psi);
    clearForwardingForSession(sessionInd);

    // Remove all session information from rate limiter
    m_rateLimiter->updateSessionUplinkLimit(sessionInd, 0);
    m_rateLimiter->updateUeDownlinkLimit(ueId, 0);

    // And remove from PDU session table
    uint32_t teid = 0;
    bool removed = false;
    {
        const std::lock_guard<std::mutex> lock(m_pduSessionMutex);
        auto it = m_pduSessions.find(sessionInd);
        if (it != m_pduSessions.end())
        {
            teid = it->second->downTunnel.teid;
            m_pduSessions.erase(it);
            removed = true;
        }
    }
    if (removed)
    {
        // And remove from the tree
        m_sessionTree.remove(sessionInd, teid);
    }
}

void GtpTask::handleUeContextDelete(int ueId)
{
    clearSourceForwardingForUe(ueId, true);
    clearTargetForwardingForUe(ueId);

    // Find PDU sessions of the UE
    std::vector<uint64_t> sessions{};
    m_sessionTree.enumerateByUe(ueId, sessions);

    for (auto &session : sessions)
    {
        // Remove all session information from rate limiter
        m_rateLimiter->updateSessionUplinkLimit(session, 0);
        m_rateLimiter->updateUeDownlinkLimit(ueId, 0);

        // And remove from PDU session table
        uint32_t teid = 0;
        {
            const std::lock_guard<std::mutex> lock(m_pduSessionMutex);
            auto it = m_pduSessions.find(session);
            if (it == m_pduSessions.end())
                continue;
            teid = it->second->downTunnel.teid;
            m_pduSessions.erase(it);
        }

        // And remove from the tree
        m_sessionTree.remove(session, teid);
    }

    // Remove all user information from rate limiter
    m_rateLimiter->updateUeUplinkLimit(ueId, 0);
    m_rateLimiter->updateUeDownlinkLimit(ueId, 0);

    // Remove UE context
    m_ueContexts.erase(ueId);
}

std::vector<GtpTask::SessionSnapshot> GtpTask::sessionSnapshot(int ueId) const
{
    std::vector<SessionSnapshot> result;
    const std::lock_guard<std::mutex> lock(m_pduSessionMutex);

    for (const auto &entry : m_pduSessions)
    {
        const auto &resource = entry.second;
        if (resource == nullptr || resource->ueId != ueId)
            continue;

        SessionSnapshot snapshot{};
        snapshot.psi = resource->psi;
        snapshot.downTunnel.teid = resource->downTunnel.teid;
        snapshot.downTunnel.address = resource->downTunnel.address.copy();

        if (resource->qosFlows)
        {
            const auto &flows = resource->qosFlows->list;
            for (int i = 0; i < flows.count; ++i)
            {
                if (flows.array[i] != nullptr)
                    snapshot.qosFlowIds.push_back(static_cast<int>(flows.array[i]->qosFlowIdentifier));
            }
        }

        result.push_back(std::move(snapshot));
    }

    return result;
}

std::vector<XnHandoverPduSessionSnapshot> GtpTask::handoverSessionSnapshot(int ueId) const
{
    std::vector<XnHandoverPduSessionSnapshot> result;
    const std::lock_guard<std::mutex> lock(m_pduSessionMutex);

    for (const auto &entry : m_pduSessions)
    {
        const auto &resource = entry.second;
        if (resource == nullptr || resource->ueId != ueId)
            continue;

        XnHandoverPduSessionSnapshot snapshot{};
        snapshot.psi = resource->psi;
        snapshot.sessionType = resource->sessionType;
        snapshot.sessionAmbr = resource->sessionAmbr;
        snapshot.upTunnel.teid = resource->upTunnel.teid;
        snapshot.upTunnel.address = resource->upTunnel.address.copy();

        if (resource->qosFlows)
        {
            const auto &flows = resource->qosFlows->list;
            for (int i = 0; i < flows.count; ++i)
            {
                if (flows.array[i] != nullptr)
                    snapshot.qosFlowIds.push_back(static_cast<int>(flows.array[i]->qosFlowIdentifier));
            }
        }

        result.push_back(std::move(snapshot));
    }

    return result;
}

bool GtpTask::updateSessionUplinkTunnel(int ueId, int psi, uint32_t teid, const OctetString &address)
{
    const std::lock_guard<std::mutex> lock(m_pduSessionMutex);
    auto it = m_pduSessions.find(MakeSessionResInd(ueId, psi));
    if (it == m_pduSessions.end())
        return false;

    it->second->upTunnel.teid = teid;
    it->second->upTunnel.address = address.copy();
    return true;
}

void GtpTask::handleUplinkData(int ueId, int psi, OctetString &&pdu)
{
    uint64_t sessionInd = MakeSessionResInd(ueId, psi);

    uint32_t upTeid = 0;
    OctetString upAddress{};
    int qfi = 0;
    PduSessionType sessionType = PduSessionType::UNSTRUCTURED;
    {
        const std::lock_guard<std::mutex> lock(m_pduSessionMutex);
        auto it = m_pduSessions.find(sessionInd);
        if (it == m_pduSessions.end())
        {
            m_logger->err("Uplink data failure, PDU session not found. UE[%d] PSI[%d]", ueId, psi);
            return;
        }

        const auto &pduSession = it->second;
        upTeid = pduSession->upTunnel.teid;
        upAddress = pduSession->upTunnel.address.copy();
        sessionType = pduSession->sessionType;
        if (!pduSession->qosFlows || pduSession->qosFlows->list.count == 0 || pduSession->qosFlows->list.array[0] == nullptr)
        {
            m_logger->err("Uplink data failure, no QoS flow. UE[%d] PSI[%d]", ueId, psi);
            return;
        }
        qfi = static_cast<int>(pduSession->qosFlows->list.array[0]->qosFlowIdentifier);
    }

    if (upTeid == 0 || upAddress.length() == 0)
    {
        m_logger->err("Uplink data failure, invalid UP tunnel. UE[%d] PSI[%d]", ueId, psi);
        return;
    }

    if (!IsUplinkPduAllowed(sessionType, pdu))
        return;

    if (m_rateLimiter->allowUplinkPacket(sessionInd, static_cast<int64_t>(pdu.length())))
    {
        gtp::GtpMessage gtp{};
        gtp.payload = std::move(pdu);
        gtp.msgType = gtp::GtpMessage::MT_G_PDU;
        gtp.teid = upTeid;

        auto ul = std::make_unique<gtp::UlPduSessionInformation>();
        // TODO: currently using first QFI
        ul->qfi = qfi;

        auto cont = std::make_unique<gtp::PduSessionContainerExtHeader>();
        cont->pduSessionInformation = std::move(ul);
        gtp.extHeaders.push_back(std::move(cont));

        OctetString gtpPdu;
        if (!gtp::EncodeGtpMessage(gtp, gtpPdu))
            m_logger->err("Uplink data failure, GTP encoding failed");
        else
            m_udpServer->send(InetAddress(upAddress, cons::GtpPort), gtpPdu);
    }
}

void GtpTask::handleUdpReceive(const udp::NwUdpServerReceive &msg)
{
    OctetView buffer{msg.packet};
    auto gtp = gtp::DecodeGtpMessage(buffer);
    if (gtp == nullptr)
    {
        m_logger->warn("Ignoring malformed GTP-U packet");
        return;
    }

    switch (gtp->msgType)
    {
    case gtp::GtpMessage::MT_G_PDU: {
        auto sessionInd = m_sessionTree.findByDownTeid(gtp->teid);
        if (sessionInd == 0)
        {
            auto forwarding = m_targetForwarding.find(gtp->teid);
            if (forwarding == m_targetForwarding.end())
            {
                m_logger->err("TEID %d not found on GTP-U Downlink or Xn-U forwarding", gtp->teid);
                return;
            }

            sessionInd = forwarding->second;
            auto w = std::make_unique<NmGnbGtpToRls>(NmGnbGtpToRls::DATA_PDU_DELIVERY);
            w->ueId = GetUeId(sessionInd);
            w->psi = GetPsi(sessionInd);
            w->pdu = std::move(gtp->payload);
            m_base->rlsTask->push(std::move(w));
            return;
        }

        if (m_rateLimiter->allowDownlinkPacket(sessionInd, gtp->payload.length()))
        {
            int downlinkQfi = 0;
            for (const auto &header : gtp->extHeaders)
            {
                if (header == nullptr || header->type != gtp::ExtHeaderType::PduSessionContainerExtHeader)
                    continue;
                const auto *container = static_cast<const gtp::PduSessionContainerExtHeader *>(header.get());
                if (container->pduSessionInformation == nullptr ||
                    container->pduSessionInformation->pduType != gtp::PduSessionInformation::PDU_TYPE_DL)
                    continue;
                downlinkQfi = static_cast<const gtp::DlPduSessionInformation *>(container->pduSessionInformation.get())->qfi;
                break;
            }
            forwardDownlink(sessionInd, gtp->payload, downlinkQfi);

            auto w = std::make_unique<NmGnbGtpToRls>(NmGnbGtpToRls::DATA_PDU_DELIVERY);
            w->ueId = GetUeId(sessionInd);
            w->psi = GetPsi(sessionInd);
            w->pdu = std::move(gtp->payload);
            m_base->rlsTask->push(std::move(w));
        }
        return;
    }
    case gtp::GtpMessage::MT_END_MARKER: {
        auto forwarding = m_targetForwarding.find(gtp->teid);
        if (forwarding != m_targetForwarding.end())
        {
            const int ueId = GetUeId(forwarding->second);
            const int psi = GetPsi(forwarding->second);
            m_targetForwarding.erase(forwarding);
            m_logger->debug("Xn-U End Marker received for UE[%d] PSI[%d] TEID[%u]", ueId, psi, gtp->teid);
        }
        return;
    }
    case gtp::GtpMessage::MT_ECHO_REQUEST: {
        gtp::GtpMessage gtpResponse{};
        gtpResponse.msgType = gtp::GtpMessage::MT_ECHO_RESPONSE;
        gtpResponse.seq = gtp->seq;
        gtpResponse.payload = OctetString::FromOctet2({14, 0});

        OctetString gtpPdu;
        if (gtp::EncodeGtpMessage(gtpResponse, gtpPdu))
            m_udpServer->send(msg.fromAddress, gtpPdu);
        else
            m_logger->err("Uplink data failure, GTP encoding failed");
        return;
    }
    default: {
        m_logger->err("Unhandled GTP-U message type: %d", gtp->msgType);
        return;
    }
    }
}

void GtpTask::handleAllocateTargetForwarding(int ueId)
{
    if (m_udpServer == nullptr)
    {
        reportForwardingFailure(ueId, "GTP-U UDP server is not available for target Xn-U forwarding");
        return;
    }

    clearTargetForwardingForUe(ueId);

    std::vector<XnForwardingEndpoint> endpoints;
    const std::string forwardingIp = m_base->config->gtpAdvertiseIp.value_or(m_base->config->gtpIp);
    const OctetString forwardingAddress = utils::IpToOctetString(forwardingIp);

    {
        const std::lock_guard<std::mutex> lock(m_pduSessionMutex);
        for (const auto &entry : m_pduSessions)
        {
            const auto &resource = entry.second;
            if (resource == nullptr || resource->ueId != ueId)
                continue;

            const uint32_t forwardingTeid = allocateForwardingTeid();
            if (forwardingTeid == 0)
            {
                clearTargetForwardingForUe(ueId);
                reportForwardingFailure(ueId, "No free Xn-U forwarding TEID");
                return;
            }

            XnForwardingEndpoint endpoint{};
            endpoint.psi = resource->psi;
            endpoint.tunnel.teid = forwardingTeid;
            endpoint.tunnel.address = forwardingAddress.copy();
            if (resource->qosFlows)
            {
                const auto &flows = resource->qosFlows->list;
                for (int i = 0; i < flows.count; ++i)
                {
                    if (flows.array[i] != nullptr)
                        endpoint.qosFlowIds.push_back(static_cast<int>(flows.array[i]->qosFlowIdentifier));
                }
            }

            m_targetForwarding.emplace(forwardingTeid, entry.first);
            endpoints.push_back(std::move(endpoint));
        }
    }

    if (endpoints.empty())
    {
        reportForwardingFailure(ueId, "No target PDU session available for Xn-U forwarding");
        return;
    }

    const size_t allocatedSessionCount = endpoints.size();
    auto response = std::make_unique<NmGnbGtpToXnap>(NmGnbGtpToXnap::TARGET_FORWARDING_ALLOCATED);
    response->ueId = ueId;
    response->endpoints = std::move(endpoints);
    m_base->xnapTask->push(std::move(response));
    m_logger->info("Allocated Xn-U target forwarding for UE[%d] with %zu PDU session(s)", ueId,
                   allocatedSessionCount);
}

void GtpTask::handleActivateSourceForwarding(int ueId, const std::vector<XnForwardingEndpoint> &endpoints)
{
    if (m_udpServer == nullptr)
    {
        reportForwardingFailure(ueId, "GTP-U UDP server is not available for source Xn-U forwarding");
        return;
    }

    if (endpoints.empty())
    {
        reportForwardingFailure(ueId, "Handover Ack contains no Xn-U forwarding endpoint");
        return;
    }

    std::vector<std::pair<uint64_t, XnForwardingEndpoint>> prepared;
    prepared.reserve(endpoints.size());
    std::unordered_set<int> seenPsis;
    {
        const std::lock_guard<std::mutex> lock(m_pduSessionMutex);
        for (const auto &endpoint : endpoints)
        {
            if (endpoint.psi <= 0 || endpoint.tunnel.teid == 0 || endpoint.tunnel.address.length() == 0 ||
                endpoint.qosFlowIds.empty())
            {
                reportForwardingFailure(ueId, "Invalid Xn-U forwarding endpoint in Handover Ack");
                return;
            }
            if (!seenPsis.insert(endpoint.psi).second)
            {
                reportForwardingFailure(ueId, "Duplicate PDU session in Xn-U forwarding endpoints");
                return;
            }

            const uint64_t sessionInd = MakeSessionResInd(ueId, endpoint.psi);
            auto session = m_pduSessions.find(sessionInd);
            if (session == m_pduSessions.end())
            {
                reportForwardingFailure(ueId, "Source PDU session is missing for Xn-U forwarding");
                return;
            }

            std::unordered_set<int> sourceQfis;
            if (session->second->qosFlows)
            {
                const auto &flows = session->second->qosFlows->list;
                for (int i = 0; i < flows.count; ++i)
                {
                    if (flows.array[i] != nullptr)
                        sourceQfis.insert(static_cast<int>(flows.array[i]->qosFlowIdentifier));
                }
            }
            for (int qfi : endpoint.qosFlowIds)
            {
                if (qfi <= 0 || qfi > 63 || sourceQfis.count(qfi) == 0)
                {
                    reportForwardingFailure(ueId, "Xn-U forwarding QFI does not match the source PDU session");
                    return;
                }
            }

            prepared.emplace_back(sessionInd, endpoint);
        }
    }

    clearSourceForwardingForUe(ueId, false);
    for (auto &entry : prepared)
        m_sourceForwarding[entry.first] = std::move(entry.second);

    auto response = std::make_unique<NmGnbGtpToXnap>(NmGnbGtpToXnap::SOURCE_FORWARDING_READY);
    response->ueId = ueId;
    m_base->xnapTask->push(std::move(response));
    m_logger->info("Activated Xn-U source forwarding for UE[%d] with %zu PDU session(s)", ueId, prepared.size());
}

void GtpTask::handleCancelForwarding(int ueId)
{
    clearSourceForwardingForUe(ueId, true);
    clearTargetForwardingForUe(ueId);
    m_logger->debug("Cleared Xn-U forwarding state for UE[%d]", ueId);
}

void GtpTask::forwardDownlink(uint64_t sessionInd, const OctetString &pdu, int qfi)
{
    auto it = m_sourceForwarding.find(sessionInd);
    if (it == m_sourceForwarding.end() || m_udpServer == nullptr)
        return;

    const auto &endpoint = it->second;
    gtp::GtpMessage forwarded{};
    forwarded.msgType = gtp::GtpMessage::MT_G_PDU;
    forwarded.teid = endpoint.tunnel.teid;
    forwarded.payload = pdu.copy();

    if (!endpoint.qosFlowIds.empty())
    {
        auto dl = std::make_unique<gtp::DlPduSessionInformation>();
        dl->qfi = std::find(endpoint.qosFlowIds.begin(), endpoint.qosFlowIds.end(), qfi) != endpoint.qosFlowIds.end()
                      ? qfi
                      : endpoint.qosFlowIds.front();
        auto container = std::make_unique<gtp::PduSessionContainerExtHeader>();
        container->pduSessionInformation = std::move(dl);
        forwarded.extHeaders.push_back(std::move(container));
    }

    OctetString encoded;
    if (!gtp::EncodeGtpMessage(forwarded, encoded))
    {
        m_logger->err("Could not encode Xn-U forwarded packet for UE[%d] PSI[%d]", GetUeId(sessionInd),
                      GetPsi(sessionInd));
        return;
    }

    m_udpServer->send(InetAddress(endpoint.tunnel.address, cons::GtpPort), encoded);
}

void GtpTask::sendEndMarker(const XnForwardingEndpoint &endpoint)
{
    if (m_udpServer == nullptr || endpoint.tunnel.teid == 0 || endpoint.tunnel.address.length() == 0)
        return;

    gtp::GtpMessage marker{};
    marker.msgType = gtp::GtpMessage::MT_END_MARKER;
    marker.teid = endpoint.tunnel.teid;

    OctetString encoded;
    if (gtp::EncodeGtpMessage(marker, encoded))
        m_udpServer->send(InetAddress(endpoint.tunnel.address, cons::GtpPort), encoded);
}

void GtpTask::clearSourceForwardingForUe(int ueId, bool sendEndMarkers)
{
    for (auto it = m_sourceForwarding.begin(); it != m_sourceForwarding.end();)
    {
        if (GetUeId(it->first) != ueId)
        {
            ++it;
            continue;
        }
        if (sendEndMarkers)
            sendEndMarker(it->second);
        it = m_sourceForwarding.erase(it);
    }
}

void GtpTask::clearTargetForwardingForUe(int ueId)
{
    for (auto it = m_targetForwarding.begin(); it != m_targetForwarding.end();)
    {
        if (GetUeId(it->second) == ueId)
            it = m_targetForwarding.erase(it);
        else
            ++it;
    }
}

void GtpTask::clearForwardingForSession(uint64_t sessionInd)
{
    auto source = m_sourceForwarding.find(sessionInd);
    if (source != m_sourceForwarding.end())
    {
        sendEndMarker(source->second);
        m_sourceForwarding.erase(source);
    }

    for (auto it = m_targetForwarding.begin(); it != m_targetForwarding.end();)
    {
        if (it->second == sessionInd)
            it = m_targetForwarding.erase(it);
        else
            ++it;
    }
}

uint32_t GtpTask::allocateForwardingTeid()
{
    for (int attempt = 0; attempt < 65536; ++attempt)
    {
        uint32_t candidate = m_nextForwardingTeid++;
        if (candidate == 0)
        {
            m_nextForwardingTeid = 0xE0000000u;
            candidate = m_nextForwardingTeid++;
        }
        if (m_targetForwarding.count(candidate) == 0 && m_sessionTree.findByDownTeid(candidate) == 0)
            return candidate;
    }
    return 0;
}

void GtpTask::reportForwardingFailure(int ueId, const std::string &cause)
{
    if (m_base->xnapTask == nullptr)
        return;
    auto response = std::make_unique<NmGnbGtpToXnap>(NmGnbGtpToXnap::FORWARDING_FAILURE);
    response->ueId = ueId;
    response->cause = cause;
    m_base->xnapTask->push(std::move(response));
    m_logger->warn("Xn-U forwarding setup failed for UE[%d]: %s", ueId, cause.c_str());
}

void GtpTask::updateAmbrForUe(int ueId)
{
    if (!m_ueContexts.count(ueId))
        return;

    auto &ue = m_ueContexts[ueId];
    m_rateLimiter->updateUeUplinkLimit(ueId, ue->ueAmbr.ulAmbr);
    m_rateLimiter->updateUeDownlinkLimit(ueId, ue->ueAmbr.dlAmbr);
}

void GtpTask::updateAmbrForSession(uint64_t pduSession)
{
    AggregateMaximumBitRate sessionAmbr{};
    {
        const std::lock_guard<std::mutex> lock(m_pduSessionMutex);
        auto it = m_pduSessions.find(pduSession);
        if (it == m_pduSessions.end())
            return;
        sessionAmbr = it->second->sessionAmbr;
    }

    m_rateLimiter->updateSessionUplinkLimit(pduSession, sessionAmbr.ulAmbr);
    m_rateLimiter->updateSessionDownlinkLimit(pduSession, sessionAmbr.dlAmbr);
}

} // namespace nr::gnb
