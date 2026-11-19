#include "codec.hpp"

#include "srsran/asn1/xnap/xnap_pdu_contents.h"
#include "srsran/asn1/xnap/xnap.h"

#include <algorithm>
#include <memory>

namespace nr::gnb::xnap
{
namespace
{

constexpr uint16_t XN_SETUP_PROCEDURE_CODE = 17;
constexpr uint16_t HANDOVER_PREPARATION_PROCEDURE_CODE = 0;
constexpr uint16_t HANDOVER_CANCEL_PROCEDURE_CODE = 2;
constexpr uint16_t UE_CONTEXT_RELEASE_PROCEDURE_CODE = 6;

uint32_t PlmnToNumber(const Plmn &plmn)
{
    const int mcc = plmn.mcc;
    const int mcc1 = (mcc % 1000) / 100;
    const int mcc2 = (mcc % 100) / 10;
    const int mcc3 = mcc % 10;

    const int mnc = plmn.mnc;
    int octet1 = (mcc2 << 4) | mcc1;
    int octet2 = 0;
    int octet3 = 0;

    if (plmn.isLongMnc)
    {
        const int mnc1 = (mnc % 1000) / 100;
        const int mnc2 = (mnc % 100) / 10;
        const int mnc3 = mnc % 10;
        octet2 = (mnc1 << 4) | mcc3;
        octet3 = (mnc3 << 4) | mnc2;
    }
    else
    {
        const int mnc1 = (mnc % 100) / 10;
        const int mnc2 = mnc % 10;
        octet2 = (0xF << 4) | mcc3;
        octet3 = (mnc2 << 4) | mnc1;
    }

    return (static_cast<uint32_t>(octet1) << 16U) | (static_cast<uint32_t>(octet2) << 8U) |
           static_cast<uint32_t>(octet3);
}

void FillGlobalGnbId(asn1::xnap::global_ng_ran_node_id_c &target, const GnbConfig &config)
{
    auto &globalGnbId = target.set_gnb();
    globalGnbId.plmn_id.from_number(PlmnToNumber(config.plmn));
    globalGnbId.gnb_id.set_gnb_id().from_number(config.getGnbId(), static_cast<uint32_t>(config.gnbIdLength));
}

void FillTaiSupportList(asn1::xnap::tai_support_list_l &target, const GnbConfig &config)
{
    target.resize(1);
    auto &tai = target[0];
    tai.tac.from_number(static_cast<uint32_t>(config.tac));
    tai.broadcast_plmns.resize(1);

    auto &broadcastPlmn = tai.broadcast_plmns[0];
    broadcastPlmn.plmn_id.from_number(PlmnToNumber(config.plmn));
    broadcastPlmn.tai_slice_support_list.resize(static_cast<uint32_t>(config.nssai.slices.size()));

    for (uint32_t i = 0; i < broadcastPlmn.tai_slice_support_list.size(); ++i)
    {
        const auto &source = config.nssai.slices[i];
        auto &targetSlice = broadcastPlmn.tai_slice_support_list[i];
        targetSlice.sst.from_number(static_cast<uint8_t>(source.sst));
        if (source.sd.has_value())
        {
            targetSlice.sd_present = true;
            targetSlice.sd.from_number(static_cast<uint32_t>(source.sd.value()));
        }
    }
}

void FillAmfRegionInfo(asn1::xnap::amf_region_info_l &target, const GnbConfig &config)
{
    target.resize(1);
    target[0].plmn_id.from_number(PlmnToNumber(config.plmn));
    target[0].amf_region_id.from_number(0);
}

UniqueBuffer EncodePdu(asn1::xnap::xn_ap_pdu_c &pdu)
{
    srsran::byte_buffer buffer{srsran::byte_buffer::fallback_allocation_tag{}};
    asn1::bit_ref writer{srsran::byte_buffer_writer{buffer}};
    if (pdu.pack(writer) != asn1::SRSASN_SUCCESS || buffer.empty())
        return {};

    auto *encoded = new uint8_t[buffer.length()];
    std::copy(buffer.begin(), buffer.end(), encoded);
    return UniqueBuffer{encoded, buffer.length()};
}

uint32_t GetPeerGnbId(const asn1::xnap::global_ng_ran_node_id_c &nodeId)
{
    if (nodeId.type().value != asn1::xnap::global_ng_ran_node_id_c::types_opts::gnb)
        return 0;

    const auto &gnb = nodeId.gnb();
    if (gnb.gnb_id.type().value != asn1::xnap::gnb_id_choice_c::types_opts::gnb_id)
        return 0;
    return static_cast<uint32_t>(gnb.gnb_id.gnb_id().to_number());
}

template <bool Aligned>
void FillOctets(asn1::unbounded_octstring<Aligned> &target, const std::vector<uint8_t> &bytes)
{
    static const uint8_t fallback = 0;
    if (bytes.empty())
        target.from_bytes(srsran::span<const uint8_t>{&fallback, 1});
    else
        target.from_bytes(srsran::span<const uint8_t>{bytes.data(), bytes.size()});
}

template <bool Aligned>
std::vector<uint8_t> ToVector(const asn1::unbounded_octstring<Aligned> &source)
{
    return {source.begin(), source.end()};
}

asn1::xnap::pdu_session_type_opts::options ToXnapSessionType(PduSessionType type)
{
    switch (type)
    {
    case PduSessionType::IPv6:
        return asn1::xnap::pdu_session_type_opts::ipv6;
    case PduSessionType::IPv4v6:
        return asn1::xnap::pdu_session_type_opts::ipv4v6;
    case PduSessionType::ETHERNET:
        return asn1::xnap::pdu_session_type_opts::ethernet;
    case PduSessionType::UNSTRUCTURED:
        return asn1::xnap::pdu_session_type_opts::unstructured;
    case PduSessionType::IPv4:
    default:
        return asn1::xnap::pdu_session_type_opts::ipv4;
    }
}

PduSessionType FromXnapSessionType(asn1::xnap::pdu_session_type_opts::options type)
{
    switch (type)
    {
    case asn1::xnap::pdu_session_type_opts::ipv6:
        return PduSessionType::IPv6;
    case asn1::xnap::pdu_session_type_opts::ipv4v6:
        return PduSessionType::IPv4v6;
    case asn1::xnap::pdu_session_type_opts::ethernet:
        return PduSessionType::ETHERNET;
    case asn1::xnap::pdu_session_type_opts::unstructured:
        return PduSessionType::UNSTRUCTURED;
    case asn1::xnap::pdu_session_type_opts::ipv4:
    default:
        return PduSessionType::IPv4;
    }
}

void FillSimulatorPduSession(asn1::xnap::pdu_session_res_to_be_setup_list_l &sessions,
                             const std::vector<XnForwardingEndpoint> &forwardingEndpoints)
{
    const size_t sessionCount = forwardingEndpoints.empty() ? 1 : forwardingEndpoints.size();
    sessions.resize(static_cast<uint32_t>(sessionCount));
    for (uint32_t i = 0; i < sessions.size(); ++i)
    {
        auto &session = sessions[i];
        const XnForwardingEndpoint *forwarding =
            forwardingEndpoints.empty() ? nullptr : &forwardingEndpoints[static_cast<size_t>(i)];
        session.pdu_session_id = forwarding == nullptr ? 1 : static_cast<uint16_t>(forwarding->psi);
        session.s_nssai.sst.from_number(1);
        auto &tunnel = session.ul_ng_u_tnl_at_up_f.set_gtp_tunnel();
        tunnel.tnl_address.from_number(0, 32);
        tunnel.gtp_teid.from_number(0);
        session.pdu_session_type =
            forwarding == nullptr ? asn1::xnap::pdu_session_type_opts::ipv4 : ToXnapSessionType(forwarding->sessionType);

        const size_t flowCount = forwarding == nullptr || forwarding->qosFlowIds.empty()
                                     ? 1
                                     : forwarding->qosFlowIds.size();
        session.qos_flows_to_be_setup_list.resize(static_cast<uint32_t>(flowCount));
        for (uint32_t flowIndex = 0; flowIndex < session.qos_flows_to_be_setup_list.size(); ++flowIndex)
        {
            auto &flow = session.qos_flows_to_be_setup_list[flowIndex];
            flow.qfi = forwarding == nullptr || forwarding->qosFlowIds.empty()
                           ? 1
                           : static_cast<uint8_t>(forwarding->qosFlowIds[static_cast<size_t>(flowIndex)]);
            flow.qos_flow_level_qos_params.qos_characteristics.set_non_dyn().five_qi = 9;
            flow.qos_flow_level_qos_params.alloc_and_retention_prio.prio_level = 1;
            flow.qos_flow_level_qos_params.alloc_and_retention_prio.pre_emption_cap =
                asn1::xnap::allocand_retention_prio_s::pre_emption_cap_opts::shall_not_trigger_preemption;
            flow.qos_flow_level_qos_params.alloc_and_retention_prio.pre_emption_vulnerability =
                asn1::xnap::allocand_retention_prio_s::pre_emption_vulnerability_opts::not_preemptable;
        }

        if (forwarding != nullptr && !forwarding->qosFlowIds.empty())
        {
            session.dataforwardinginfofrom_source_present = true;
            auto &proposal = session.dataforwardinginfofrom_source.qos_flows_to_be_forwarded;
            proposal.resize(static_cast<uint32_t>(forwarding->qosFlowIds.size()));
            for (uint32_t flowIndex = 0; flowIndex < proposal.size(); ++flowIndex)
            {
                proposal[flowIndex].qos_flow_id =
                    static_cast<uint8_t>(forwarding->qosFlowIds[static_cast<size_t>(flowIndex)]);
                proposal[flowIndex].dl_dataforwarding =
                    asn1::xnap::dl_forwarding_opts::dl_forwarding_proposed;
                // The imported XnAP ASN.1 model represents both forwarding direction enums as
                // mandatory members of QoSFLowsToBeForwarded-Item. This simulator implements
                // downlink Xn-U forwarding only; populate the mandatory UL enum so the PDU is
                // encodable, while DecodeForwardingProposals intentionally consumes only DL.
                proposal[flowIndex].ul_dataforwarding =
                    asn1::xnap::ul_forwarding_opts::ul_forwarding_proposed;
            }
        }
    }
}

void FillSimulatorUeContext(asn1::xnap::ue_context_info_ho_request_s &context, const HandoverData &data)
{
    context.ng_c_ue_ref = data.amfUeNgapId.value_or(0);
    context.cp_tnl_info_source.set_endpoint_ip_address().from_number(0, 32);
    context.ue_security_cap.nr_encyption_algorithms.from_number(0);
    context.ue_security_cap.nr_integrity_protection_algorithms.from_number(0);
    context.ue_security_cap.e_utra_encyption_algorithms.from_number(0);
    context.ue_security_cap.e_utra_integrity_protection_algorithms.from_number(0);
    context.security_info.key_ng_ran_star.from_number(0);
    context.security_info.ncc = 0;
    context.ue_ambr.dl_ue_ambr = 0;
    context.ue_ambr.ul_ue_ambr = 0;
    FillSimulatorPduSession(context.pdu_session_res_to_be_setup_list, data.forwardingEndpoints);
    FillOctets(context.rrc_context, data.transparentContainer);
}

void FillSimulatorUeHistory(asn1::xnap::ue_history_info_l &history)
{
    history.resize(1);
    static const uint8_t placeholder = 0;
    history[0].set_ng_ran_cell().from_bytes(srsran::span<const uint8_t>{&placeholder, 1});
}

bool FillIpv4Tunnel(asn1::xnap::up_transport_layer_info_c &target, const GtpTunnel &source)
{
    if (source.address.length() != 4 || source.teid == 0)
        return false;
    uint32_t address = 0;
    for (int i = 0; i < 4; ++i)
        address = (address << 8U) | static_cast<uint32_t>(source.address.data()[i]);
    auto &tunnel = target.set_gtp_tunnel();
    tunnel.tnl_address.from_number(address, 32);
    tunnel.gtp_teid.from_number(source.teid);
    return true;
}

void FillSimulatorAdmittedSessions(asn1::xnap::pdu_session_res_admitted_list_l &sessions,
                                   const std::vector<XnForwardingEndpoint> &forwardingEndpoints)
{
    const size_t sessionCount = forwardingEndpoints.empty() ? 1 : forwardingEndpoints.size();
    sessions.resize(static_cast<uint32_t>(sessionCount));
    for (uint32_t i = 0; i < sessions.size(); ++i)
    {
        auto &session = sessions[i];
        const XnForwardingEndpoint *forwarding =
            forwardingEndpoints.empty() ? nullptr : &forwardingEndpoints[static_cast<size_t>(i)];
        session.pdu_session_id = forwarding == nullptr ? 1 : static_cast<uint16_t>(forwarding->psi);
        const size_t flowCount = forwarding == nullptr || forwarding->qosFlowIds.empty()
                                     ? 1
                                     : forwarding->qosFlowIds.size();
        session.pdu_session_res_admitted_info.qos_flows_admitted_list.resize(static_cast<uint32_t>(flowCount));
        for (uint32_t flowIndex = 0;
             flowIndex < session.pdu_session_res_admitted_info.qos_flows_admitted_list.size(); ++flowIndex)
        {
            session.pdu_session_res_admitted_info.qos_flows_admitted_list[flowIndex].qfi =
                forwarding == nullptr || forwarding->qosFlowIds.empty()
                    ? 1
                    : static_cast<uint8_t>(forwarding->qosFlowIds[static_cast<size_t>(flowIndex)]);
        }

        if (forwarding != nullptr && FillIpv4Tunnel(
                                        session.pdu_session_res_admitted_info.data_forwarding_info_from_target
                                            .pdu_session_level_dl_data_forwarding_info,
                                        forwarding->tunnel))
        {
            auto &info = session.pdu_session_res_admitted_info.data_forwarding_info_from_target;
            session.pdu_session_res_admitted_info.data_forwarding_info_from_target_present = true;
            info.pdu_session_level_dl_data_forwarding_info_present = true;
            info.qos_flows_accepted_for_data_forwarding_list.resize(
                static_cast<uint32_t>(forwarding->qosFlowIds.size()));
            for (uint32_t flowIndex = 0; flowIndex < info.qos_flows_accepted_for_data_forwarding_list.size();
                 ++flowIndex)
            {
                info.qos_flows_accepted_for_data_forwarding_list[flowIndex].qos_flow_id =
                    static_cast<uint8_t>(forwarding->qosFlowIds[static_cast<size_t>(flowIndex)]);
            }
        }
    }
}

std::vector<XnForwardingEndpoint>
DecodeForwardingProposals(const asn1::xnap::pdu_session_res_to_be_setup_list_l &sessions)
{
    std::vector<XnForwardingEndpoint> result;
    for (const auto &session : sessions)
    {
        if (!session.dataforwardinginfofrom_source_present)
            continue;
        XnForwardingEndpoint endpoint{};
        endpoint.psi = static_cast<int>(session.pdu_session_id);
        endpoint.sessionType = FromXnapSessionType(session.pdu_session_type.value);
        for (const auto &flow : session.dataforwardinginfofrom_source.qos_flows_to_be_forwarded)
        {
            if (flow.dl_dataforwarding.value == asn1::xnap::dl_forwarding_opts::dl_forwarding_proposed)
                endpoint.qosFlowIds.push_back(static_cast<int>(flow.qos_flow_id));
        }
        if (!endpoint.qosFlowIds.empty())
            result.push_back(std::move(endpoint));
    }
    return result;
}

std::vector<XnForwardingEndpoint>
DecodeForwardingEndpoints(const asn1::xnap::pdu_session_res_admitted_list_l &sessions)
{
    std::vector<XnForwardingEndpoint> result;
    for (const auto &session : sessions)
    {
        const auto &admitted = session.pdu_session_res_admitted_info;
        if (!admitted.data_forwarding_info_from_target_present ||
            !admitted.data_forwarding_info_from_target.pdu_session_level_dl_data_forwarding_info_present)
            continue;
        const auto &transport =
            admitted.data_forwarding_info_from_target.pdu_session_level_dl_data_forwarding_info;
        if (transport.type().value != asn1::xnap::up_transport_layer_info_c::types_opts::gtp_tunnel)
            continue;
        const auto &tunnel = transport.gtp_tunnel();
        if (tunnel.tnl_address.length() != 32)
            continue;

        XnForwardingEndpoint endpoint{};
        endpoint.psi = static_cast<int>(session.pdu_session_id);
        endpoint.tunnel.teid = static_cast<uint32_t>(tunnel.gtp_teid.to_number());
        uint32_t address = static_cast<uint32_t>(tunnel.tnl_address.to_number());
        endpoint.tunnel.address.appendOctet(static_cast<uint8_t>((address >> 24U) & 0xffU));
        endpoint.tunnel.address.appendOctet(static_cast<uint8_t>((address >> 16U) & 0xffU));
        endpoint.tunnel.address.appendOctet(static_cast<uint8_t>((address >> 8U) & 0xffU));
        endpoint.tunnel.address.appendOctet(static_cast<uint8_t>(address & 0xffU));
        for (const auto &flow : admitted.data_forwarding_info_from_target.qos_flows_accepted_for_data_forwarding_list)
            endpoint.qosFlowIds.push_back(static_cast<int>(flow.qos_flow_id));
        result.push_back(std::move(endpoint));
    }
    return result;
}

void FillUnspecifiedCause(asn1::xnap::cause_c &cause)
{
    cause.set_radio_network() = asn1::xnap::cause_radio_network_layer_opts::unspecified;
}

} // namespace

UniqueBuffer EncodeSetupRequest(const GnbConfig &config)
{
    if (config.gnbIdLength < 22 || config.gnbIdLength > 32 || config.nssai.slices.empty())
        return {};

    asn1::xnap::xn_ap_pdu_c pdu;
    auto &message = pdu.set_init_msg();
    if (!message.load_info_obj(XN_SETUP_PROCEDURE_CODE))
        return {};

    auto &request = message.value.xn_setup_request();
    FillGlobalGnbId(request->global_ng_ran_node_id, config);
    FillTaiSupportList(request->tai_support_list, config);
    FillAmfRegionInfo(request->amf_region_info, config);
    return EncodePdu(pdu);
}

UniqueBuffer EncodeSetupResponse(const GnbConfig &config)
{
    if (config.gnbIdLength < 22 || config.gnbIdLength > 32 || config.nssai.slices.empty())
        return {};

    asn1::xnap::xn_ap_pdu_c pdu;
    auto &message = pdu.set_successful_outcome();
    if (!message.load_info_obj(XN_SETUP_PROCEDURE_CODE))
        return {};

    auto &response = message.value.xn_setup_resp();
    FillGlobalGnbId(response->global_ng_ran_node_id, config);
    FillTaiSupportList(response->tai_support_list, config);
    return EncodePdu(pdu);
}

UniqueBuffer EncodeHandoverRequest(const HandoverData &data, const GnbConfig &targetConfig)
{
    asn1::xnap::xn_ap_pdu_c pdu;
    auto &message = pdu.set_init_msg();
    if (!message.load_info_obj(HANDOVER_PREPARATION_PROCEDURE_CODE))
        return {};

    auto &request = message.value.ho_request();
    request->source_ng_ra_nnode_ue_xn_ap_id = data.sourceUeXnapId;
    FillUnspecifiedCause(request->cause);

    auto &targetCgi = request->target_cell_global_id.set_nr();
    targetCgi.plmn_id.from_number(PlmnToNumber(targetConfig.plmn));
    targetCgi.nr_ci.from_number(static_cast<uint64_t>(targetConfig.nci));

    // UERANSIM does not currently carry GUAMI/security/PDU-session state in its Xn control model.
    // Fill protocol-valid placeholders while preserving the UE reference and opaque RRC context.
    request->guami.plmn_id.from_number(PlmnToNumber(targetConfig.plmn));
    request->guami.amf_region_id.from_number(0);
    request->guami.amf_set_id.from_number(0);
    request->guami.amf_pointer.from_number(0);
    FillSimulatorUeContext(request->ue_context_info_ho_request, data);
    FillSimulatorUeHistory(request->ue_history_info);
    return EncodePdu(pdu);
}

UniqueBuffer EncodeHandoverRequestAck(const HandoverData &data)
{
    asn1::xnap::xn_ap_pdu_c pdu;
    auto &message = pdu.set_successful_outcome();
    if (!message.load_info_obj(HANDOVER_PREPARATION_PROCEDURE_CODE))
        return {};

    auto &ack = message.value.ho_request_ack();
    ack->source_ng_ra_nnode_ue_xn_ap_id = data.sourceUeXnapId;
    ack->target_ng_ra_nnode_ue_xn_ap_id = data.targetUeXnapId;
    FillSimulatorAdmittedSessions(ack->pdu_session_res_admitted_list, data.forwardingEndpoints);
    FillOctets(ack->target2_source_ng_ra_nnode_transp_container, data.transparentContainer);
    return EncodePdu(pdu);
}

UniqueBuffer EncodeHandoverPreparationFailure(const HandoverData &data)
{
    asn1::xnap::xn_ap_pdu_c pdu;
    auto &message = pdu.set_unsuccessful_outcome();
    if (!message.load_info_obj(HANDOVER_PREPARATION_PROCEDURE_CODE))
        return {};

    auto &failure = message.value.ho_prep_fail();
    failure->source_ng_ra_nnode_ue_xn_ap_id = data.sourceUeXnapId;
    FillUnspecifiedCause(failure->cause);
    return EncodePdu(pdu);
}

UniqueBuffer EncodeHandoverCancel(const HandoverData &data)
{
    asn1::xnap::xn_ap_pdu_c pdu;
    auto &message = pdu.set_init_msg();
    if (!message.load_info_obj(HANDOVER_CANCEL_PROCEDURE_CODE))
        return {};

    auto &cancel = message.value.ho_cancel();
    cancel->source_ng_ra_nnode_ue_xn_ap_id = data.sourceUeXnapId;
    if (data.targetUeXnapId != 0)
    {
        cancel->target_ng_ra_nnode_ue_xn_ap_id_present = true;
        cancel->target_ng_ra_nnode_ue_xn_ap_id = data.targetUeXnapId;
    }
    FillUnspecifiedCause(cancel->cause);
    return EncodePdu(pdu);
}

UniqueBuffer EncodeUeContextRelease(const HandoverData &data)
{
    asn1::xnap::xn_ap_pdu_c pdu;
    auto &message = pdu.set_init_msg();
    if (!message.load_info_obj(UE_CONTEXT_RELEASE_PROCEDURE_CODE))
        return {};

    auto &release = message.value.ue_context_release();
    release->source_ng_ra_nnode_ue_xn_ap_id = data.sourceUeXnapId;
    release->target_ng_ra_nnode_ue_xn_ap_id = data.targetUeXnapId;
    return EncodePdu(pdu);
}

std::optional<DecodedMessage> Decode(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0)
        return std::nullopt;

    srsran::span<const uint8_t> input{data, size};
    srsran::byte_buffer buffer{srsran::byte_buffer::fallback_allocation_tag{}, input};
    asn1::cbit_ref reader{buffer};
    asn1::xnap::xn_ap_pdu_c pdu;
    if (pdu.unpack(reader) != asn1::SRSASN_SUCCESS)
        return std::nullopt;

    DecodedMessage result{};
    if (pdu.type().value == asn1::xnap::xn_ap_pdu_c::types_opts::init_msg)
    {
        const auto &message = pdu.init_msg();
        if (message.proc_code == XN_SETUP_PROCEDURE_CODE &&
            message.value.type().value == asn1::xnap::xnap_elem_procs_o::init_msg_c::types_opts::xn_setup_request)
        {
            result.type = MessageType::XN_SETUP_REQUEST;
            result.peerGnbId = GetPeerGnbId(message.value.xn_setup_request()->global_ng_ran_node_id);
        }
        else if (message.proc_code == HANDOVER_PREPARATION_PROCEDURE_CODE &&
                 message.value.type().value == asn1::xnap::xnap_elem_procs_o::init_msg_c::types_opts::ho_request)
        {
            const auto &request = message.value.ho_request();
            result.type = MessageType::HANDOVER_REQUEST;
            result.handover = HandoverData{};
            result.handover->sourceUeXnapId = static_cast<uint32_t>(request->source_ng_ra_nnode_ue_xn_ap_id);
            result.handover->amfUeNgapId = request->ue_context_info_ho_request.ng_c_ue_ref;
            result.handover->transparentContainer = ToVector(request->ue_context_info_ho_request.rrc_context);
            result.handover->forwardingEndpoints =
                DecodeForwardingProposals(request->ue_context_info_ho_request.pdu_session_res_to_be_setup_list);
        }
        else if (message.proc_code == HANDOVER_CANCEL_PROCEDURE_CODE &&
                 message.value.type().value == asn1::xnap::xnap_elem_procs_o::init_msg_c::types_opts::ho_cancel)
        {
            const auto &cancel = message.value.ho_cancel();
            result.type = MessageType::HANDOVER_CANCEL;
            result.handover = HandoverData{};
            result.handover->sourceUeXnapId = static_cast<uint32_t>(cancel->source_ng_ra_nnode_ue_xn_ap_id);
            if (cancel->target_ng_ra_nnode_ue_xn_ap_id_present)
                result.handover->targetUeXnapId =
                    static_cast<uint32_t>(cancel->target_ng_ra_nnode_ue_xn_ap_id);
        }
        else if (message.proc_code == UE_CONTEXT_RELEASE_PROCEDURE_CODE &&
                 message.value.type().value ==
                     asn1::xnap::xnap_elem_procs_o::init_msg_c::types_opts::ue_context_release)
        {
            const auto &release = message.value.ue_context_release();
            result.type = MessageType::UE_CONTEXT_RELEASE;
            result.handover = HandoverData{};
            result.handover->sourceUeXnapId = static_cast<uint32_t>(release->source_ng_ra_nnode_ue_xn_ap_id);
            result.handover->targetUeXnapId = static_cast<uint32_t>(release->target_ng_ra_nnode_ue_xn_ap_id);
        }
    }
    else if (pdu.type().value == asn1::xnap::xn_ap_pdu_c::types_opts::successful_outcome)
    {
        const auto &message = pdu.successful_outcome();
        if (message.proc_code == XN_SETUP_PROCEDURE_CODE &&
            message.value.type().value ==
                asn1::xnap::xnap_elem_procs_o::successful_outcome_c::types_opts::xn_setup_resp)
        {
            result.type = MessageType::XN_SETUP_RESPONSE;
            result.peerGnbId = GetPeerGnbId(message.value.xn_setup_resp()->global_ng_ran_node_id);
        }
        else if (message.proc_code == HANDOVER_PREPARATION_PROCEDURE_CODE &&
                 message.value.type().value ==
                     asn1::xnap::xnap_elem_procs_o::successful_outcome_c::types_opts::ho_request_ack)
        {
            const auto &ack = message.value.ho_request_ack();
            result.type = MessageType::HANDOVER_REQUEST_ACK;
            result.handover = HandoverData{};
            result.handover->sourceUeXnapId = static_cast<uint32_t>(ack->source_ng_ra_nnode_ue_xn_ap_id);
            result.handover->targetUeXnapId = static_cast<uint32_t>(ack->target_ng_ra_nnode_ue_xn_ap_id);
            result.handover->transparentContainer = ToVector(ack->target2_source_ng_ra_nnode_transp_container);
            result.handover->forwardingEndpoints = DecodeForwardingEndpoints(ack->pdu_session_res_admitted_list);
        }
    }
    else if (pdu.type().value == asn1::xnap::xn_ap_pdu_c::types_opts::unsuccessful_outcome)
    {
        const auto &message = pdu.unsuccessful_outcome();
        if (message.proc_code == XN_SETUP_PROCEDURE_CODE &&
            message.value.type().value ==
                asn1::xnap::xnap_elem_procs_o::unsuccessful_outcome_c::types_opts::xn_setup_fail)
        {
            result.type = MessageType::XN_SETUP_FAILURE;
        }
        else if (message.proc_code == HANDOVER_PREPARATION_PROCEDURE_CODE &&
                 message.value.type().value ==
                     asn1::xnap::xnap_elem_procs_o::unsuccessful_outcome_c::types_opts::ho_prep_fail)
        {
            const auto &failure = message.value.ho_prep_fail();
            result.type = MessageType::HANDOVER_PREPARATION_FAILURE;
            result.handover = HandoverData{};
            result.handover->sourceUeXnapId = static_cast<uint32_t>(failure->source_ng_ra_nnode_ue_xn_ap_id);
        }
    }

    return result;
}

} // namespace nr::gnb::xnap
