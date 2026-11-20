//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include <optional>
#include <unordered_map>
#include <unordered_set>

#include <gnb/nts.hpp>
#include <gnb/types.hpp>
#include <lib/app/monitor.hpp>
#include <utils/logger.hpp>
#include <utils/nts.hpp>

extern "C"
{
    struct ASN_NGAP_NGAP_PDU;
    struct ASN_NGAP_NGSetupResponse;
    struct ASN_NGAP_NGSetupFailure;
    struct ASN_NGAP_ErrorIndication;
    struct ASN_NGAP_DownlinkNASTransport;
    struct ASN_NGAP_RerouteNASRequest;
    struct ASN_NGAP_PDUSessionResourceSetupRequest;
    struct ASN_NGAP_InitialContextSetupRequest;
    struct ASN_NGAP_UEContextReleaseCommand;
    struct ASN_NGAP_UEContextModificationRequest;
    struct ASN_NGAP_AMFConfigurationUpdate;
    struct ASN_NGAP_OverloadStart;
    struct ASN_NGAP_OverloadStop;
    struct ASN_NGAP_PDUSessionResourceReleaseCommand;
    struct ASN_NGAP_Paging;
    struct ASN_NGAP_HandoverRequest;
    struct ASN_NGAP_HandoverCommand;
    struct ASN_NGAP_HandoverPreparationFailure;
    struct ASN_NGAP_HandoverCancelAcknowledge;
    struct ASN_NGAP_PathSwitchRequestAcknowledge;
    struct ASN_NGAP_PathSwitchRequestFailure;
}

namespace nr::gnb
{

class SctpTask;
class GnbRrcTask;
class GtpTask;
class GnbAppTask;
class XnapTask;

class NgapTask : public NtsTask
{
  private:
    TaskBase *m_base;
    std::unique_ptr<Logger> m_logger;

    std::unordered_map<int, NgapAmfContext *> m_amfCtx;
    std::unordered_map<int, NgapUeContext *> m_ueCtx;
    int64_t m_ueNgapIdCounter;
    uint32_t m_downlinkTeidCounter;
    bool m_isInitialized;
    std::unordered_set<int> m_n2HandoverTargets;
    // UEs for which this gNB sent a Handover Required and that are still in that handover: erased on a Handover Preparation
    // Failure, a Handover Cancel Acknowledge, a failure to send the command (before the cancel is sent) and when the UE
    // context is deleted. A successful command removes nothing (RRC reports only failures), so the entry then stays until
    // the UE context is deleted. A failure to send the handover command cancels the handover only for UEs in this set.
    std::unordered_set<int> m_n2HandoverSources;

    friend class GnbCmdHandler;
    friend class XnapTask;

  public:
    explicit NgapTask(TaskBase *base);
    ~NgapTask() override = default;

  protected:
    void onStart() override;
    void onLoop() override;
    void onQuit() override;

  private:
    /* Utility functions */
    void createAmfContext(const GnbAmfConfig &config);
    NgapAmfContext *findAmfContext(int ctxId);
    void createUeContext(int ueId, int32_t requestedSliceType, const std::optional<GutiMobileIdentity> &sTmsi);
    NgapUeContext *findUeContext(int ctxId);
    NgapUeContext *findUeByRanId(int64_t ranUeNgapId);
    NgapUeContext *findUeByAmfId(int64_t amfUeNgapId);
    NgapUeContext *findUeByNgapIdPair(int amfCtxId, const NgapIdPair &idPair);
    void deleteUeContext(int ueId);
    void deleteAmfContext(int amfId);
    void releaseHandoverSource(int ueId);
    void prepareHandoverTarget(int ueId, const std::optional<int64_t> &amfUeNgapId,
                               std::unique_ptr<XnHandoverNgapSnapshot> &&snapshot);
    void notifyTargetPrepareResult(int ueId, bool success, const std::string &cause = {});

    /* Interface management */
    void handleAssociationSetup(int amfId, int ascId, int inCount, int outCount);
    void handleAssociationShutdown(int amfId);
    void sendNgSetupRequest(int amfId);
    void sendErrorIndication(int amfId, NgapCause cause = NgapCause::Protocol_unspecified, int ueId = 0);
    void receiveNgSetupResponse(int amfId, ASN_NGAP_NGSetupResponse *msg);
    void receiveNgSetupFailure(int amfId, ASN_NGAP_NGSetupFailure *msg);
    void receiveErrorIndication(int amfId, ASN_NGAP_ErrorIndication *msg);
    void receiveAmfConfigurationUpdate(int amfId, ASN_NGAP_AMFConfigurationUpdate *msg);
    void receiveOverloadStart(int amfId, ASN_NGAP_OverloadStart *msg);
    void receiveOverloadStop(int amfId, ASN_NGAP_OverloadStop *msg);

    /* Message transport */
    void sendNgapNonUe(int amfId, ASN_NGAP_NGAP_PDU *pdu);
    // False (and logged) when the UE or its AMF is unknown and nothing was sent; the PDU is freed either way.
    bool sendNgapUeAssociated(int ueId, ASN_NGAP_NGAP_PDU *pdu);
    void handleSctpMessage(int amfId, uint16_t stream, const UniqueBuffer &buffer);
    bool handleSctpStreamId(int amfId, int stream, const ASN_NGAP_NGAP_PDU &pdu);

    /* NAS transport */
    void handleInitialNasTransport(int ueId, OctetString &nasPdu, int64_t rrcEstablishmentCause,
                                   const std::optional<GutiMobileIdentity> &sTmsi);
    void handleUplinkNasTransport(int ueId, const OctetString &nasPdu);
    void receiveDownlinkNasTransport(int amfId, ASN_NGAP_DownlinkNASTransport *msg);
    void deliverDownlinkNas(int ueId, OctetString &&nasPdu);
    void sendNasNonDeliveryIndication(int ueId, const OctetString &nasPdu, NgapCause cause);
    void receiveRerouteNasRequest(int amfId, ASN_NGAP_RerouteNASRequest *msg);

    /* PDU session management */
    void receiveSessionResourceSetupRequest(int amfId, ASN_NGAP_PDUSessionResourceSetupRequest *msg);
    void receiveSessionResourceReleaseCommand(int amfId, ASN_NGAP_PDUSessionResourceReleaseCommand *msg);
    std::optional<NgapCause> setupPduSessionResource(NgapUeContext *ue, PduSessionResource *resource);

    /* Xn handover path switch */
    void sendPathSwitchRequest(int ueId);
    void receivePathSwitchRequestAcknowledge(int amfId, ASN_NGAP_PathSwitchRequestAcknowledge *msg);
    void receivePathSwitchRequestFailure(int amfId, ASN_NGAP_PathSwitchRequestFailure *msg);
    void notifyPathSwitchResult(int ueId, bool success, const std::string &cause = {});

    /* N2 handover */
    bool sendHandoverRequired(int ueId, int gnbTargetId, std::string &error);
    // Tells the AMF that the handover of this UE is cancelled (the handover command could not be sent to the UE).
    void sendHandoverCancel(int ueId);
    void releaseFailedTarget(int ueId);
    void notifyPathSwitchUnusable(int ueId, const std::string &cause);
    void receiveHandoverRequest(int amfId, ASN_NGAP_HandoverRequest *msg);
    void receiveHandoverCommand(int amfId, ASN_NGAP_HandoverCommand *msg);
    void handleHandoverConfirm(int ueId);
    void sendHandoverNotify(int ueId);
    void receiveHandoverPreparationFailure(int amfId, ASN_NGAP_HandoverPreparationFailure *msg);
    void receiveHandoverCancelAcknowledge(int amfId, ASN_NGAP_HandoverCancelAcknowledge *msg);

    /* UE context management */
    void receiveInitialContextSetup(int amfId, ASN_NGAP_InitialContextSetupRequest *msg);
    void receiveContextRelease(int amfId, ASN_NGAP_UEContextReleaseCommand *msg);
    void receiveContextModification(int amfId, ASN_NGAP_UEContextModificationRequest *msg);
    // False when the request could not be sent (see sendNgapUeAssociated).
    bool sendContextRelease(int ueId, NgapCause cause);

    /* NAS Node Selection */
    NgapAmfContext *selectAmf(int ueId, int32_t requestedSliceType, const std::optional<GutiMobileIdentity> &sTmsi);
    NgapAmfContext *selectNewAmfForReAllocation(int ueId, int initiatedAmfId, int amfSetId);

    /* Radio resource control */
    void handleRadioLinkFailure(int ueId);
    void receivePaging(int amfId, ASN_NGAP_Paging *msg);
};

} // namespace nr::gnb
