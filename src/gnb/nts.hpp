//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include "types.hpp"

#include <utility>

#include <lib/app/cli_base.hpp>
#include <lib/app/cli_cmd.hpp>
#include <lib/asn/utils.hpp>
#include <lib/rls/rls_base.hpp>
#include <lib/rrc/rrc.hpp>
#include <lib/sctp/sctp.hpp>
#include <utils/network.hpp>
#include <utils/nts.hpp>
#include <utils/octet_string.hpp>
#include <utils/unique_buffer.hpp>

extern "C"
{
    struct ASN_NGAP_FiveG_S_TMSI;
    struct ASN_NGAP_TAIListForPaging;
}

namespace nr::gnb
{

struct NmGnbRlsToRrc : NtsMessage
{
    enum PR
    {
        SIGNAL_DETECTED,
        SIGNAL_LOST,
        UPLINK_RRC,
    } present;

    // SIGNAL_DETECTED
    // SIGNAL_LOST
    // UPLINK_RRC
    int ueId{};

    // UPLINK_RRC
    OctetString data;
    rrc::RrcChannel rrcChannel{};

    explicit NmGnbRlsToRrc(PR present) : NtsMessage(NtsMessageType::GNB_RLS_TO_RRC), present(present)
    {
    }
};

struct NmGnbRlsToGtp : NtsMessage
{
    enum PR
    {
        DATA_PDU_DELIVERY,
    } present;

    // DATA_PDU_DELIVERY
    int ueId{};
    int psi{};
    OctetString pdu;

    explicit NmGnbRlsToGtp(PR present) : NtsMessage(NtsMessageType::GNB_RLS_TO_GTP), present(present)
    {
    }
};

struct NmGnbGtpToRls : NtsMessage
{
    enum PR
    {
        DATA_PDU_DELIVERY,
    } present;

    // DATA_PDU_DELIVERY
    int ueId{};
    int psi{};
    OctetString pdu{};

    explicit NmGnbGtpToRls(PR present) : NtsMessage(NtsMessageType::GNB_GTP_TO_RLS), present(present)
    {
    }
};

struct NmGnbRlsToRls : NtsMessage
{
    enum PR
    {
        SIGNAL_DETECTED,
        SIGNAL_LOST,
        RECEIVE_RLS_MESSAGE,
        DOWNLINK_RRC,
        DOWNLINK_DATA,
        UPLINK_RRC,
        UPLINK_DATA,
        RADIO_LINK_FAILURE,
        TRANSMISSION_FAILURE,
    } present;

    // SIGNAL_DETECTED
    // SIGNAL_LOST
    // DOWNLINK_RRC
    // DOWNLINK_DATA
    // UPLINK_DATA
    // UPLINK_RRC
    int ueId{};

    // RECEIVE_RLS_MESSAGE
    std::unique_ptr<rls::RlsMessage> msg{};

    // DOWNLINK_DATA
    // UPLINK_DATA
    int psi{};

    // DOWNLINK_DATA
    // DOWNLINK_RRC
    // UPLINK_DATA
    // UPLINK_RRC
    OctetString data;

    // DOWNLINK_RRC
    uint32_t pduId{};

    // DOWNLINK_RRC
    // UPLINK_RRC
    rrc::RrcChannel rrcChannel{};

    // RADIO_LINK_FAILURE
    rls::ERlfCause rlfCause{};

    // TRANSMISSION_FAILURE
    std::vector<rls::PduInfo> pduList;

    explicit NmGnbRlsToRls(PR present) : NtsMessage(NtsMessageType::GNB_RLS_TO_RLS), present(present)
    {
    }
};

struct NmGnbRrcToRls : NtsMessage
{
    enum PR
    {
        RRC_PDU_DELIVERY,
    } present;

    // RRC_PDU_DELIVERY
    int ueId{};
    rrc::RrcChannel channel{};
    OctetString pdu{};

    explicit NmGnbRrcToRls(PR present) : NtsMessage(NtsMessageType::GNB_RRC_TO_RLS), present(present)
    {
    }
};

struct NmGnbNgapToRrc : NtsMessage
{
    enum PR
    {
        RADIO_POWER_ON,
        NAS_DELIVERY,
        AN_RELEASE,
        PAGING,
        HANDOVER,
    } present;

    // NAS_DELIVERY
    // AN_RELEASE
    int ueId{};

    // NAS_DELIVERY
    OctetString pdu{};

    // PAGING
    asn::Unique<ASN_NGAP_FiveG_S_TMSI> uePagingTmsi{};
    asn::Unique<ASN_NGAP_TAIListForPaging> taiListForPaging{};

    // HANDOVER
    OctetString rrcContainer{};

    explicit NmGnbNgapToRrc(PR present) : NtsMessage(NtsMessageType::GNB_NGAP_TO_RRC), present(present)
    {
    }
};

struct NmGnbRrcToNgap : NtsMessage
{
    enum PR
    {
        INITIAL_NAS_DELIVERY,
        UPLINK_NAS_DELIVERY,
        HANDOVER_CONFIRM,
        RADIO_LINK_FAILURE,
        HANDOVER_COMMAND_FAILED,
    } present;

    // INITIAL_NAS_DELIVERY
    // UPLINK_NAS_DELIVERY
    // RADIO_LINK_FAILURE
    // HANDOVER_COMMAND_FAILED
    int ueId{};

    // INITIAL_NAS_DELIVERY
    // UPLINK_NAS_DELIVERY
    OctetString pdu{};

    // INITIAL_NAS_DELIVERY
    int64_t rrcEstablishmentCause{};
    std::optional<GutiMobileIdentity> sTmsi{};

    explicit NmGnbRrcToNgap(PR present) : NtsMessage(NtsMessageType::GNB_RRC_TO_NGAP), present(present)
    {
    }
};

struct NmGnbXnapToRrc : NtsMessage
{
    enum PR
    {
        SEND_HANDOVER_COMMAND,
        RELEASE_HANDOVER_SOURCE,
    } present;

    // SEND_HANDOVER_COMMAND
    // RELEASE_HANDOVER_SOURCE
    int ueId{};

    // SEND_HANDOVER_COMMAND
    OctetString targetCellContainer{};

    explicit NmGnbXnapToRrc(PR present) : NtsMessage(NtsMessageType::GNB_XNAP_TO_RRC), present(present)
    {
    }
};

struct NmGnbRrcToXnap : NtsMessage
{
    enum PR
    {
        HANDOVER_CONFIRM,
        HANDOVER_COMMAND_FAILED,
    } present;

    // HANDOVER_CONFIRM
    // HANDOVER_COMMAND_FAILED
    int ueId{};

    explicit NmGnbRrcToXnap(PR present) : NtsMessage(NtsMessageType::GNB_RRC_TO_XNAP), present(present)
    {
    }
};

struct NmGnbXnapToNgap : NtsMessage
{
    enum PR
    {
        PREPARE_HANDOVER_TARGET,
        PATH_SWITCH_REQUEST,
        // Releases the NGAP/GTP context of a UE locally (no AMF signalling): the source after the target switched the path,
        // or a target whose handover was dropped before the UE attached.
        RELEASE_HANDOVER_SOURCE,
        // As RELEASE_HANDOVER_SOURCE for a target whose UE is attached but whose Path Switch was rejected by the AMF: the UE
        // connection is released as well (RRC Release).
        RELEASE_FAILED_TARGET,
        // Asks the AMF to release the UE (UE Context Release Request); the AMF's release command then removes the
        // contexts. Used when a Path Switch Request was never answered. Falls back to RELEASE_FAILED_TARGET when the
        // request can not be sent.
        REQUEST_UE_RELEASE,
    } present;

    // PREPARE_HANDOVER_TARGET
    // PATH_SWITCH_REQUEST
    // RELEASE_HANDOVER_SOURCE
    // RELEASE_FAILED_TARGET
    // REQUEST_UE_RELEASE
    int ueId{};

    // PREPARE_HANDOVER_TARGET
    std::optional<int64_t> amfUeNgapId{};
    std::unique_ptr<XnHandoverNgapSnapshot> handoverSnapshot{};

    explicit NmGnbXnapToNgap(PR present) : NtsMessage(NtsMessageType::GNB_XNAP_TO_NGAP), present(present)
    {
    }
};

struct NmGnbNgapToXnap : NtsMessage
{
    enum PR
    {
        TARGET_PREPARE_ACK,
        TARGET_PREPARE_FAILURE,
        PATH_SWITCH_ACK,
        PATH_SWITCH_FAILURE,
        // The AMF accepted the Path Switch but its acknowledgement is unusable; this gNB asked the AMF to release the UE,
        // so the context stays until the AMF's release command arrives.
        PATH_SWITCH_UNUSABLE,
    } present;

    // TARGET_PREPARE_ACK
    // TARGET_PREPARE_FAILURE
    // PATH_SWITCH_ACK
    // PATH_SWITCH_FAILURE
    // PATH_SWITCH_UNUSABLE
    int ueId{};

    // TARGET_PREPARE_FAILURE
    // PATH_SWITCH_ACK: optional detail (for example sessions released by the AMF)
    // PATH_SWITCH_FAILURE: useful NGAP/local failure cause text
    std::string cause{};

    explicit NmGnbNgapToXnap(PR present) : NtsMessage(NtsMessageType::GNB_NGAP_TO_XNAP), present(present)
    {
    }
};

struct NmGnbXnapToGtp : NtsMessage
{
    enum PR
    {
        ALLOCATE_TARGET_FORWARDING,
        ACTIVATE_SOURCE_FORWARDING,
        CANCEL_FORWARDING,
    } present;

    int ueId{};

    // ACTIVATE_SOURCE_FORWARDING
    std::vector<XnForwardingEndpoint> endpoints{};

    explicit NmGnbXnapToGtp(PR present) : NtsMessage(NtsMessageType::GNB_XNAP_TO_GTP), present(present)
    {
    }
};

struct NmGnbGtpToXnap : NtsMessage
{
    enum PR
    {
        TARGET_SESSIONS_READY,
        TARGET_FORWARDING_ALLOCATED,
        SOURCE_FORWARDING_READY,
        FORWARDING_FAILURE,
    } present;

    int ueId{};

    // TARGET_FORWARDING_ALLOCATED
    std::vector<XnForwardingEndpoint> endpoints{};

    // FORWARDING_FAILURE
    std::string cause{};

    explicit NmGnbGtpToXnap(PR present) : NtsMessage(NtsMessageType::GNB_GTP_TO_XNAP), present(present)
    {
    }
};

struct NmGnbNgapToGtp : NtsMessage
{
    enum PR
    {
        UE_CONTEXT_UPDATE,
        UE_CONTEXT_RELEASE,
        SESSION_CREATE,
        SESSION_RELEASE,
        XN_TARGET_PREPARE_COMPLETE,
    } present;

    // UE_CONTEXT_UPDATE
    std::unique_ptr<GtpUeContextUpdate> update{};

    // SESSION_CREATE
    PduSessionResource *resource{};

    // UE_CONTEXT_RELEASE
    // SESSION_RELEASE
    int ueId{};

    // SESSION_RELEASE
    int psi{};

    explicit NmGnbNgapToGtp(PR present) : NtsMessage(NtsMessageType::GNB_NGAP_TO_GTP), present(present)
    {
    }
};

struct NmGnbSctp : NtsMessage
{
    enum PR
    {
        CONNECTION_REQUEST,
        CONNECTION_FAILURE,
        LISTEN_REQUEST,
        CONNECTION_ACCEPTED,
        CONNECTION_CLOSE,
        ASSOCIATION_SETUP,
        ASSOCIATION_SHUTDOWN,
        RECEIVE_MESSAGE,
        SEND_MESSAGE,
        UNHANDLED_NOTIFICATION,
    } present;

    // CONNECTION_REQUEST
    // CONNECTION_FAILURE
    // LISTEN_REQUEST
    // CONNECTION_ACCEPTED
    // CONNECTION_CLOSE
    // ASSOCIATION_SETUP
    // ASSOCIATION_SHUTDOWN
    // RECEIVE_MESSAGE
    // SEND_MESSAGE
    // UNHANDLED_NOTIFICATION
    int clientId{};

    // CONNECTION_FAILURE
    std::string error{};

    // LISTEN_REQUEST
    int listenerId{};

    // CONNECTION_REQUEST
    std::string localAddress{};
    uint16_t localPort{};
    std::string remoteAddress{};
    uint16_t remotePort{};
    sctp::PayloadProtocolId ppid{};
    NtsTask *associatedTask{};

    // CONNECTION_ACCEPTED (internal SctpTask hand-off)
    std::unique_ptr<sctp::SctpClient> acceptedClient{};

    // ASSOCIATION_SETUP
    int associationId{};
    int inStreams{};
    int outStreams{};

    // RECEIVE_MESSAGE
    // SEND_MESSAGE
    UniqueBuffer buffer{};
    uint16_t stream{};

    explicit NmGnbSctp(PR present) : NtsMessage(NtsMessageType::GNB_SCTP), present(present)
    {
    }
};

struct NmGnbStatusUpdate : NtsMessage
{
    static constexpr const int NGAP_IS_UP = 1;

    const int what;

    // NGAP_IS_UP
    bool isNgapUp{};

    explicit NmGnbStatusUpdate(const int what) : NtsMessage(NtsMessageType::GNB_STATUS_UPDATE), what(what)
    {
    }
};

struct NmGnbCliCommand : NtsMessage
{
    std::unique_ptr<app::GnbCliCommand> cmd;
    InetAddress address;

    NmGnbCliCommand(std::unique_ptr<app::GnbCliCommand> cmd, InetAddress address)
        : NtsMessage(NtsMessageType::GNB_CLI_COMMAND), cmd(std::move(cmd)), address(address)
    {
    }
};

} // namespace nr::gnb
