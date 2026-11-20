//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "task.hpp"

#include <gnb/gtp/task.hpp>
#include <gnb/ngap/utils.hpp>
#include <gnb/rrc/task.hpp>
#include <gnb/xnap/task.hpp>
#include <utils/common.hpp>

#include <asn/ngap/ASN_NGAP_QosFlowSetupRequestItem.h>
#include <asn/ngap/ASN_NGAP_QosFlowSetupRequestList.h>

namespace nr::gnb
{

NgapAmfContext *NgapTask::findAmfContext(int ctxId)
{
    NgapAmfContext *ctx = nullptr;
    if (m_amfCtx.count(ctxId))
        ctx = m_amfCtx[ctxId];
    if (ctx == nullptr)
        m_logger->err("AMF context not found with id: %d", ctxId);
    return ctx;
}

void NgapTask::createAmfContext(const GnbAmfConfig &conf)
{
    auto *ctx = new NgapAmfContext();
    ctx->ctxId = utils::NextId();
    ctx->state = EAmfState::NOT_CONNECTED;
    ctx->address = conf.address;
    ctx->port = conf.port;
    m_amfCtx[ctx->ctxId] = ctx;
}

void NgapTask::createUeContext(int ueId, int32_t requestedSliceType, const std::optional<GutiMobileIdentity> &sTmsi)
{
    auto *ctx = new NgapUeContext(ueId);
    ctx->amfUeNgapId = -1;
    ctx->ranUeNgapId = ++m_ueNgapIdCounter;

    m_ueCtx[ctx->ctxId] = ctx;

    // Perform AMF selection
    auto *amf = selectAmf(ueId, requestedSliceType, sTmsi);
    if (amf == nullptr)
        m_logger->err("AMF selection for UE[%d] failed. Could not find a suitable AMF.", ueId);
    else
        ctx->associatedAmfId = amf->ctxId;
}

NgapUeContext *NgapTask::findUeContext(int ctxId)
{
    NgapUeContext *ctx = nullptr;
    if (m_ueCtx.count(ctxId))
        ctx = m_ueCtx[ctxId];
    if (ctx == nullptr)
        m_logger->err("UE context not found with id: %d", ctxId);
    return ctx;
}

NgapUeContext *NgapTask::findUeByRanId(int64_t ranUeNgapId)
{
    if (ranUeNgapId <= 0)
        return nullptr;
    // TODO: optimize
    for (auto &ue : m_ueCtx)
        if (ue.second->ranUeNgapId == ranUeNgapId)
            return ue.second;
    return nullptr;
}

NgapUeContext *NgapTask::findUeByAmfId(int64_t amfUeNgapId)
{
    if (amfUeNgapId <= 0)
        return nullptr;
    // TODO: optimize
    for (auto &ue : m_ueCtx)
        if (ue.second->amfUeNgapId == amfUeNgapId)
            return ue.second;
    return nullptr;
}

NgapUeContext *NgapTask::findUeByNgapIdPair(int amfCtxId, const NgapIdPair &idPair)
{
    auto &amfId = idPair.amfUeNgapId;
    auto &ranId = idPair.ranUeNgapId;

    if (!amfId.has_value() && !ranId.has_value())
    {
        sendErrorIndication(amfCtxId, NgapCause::Protocol_abstract_syntax_error_falsely_constructed_message);
        return nullptr;
    }

    if (!amfId.has_value())
    {
        auto ue = findUeByRanId(ranId.value());
        if (ue == nullptr)
        {
            sendErrorIndication(amfCtxId, NgapCause::RadioNetwork_unknown_local_UE_NGAP_ID);
            return nullptr;
        }

        return ue;
    }

    if (!ranId.has_value())
    {
        auto ue = findUeByAmfId(amfId.value());
        if (ue == nullptr)
        {
            sendErrorIndication(amfCtxId, NgapCause::RadioNetwork_inconsistent_remote_UE_NGAP_ID);
            return nullptr;
        }

        return ue;
    }

    auto ue = findUeByRanId(ranId.value());
    if (ue == nullptr)
    {
        sendErrorIndication(amfCtxId, NgapCause::RadioNetwork_unknown_local_UE_NGAP_ID);
        return nullptr;
    }

    if (ue->amfUeNgapId == -1)
        ue->amfUeNgapId = amfId.value();
    else if (ue->amfUeNgapId != amfId.value())
    {
        sendErrorIndication(amfCtxId, NgapCause::RadioNetwork_inconsistent_remote_UE_NGAP_ID);
        return nullptr;
    }

    return ue;
}

void NgapTask::deleteUeContext(int ueId)
{
    m_n2HandoverSources.erase(ueId);
    m_n2HandoverTargets.erase(ueId);
    const auto it = m_ueCtx.find(ueId);
    if (it == m_ueCtx.end())
        return;
    delete it->second;
    m_ueCtx.erase(it);
}

void NgapTask::deleteAmfContext(int amfId)
{
    auto *amf = m_amfCtx[amfId];
    if (amf)
    {
        delete amf;
        m_amfCtx.erase(amfId);
    }
}

void NgapTask::releaseHandoverSource(int ueId)
{
    auto it = m_ueCtx.find(ueId);
    if (it == m_ueCtx.end() || it->second == nullptr)
    {
        m_logger->debug("Source NGAP UE context already absent for UE[%d]", ueId);
        return;
    }

    // Releases the NGAP/GTP context of a UE without signalling the AMF. On the source this is the cleanup after the target
    // switched the path (the AMF now owns the target path); on the target it removes the context of a handover that was
    // dropped before the UE attached, which the AMF never heard of.
    auto release = std::make_unique<NmGnbNgapToGtp>(NmGnbNgapToGtp::UE_CONTEXT_RELEASE);
    release->ueId = ueId;
    if (m_base->gtpTask != nullptr)
        m_base->gtpTask->push(std::move(release));

    deleteUeContext(ueId);
    m_logger->debug("Released NGAP/GTP context of UE[%d] without AMF signalling (Xn handover cleanup)", ueId);
}

void NgapTask::releaseFailedTarget(int ueId)
{
    // The AMF rejected the Path Switch, so it never moved the UE to this gNB: release the UE connection and the local
    // context. (If the AMF accepted the switch, the AMF's own release command does this instead.) Without an NGAP context
    // the UE was already released (for example by that command), so nothing is sent to RRC either.
    const auto found = m_ueCtx.find(ueId);
    if (found == m_ueCtx.end() || found->second == nullptr)
    {
        m_logger->debug("UE[%d] is already released, nothing to release after the failed Path Switch", ueId);
        return;
    }
    auto toRrc = std::make_unique<NmGnbNgapToRrc>(NmGnbNgapToRrc::AN_RELEASE);
    toRrc->ueId = ueId;
    m_base->rrcTask->push(std::move(toRrc));
    releaseHandoverSource(ueId);
}

void NgapTask::notifyTargetPrepareResult(int ueId, bool success, const std::string &cause)
{
    if (m_base->xnapTask == nullptr)
    {
        m_logger->warn("Cannot report target NGAP preparation result for UE[%d]: XnAP task is not available", ueId);
        return;
    }

    auto w = std::make_unique<NmGnbNgapToXnap>(success ? NmGnbNgapToXnap::TARGET_PREPARE_ACK
                                                       : NmGnbNgapToXnap::TARGET_PREPARE_FAILURE);
    w->ueId = ueId;
    w->cause = cause;
    m_base->xnapTask->push(std::move(w));
}

void NgapTask::prepareHandoverTarget(int ueId, const std::optional<int64_t> &amfUeNgapId,
                                     std::unique_ptr<XnHandoverNgapSnapshot> &&snapshot)
{
    if (ueId <= 0)
    {
        notifyTargetPrepareResult(ueId, false, "Invalid UE ID");
        return;
    }

    if (snapshot == nullptr)
    {
        notifyTargetPrepareResult(ueId, false, "Target handover NGAP snapshot is missing");
        return;
    }
    if (!snapshot->securityCapabilities.valid)
    {
        notifyTargetPrepareResult(ueId, false, "Target handover security capabilities are missing");
        return;
    }
    if (snapshot->pduSessions.empty())
    {
        notifyTargetPrepareResult(ueId, false, "Target handover has no PDU session snapshot");
        return;
    }
    if (m_base->gtpTask == nullptr)
    {
        notifyTargetPrepareResult(ueId, false, "GTP task is not available");
        return;
    }

    std::set<int> seenPsi;
    for (const auto &session : snapshot->pduSessions)
    {
        if (session.psi <= 0 || !seenPsi.insert(session.psi).second)
        {
            notifyTargetPrepareResult(ueId, false, "Target handover contains invalid or duplicate PDU Session ID");
            return;
        }
        if (!ngap_utils::IsSupportedPduSessionType(session.sessionType))
        {
            notifyTargetPrepareResult(ueId, false, "Target handover supports IPv4 and Ethernet PDU sessions only");
            return;
        }
        if (session.upTunnel.teid == 0 || session.upTunnel.address.length() == 0)
        {
            notifyTargetPrepareResult(ueId, false, "Target handover PDU session is missing UPF tunnel information");
            return;
        }
        if (session.qosFlowIds.empty())
        {
            notifyTargetPrepareResult(ueId, false, "Target handover PDU session has no QoS flows");
            return;
        }
    }

    auto existingIt = m_ueCtx.find(ueId);
    auto *existing = existingIt == m_ueCtx.end() ? nullptr : existingIt->second;
    if (existing != nullptr)
    {
        auto *amf = findAmfContext(existing->associatedAmfId);
        if (amf == nullptr || amf->state != EAmfState::CONNECTED)
        {
            notifyTargetPrepareResult(ueId, false, "Existing target UE context has no connected AMF");
            return;
        }

        notifyTargetPrepareResult(ueId, false, "Target NGAP UE context already exists");
        return;
    }

    auto *amf = selectAmf(ueId, -1, std::nullopt);
    if (amf == nullptr || amf->state != EAmfState::CONNECTED)
    {
        notifyTargetPrepareResult(ueId, false, "No connected AMF is available for target UE context");
        return;
    }

    auto *ctx = new NgapUeContext(ueId);
    ctx->ranUeNgapId = ++m_ueNgapIdCounter;
    ctx->associatedAmfId = amf->ctxId;
    ctx->amfUeNgapId = amfUeNgapId.value_or(-1);
    ctx->ueAmbr = snapshot->ueAmbr;
    ctx->securityCapabilities = snapshot->securityCapabilities;

    if (amf->association.outStreams > 0)
    {
        amf->nextStream = (amf->nextStream + 1) % amf->association.outStreams;
        if (amf->nextStream == 0 && amf->association.outStreams > 1)
            amf->nextStream = 1;
        ctx->uplinkStream = amf->nextStream;
    }

    m_ueCtx[ueId] = ctx;

    auto gtpUe = std::make_unique<NmGnbNgapToGtp>(NmGnbNgapToGtp::UE_CONTEXT_UPDATE);
    gtpUe->update = std::make_unique<GtpUeContextUpdate>(true, ueId, ctx->ueAmbr);
    m_base->gtpTask->push(std::move(gtpUe));

    for (auto &sessionSnapshot : snapshot->pduSessions)
    {
        auto *resource = new PduSessionResource(ueId, sessionSnapshot.psi);
        resource->sessionType = sessionSnapshot.sessionType;
        resource->sessionAmbr = sessionSnapshot.sessionAmbr;
        resource->upTunnel.teid = sessionSnapshot.upTunnel.teid;
        resource->upTunnel.address = sessionSnapshot.upTunnel.address.copy();

        auto *qosFlows = asn::New<ASN_NGAP_QosFlowSetupRequestList>();
        for (int qfi : sessionSnapshot.qosFlowIds)
        {
            auto *flow = asn::New<ASN_NGAP_QosFlowSetupRequestItem>();
            flow->qosFlowIdentifier = qfi;
            asn::SequenceAdd(*qosFlows, flow);
        }
        resource->qosFlows = asn::WrapUnique(qosFlows, asn_DEF_ASN_NGAP_QosFlowSetupRequestList);

        auto error = setupPduSessionResource(ctx, resource);
        if (error.has_value())
        {
            delete resource;
            auto release = std::make_unique<NmGnbNgapToGtp>(NmGnbNgapToGtp::UE_CONTEXT_RELEASE);
            release->ueId = ueId;
            m_base->gtpTask->push(std::move(release));
            deleteUeContext(ueId);
            notifyTargetPrepareResult(ueId, false, "Failed to create target PDU session resources");
            return;
        }
    }

    m_logger->debug("Prepared target NGAP UE context UE[%d] RAN-UE-NGAP-ID[%ld] AMF[%d]", ueId,
                    ctx->ranUeNgapId, amf->ctxId);
    // 等 GTP task 依序處理完前面的 session 建立工作，再通知 XnAP 準備完成。
    auto ready = std::make_unique<NmGnbNgapToGtp>(NmGnbNgapToGtp::XN_TARGET_PREPARE_COMPLETE);
    ready->ueId = ueId;
    m_base->gtpTask->push(std::move(ready));
}

} // namespace nr::gnb
