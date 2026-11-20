//
// This file is a part of UERANSIM project.
// Copyright (c) 2023 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "encode.hpp"
#include "task.hpp"
#include "utils.hpp"

#include <sstream>

#include <gnb/gtp/task.hpp>
#include <gnb/xnap/task.hpp>
#include <lib/asn/ngap.hpp>
#include <lib/asn/utils.hpp>

#include <asn/ngap/ASN_NGAP_GTPTunnel.h>
#include <asn/ngap/ASN_NGAP_PathSwitchRequest.h>
#include <asn/ngap/ASN_NGAP_PathSwitchRequestAcknowledge.h>
#include <asn/ngap/ASN_NGAP_PathSwitchRequestAcknowledgeTransfer.h>
#include <asn/ngap/ASN_NGAP_PathSwitchRequestFailure.h>
#include <asn/ngap/ASN_NGAP_PathSwitchRequestTransfer.h>
#include <asn/ngap/ASN_NGAP_PathSwitchRequestUnsuccessfulTransfer.h>
#include <asn/ngap/ASN_NGAP_PDUSessionResourceReleasedItemPSAck.h>
#include <asn/ngap/ASN_NGAP_PDUSessionResourceReleasedItemPSFail.h>
#include <asn/ngap/ASN_NGAP_PDUSessionResourceSwitchedItem.h>
#include <asn/ngap/ASN_NGAP_PDUSessionResourceToBeSwitchedDLItem.h>
#include <asn/ngap/ASN_NGAP_ProtocolIE-Field.h>
#include <asn/ngap/ASN_NGAP_ProtocolIE-ID.h>
#include <asn/ngap/ASN_NGAP_QosFlowAcceptedItem.h>
#include <asn/ngap/ASN_NGAP_UESecurityCapabilities.h>
#include <asn/ngap/ASN_NGAP_UPTransportLayerInformation.h>

namespace nr::gnb
{

void NgapTask::notifyPathSwitchResult(int ueId, bool success, const std::string &cause)
{
    if (m_base->xnapTask == nullptr)
    {
        m_logger->warn("Cannot report Path Switch result for UE[%d]: XnAP task is not available", ueId);
        return;
    }

    auto w = std::make_unique<NmGnbNgapToXnap>(success ? NmGnbNgapToXnap::PATH_SWITCH_ACK
                                                       : NmGnbNgapToXnap::PATH_SWITCH_FAILURE);
    w->ueId = ueId;
    w->cause = cause;
    m_base->xnapTask->push(std::move(w));
}

void NgapTask::notifyPathSwitchUnusable(int ueId, const std::string &cause)
{
    if (m_base->xnapTask == nullptr)
        return;
    auto w = std::make_unique<NmGnbNgapToXnap>(NmGnbNgapToXnap::PATH_SWITCH_UNUSABLE);
    w->ueId = ueId;
    w->cause = cause;
    m_base->xnapTask->push(std::move(w));
}

void NgapTask::sendPathSwitchRequest(int ueId)
{
    auto *ue = findUeContext(ueId);
    if (ue == nullptr)
    {
        notifyPathSwitchResult(ueId, false, "NGAP UE context not found");
        return;
    }

    if (ue->amfUeNgapId <= 0)
    {
        notifyPathSwitchResult(ueId, false, "AMF UE NGAP ID is not assigned");
        return;
    }

    auto *amf = findAmfContext(ue->associatedAmfId);
    if (amf == nullptr || amf->state != EAmfState::CONNECTED)
    {
        notifyPathSwitchResult(ueId, false, "Associated AMF is not connected");
        return;
    }

    if (!ue->securityCapabilities.valid)
    {
        notifyPathSwitchResult(ueId, false, "UE security capabilities are unavailable");
        return;
    }

    if (ue->pduSessions.empty())
    {
        notifyPathSwitchResult(ueId, false, "UE has no active PDU sessions");
        return;
    }

    if (m_base->gtpTask == nullptr)
    {
        notifyPathSwitchResult(ueId, false, "GTP task is not available");
        return;
    }

    auto sessions = m_base->gtpTask->sessionSnapshot(ueId);
    if (sessions.empty())
    {
        notifyPathSwitchResult(ueId, false, "No GTP session resources found");
        return;
    }

    std::vector<ASN_NGAP_PathSwitchRequestIEs *> ies;

    auto *sourceAmfIe = asn::New<ASN_NGAP_PathSwitchRequestIEs>();
    sourceAmfIe->id = ASN_NGAP_ProtocolIE_ID_id_SourceAMF_UE_NGAP_ID;
    sourceAmfIe->criticality = ASN_NGAP_Criticality_reject;
    sourceAmfIe->value.present = ASN_NGAP_PathSwitchRequestIEs__value_PR_AMF_UE_NGAP_ID;
    asn::SetSigned64(ue->amfUeNgapId, sourceAmfIe->value.choice.AMF_UE_NGAP_ID);
    ies.push_back(sourceAmfIe);

    auto *securityIe = asn::New<ASN_NGAP_PathSwitchRequestIEs>();
    securityIe->id = ASN_NGAP_ProtocolIE_ID_id_UESecurityCapabilities;
    securityIe->criticality = ASN_NGAP_Criticality_ignore;
    securityIe->value.present = ASN_NGAP_PathSwitchRequestIEs__value_PR_UESecurityCapabilities;
    auto &security = securityIe->value.choice.UESecurityCapabilities;
    asn::SetBitStringInt<16>(ue->securityCapabilities.nrEncryptionAlgorithms, security.nRencryptionAlgorithms);
    asn::SetBitStringInt<16>(ue->securityCapabilities.nrIntegrityProtectionAlgorithms,
                             security.nRintegrityProtectionAlgorithms);
    asn::SetBitStringInt<16>(ue->securityCapabilities.eutraEncryptionAlgorithms,
                             security.eUTRAencryptionAlgorithms);
    asn::SetBitStringInt<16>(ue->securityCapabilities.eutraIntegrityProtectionAlgorithms,
                             security.eUTRAintegrityProtectionAlgorithms);
    ies.push_back(securityIe);

    auto *sessionListIe = asn::New<ASN_NGAP_PathSwitchRequestIEs>();
    sessionListIe->id = ASN_NGAP_ProtocolIE_ID_id_PDUSessionResourceToBeSwitchedDLList;
    sessionListIe->criticality = ASN_NGAP_Criticality_reject;
    sessionListIe->value.present = ASN_NGAP_PathSwitchRequestIEs__value_PR_PDUSessionResourceToBeSwitchedDLList;

    std::set<int> includedSessions;
    for (const auto &session : sessions)
    {
        if (ue->pduSessions.count(session.psi) == 0)
            continue;
        if (session.downTunnel.teid == 0 || session.downTunnel.address.length() == 0 || session.qosFlowIds.empty())
        {
            std::stringstream ss;
            ss << "Incomplete GTP resource for PDU session " << session.psi;
            asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestIEs, sourceAmfIe);
            asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestIEs, securityIe);
            asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestIEs, sessionListIe);
            notifyPathSwitchResult(ueId, false, ss.str());
            return;
        }

        auto *transfer = asn::New<ASN_NGAP_PathSwitchRequestTransfer>();
        transfer->dL_NGU_UP_TNLInformation.present = ASN_NGAP_UPTransportLayerInformation_PR_gTPTunnel;
        transfer->dL_NGU_UP_TNLInformation.choice.gTPTunnel = asn::New<ASN_NGAP_GTPTunnel>();
        asn::SetBitString(transfer->dL_NGU_UP_TNLInformation.choice.gTPTunnel->transportLayerAddress,
                          session.downTunnel.address);
        asn::SetOctetString4(transfer->dL_NGU_UP_TNLInformation.choice.gTPTunnel->gTP_TEID,
                             static_cast<octet4>(session.downTunnel.teid));

        for (int qfi : session.qosFlowIds)
        {
            auto *accepted = asn::New<ASN_NGAP_QosFlowAcceptedItem>();
            accepted->qosFlowIdentifier = qfi;
            asn::SequenceAdd(transfer->qosFlowAcceptedList, accepted);
        }

        OctetString encodedTransfer = ngap_encode::EncodeS(asn_DEF_ASN_NGAP_PathSwitchRequestTransfer, transfer);
        asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestTransfer, transfer);
        if (encodedTransfer.length() == 0)
        {
            asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestIEs, sourceAmfIe);
            asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestIEs, securityIe);
            asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestIEs, sessionListIe);
            notifyPathSwitchResult(ueId, false, "PathSwitchRequestTransfer encoding failed");
            return;
        }

        auto *item = asn::New<ASN_NGAP_PDUSessionResourceToBeSwitchedDLItem>();
        item->pDUSessionID = session.psi;
        asn::SetOctetString(item->pathSwitchRequestTransfer, encodedTransfer);
        asn::SequenceAdd(sessionListIe->value.choice.PDUSessionResourceToBeSwitchedDLList, item);
        includedSessions.insert(session.psi);
    }

    if (includedSessions != ue->pduSessions)
    {
        asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestIEs, sourceAmfIe);
        asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestIEs, securityIe);
        asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestIEs, sessionListIe);
        notifyPathSwitchResult(ueId, false, "NGAP and GTP PDU session state is inconsistent");
        return;
    }
    ies.push_back(sessionListIe);

    auto *pdu = asn::ngap::NewMessagePdu<ASN_NGAP_PathSwitchRequest>(ies);
    m_logger->debug("Sending Path Switch Request for UE[%d] with %zu PDU session(s)", ueId, includedSessions.size());
    if (!sendNgapUeAssociated(ueId, pdu))
        notifyPathSwitchResult(ueId, false, "The Path Switch Request could not be sent to the AMF");
}

void NgapTask::receivePathSwitchRequestAcknowledge(int amfId, ASN_NGAP_PathSwitchRequestAcknowledge *msg)
{
    auto *ue = findUeByNgapIdPair(amfId, ngap_utils::FindNgapIdPair(msg));
    if (ue == nullptr)
        return;

    // An acknowledgement the target can not use must not be reported as a success: that makes the source release the UE
    // while the target would still send uplink data to the old UPF tunnel.
    auto failPathSwitch = [&](const std::string &cause) {
        m_logger->err("Path Switch Request Acknowledge for UE[%d] is unusable: %s", ue->ctxId, cause.c_str());
        // The AMF accepted the path switch, so it has to be told that this gNB drops the UE. Its release command then
        // releases the RRC, GTP and NGAP contexts and is answered; XnAP only drops its record meanwhile.
        if (sendContextRelease(ue->ctxId, NgapCause::RadioNetwork_unspecified))
            notifyPathSwitchUnusable(ue->ctxId, cause);
        else
            notifyPathSwitchResult(ue->ctxId, false, cause); // no release command will come: release locally
    };

    auto *switchedIe = asn::ngap::GetProtocolIe(msg, ASN_NGAP_ProtocolIE_ID_id_PDUSessionResourceSwitchedList);
    if (switchedIe == nullptr || switchedIe->PDUSessionResourceSwitchedList.list.count == 0)
    {
        failPathSwitch("Acknowledge lists no switched PDU session");
        return;
    }

    auto &list = switchedIe->PDUSessionResourceSwitchedList.list;
    for (int i = 0; i < list.count; ++i)
    {
        auto *item = list.array[i];
        if (item == nullptr)
        {
            failPathSwitch("Acknowledge has an empty switched PDU session item");
            return;
        }

        auto *transfer = ngap_encode::Decode<ASN_NGAP_PathSwitchRequestAcknowledgeTransfer>(
            asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer, item->pathSwitchRequestAcknowledgeTransfer);
        if (transfer == nullptr)
        {
            failPathSwitch("Acknowledge transfer of PDU session " + std::to_string(item->pDUSessionID) +
                           " can not be decoded");
            return;
        }

        const bool hasTunnel =
            transfer->uL_NGU_UP_TNLInformation != nullptr &&
            transfer->uL_NGU_UP_TNLInformation->present == ASN_NGAP_UPTransportLayerInformation_PR_gTPTunnel &&
            transfer->uL_NGU_UP_TNLInformation->choice.gTPTunnel != nullptr;
        // The uplink tunnel is optional in the acknowledgement transfer: without it the UPF tunnel stays as it is.
        bool updated = !hasTunnel;
        if (hasTunnel)
        {
            OctetString address =
                asn::GetOctetString(transfer->uL_NGU_UP_TNLInformation->choice.gTPTunnel->transportLayerAddress);
            uint32_t teid = static_cast<uint32_t>(
                asn::GetOctet4(transfer->uL_NGU_UP_TNLInformation->choice.gTPTunnel->gTP_TEID));
            updated = teid != 0 && m_base->gtpTask != nullptr &&
                      m_base->gtpTask->updateSessionUplinkTunnel(ue->ctxId, static_cast<int>(item->pDUSessionID),
                                                                teid, address);
        }
        asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestAcknowledgeTransfer, transfer);
        if (!updated)
        {
            failPathSwitch("The uplink tunnel of PDU session " + std::to_string(item->pDUSessionID) +
                           " can not be applied");
            return;
        }
    }

    std::stringstream detail;
    bool firstReleased = true;
    auto *releasedIe = asn::ngap::GetProtocolIe(msg, ASN_NGAP_ProtocolIE_ID_id_PDUSessionResourceReleasedListPSAck);
    if (releasedIe)
    {
        auto &list = releasedIe->PDUSessionResourceReleasedListPSAck.list;
        if (list.count > 0)
            detail << "AMF released PDU session(s): ";

        for (int i = 0; i < list.count; ++i)
        {
            auto *item = list.array[i];
            if (item == nullptr)
                continue;
            if (!firstReleased)
                detail << ",";
            detail << item->pDUSessionID;
            firstReleased = false;

            ue->pduSessions.erase(static_cast<int>(item->pDUSessionID));
            if (m_base->gtpTask != nullptr)
            {
                auto release = std::make_unique<NmGnbNgapToGtp>(NmGnbNgapToGtp::SESSION_RELEASE);
                release->ueId = ue->ctxId;
                release->psi = static_cast<int>(item->pDUSessionID);
                m_base->gtpTask->push(std::move(release));
            }
        }
    }

    m_logger->debug("Path Switch Request Acknowledge received for UE[%d]", ue->ctxId);
    notifyPathSwitchResult(ue->ctxId, true, detail.str());
}

void NgapTask::receivePathSwitchRequestFailure(int amfId, ASN_NGAP_PathSwitchRequestFailure *msg)
{
    auto *ue = findUeByNgapIdPair(amfId, ngap_utils::FindNgapIdPair(msg));
    if (ue == nullptr)
        return;

    std::string cause = "Path Switch Request rejected by AMF";
    auto *releasedIe = asn::ngap::GetProtocolIe(msg, ASN_NGAP_ProtocolIE_ID_id_PDUSessionResourceReleasedListPSFail);
    if (releasedIe)
    {
        auto &list = releasedIe->PDUSessionResourceReleasedListPSFail.list;
        for (int i = 0; i < list.count; ++i)
        {
            auto *item = list.array[i];
            if (item == nullptr)
                continue;

            auto *transfer = ngap_encode::Decode<ASN_NGAP_PathSwitchRequestUnsuccessfulTransfer>(
                asn_DEF_ASN_NGAP_PathSwitchRequestUnsuccessfulTransfer, item->pathSwitchRequestUnsuccessfulTransfer);
            if (transfer != nullptr)
            {
                cause = ngap_utils::CauseToString(transfer->cause);
                asn::Free(asn_DEF_ASN_NGAP_PathSwitchRequestUnsuccessfulTransfer, transfer);
                break;
            }
        }
    }

    m_logger->warn("Path Switch Request Failure received for UE[%d]: %s", ue->ctxId, cause.c_str());
    notifyPathSwitchResult(ue->ctxId, false, cause);
}

} // namespace nr::gnb
