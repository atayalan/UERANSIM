//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <ext/doctest/doctest.h>
#include <gnb/xnap/codec.hpp>

#include "srsran/asn1/xnap/xnap.h"
#include "srsran/asn1/xnap/xnap_ies.h"
#include "srsran/asn1/xnap/xnap_pdu_contents.h"

namespace
{

constexpr uint16_t XN_SETUP_PROCEDURE_CODE = 17;
constexpr uint32_t TEST_PLMN = 0x00F110; // MCC 001, MNC 01.

void fillGlobalGnbId(asn1::xnap::global_ng_ran_node_id_c &target, uint32_t gnbId, uint32_t gnbIdLength)
{
    auto &globalGnbId = target.set_gnb();
    globalGnbId.plmn_id.from_number(TEST_PLMN);
    globalGnbId.gnb_id.set_gnb_id().from_number(gnbId, gnbIdLength);
}

void fillTaiSupportList(asn1::xnap::tai_support_list_l &target)
{
    target.resize(1);
    target[0].tac.from_number(0x010203);
    target[0].broadcast_plmns.resize(1);

    auto &broadcastPlmn = target[0].broadcast_plmns[0];
    broadcastPlmn.plmn_id.from_number(TEST_PLMN);
    broadcastPlmn.tai_slice_support_list.resize(1);
    broadcastPlmn.tai_slice_support_list[0].sst.from_number(1);
    broadcastPlmn.tai_slice_support_list[0].sd_present = true;
    broadcastPlmn.tai_slice_support_list[0].sd.from_number(0x112233);
}

TEST_CASE("gnb/xnap/codec: an Xn Setup Request survives encode and decode")
{
    constexpr uint32_t TEST_GNB_ID = 0xABCDE;
    constexpr uint32_t TEST_GNB_ID_LENGTH = 24;

    asn1::xnap::xn_ap_pdu_c encodedPdu;
    auto &message = encodedPdu.set_init_msg();
    REQUIRE(message.load_info_obj(XN_SETUP_PROCEDURE_CODE));

    auto &request = message.value.xn_setup_request();
    fillGlobalGnbId(request->global_ng_ran_node_id, TEST_GNB_ID, TEST_GNB_ID_LENGTH);
    fillTaiSupportList(request->tai_support_list);
    request->amf_region_info.resize(1);
    request->amf_region_info[0].plmn_id.from_number(TEST_PLMN);
    request->amf_region_info[0].amf_region_id.from_number(0);

    srsran::byte_buffer buffer{srsran::byte_buffer::fallback_allocation_tag{}};
    asn1::bit_ref writer{srsran::byte_buffer_writer{buffer}};
    REQUIRE_EQ(encodedPdu.pack(writer), asn1::SRSASN_SUCCESS);
    CHECK(!buffer.empty());

    asn1::xnap::xn_ap_pdu_c decodedPdu;
    asn1::cbit_ref reader{buffer};
    REQUIRE_EQ(decodedPdu.unpack(reader), asn1::SRSASN_SUCCESS);
    CHECK(decodedPdu.type().value == asn1::xnap::xn_ap_pdu_c::types_opts::init_msg);

    const auto &decodedMessage = decodedPdu.init_msg();
    CHECK_EQ(decodedMessage.proc_code, XN_SETUP_PROCEDURE_CODE);
    CHECK(decodedMessage.value.type().value == asn1::xnap::xnap_elem_procs_o::init_msg_c::types_opts::xn_setup_request);

    const auto &decodedRequest = decodedMessage.value.xn_setup_request();
    CHECK_EQ(decodedRequest->global_ng_ran_node_id.gnb().gnb_id.gnb_id().to_number(), TEST_GNB_ID);
    CHECK_EQ(decodedRequest->global_ng_ran_node_id.gnb().gnb_id.gnb_id().length(), TEST_GNB_ID_LENGTH);
    CHECK_EQ(decodedRequest->tai_support_list[0].tac.to_number(), 0x010203U);
    CHECK_EQ(decodedRequest->tai_support_list[0].broadcast_plmns[0].tai_slice_support_list[0].sst.to_number(), 1U);
    CHECK_EQ(decodedRequest->tai_support_list[0].broadcast_plmns[0].tai_slice_support_list[0].sd.to_number(), 0x112233U);
}

TEST_CASE("gnb/xnap/codec: an Xn Setup Response survives encode and decode")
{
    constexpr uint32_t TEST_GNB_ID = 0x123456;
    constexpr uint32_t TEST_GNB_ID_LENGTH = 24;

    asn1::xnap::xn_ap_pdu_c encodedPdu;
    auto &message = encodedPdu.set_successful_outcome();
    REQUIRE(message.load_info_obj(XN_SETUP_PROCEDURE_CODE));

    auto &response = message.value.xn_setup_resp();
    fillGlobalGnbId(response->global_ng_ran_node_id, TEST_GNB_ID, TEST_GNB_ID_LENGTH);
    fillTaiSupportList(response->tai_support_list);

    srsran::byte_buffer buffer{srsran::byte_buffer::fallback_allocation_tag{}};
    asn1::bit_ref writer{srsran::byte_buffer_writer{buffer}};
    REQUIRE_EQ(encodedPdu.pack(writer), asn1::SRSASN_SUCCESS);
    CHECK(!buffer.empty());

    asn1::xnap::xn_ap_pdu_c decodedPdu;
    asn1::cbit_ref reader{buffer};
    REQUIRE_EQ(decodedPdu.unpack(reader), asn1::SRSASN_SUCCESS);
    CHECK(decodedPdu.type().value == asn1::xnap::xn_ap_pdu_c::types_opts::successful_outcome);

    const auto &decodedMessage = decodedPdu.successful_outcome();
    CHECK_EQ(decodedMessage.proc_code, XN_SETUP_PROCEDURE_CODE);
    CHECK(decodedMessage.value.type().value == asn1::xnap::xnap_elem_procs_o::successful_outcome_c::types_opts::xn_setup_resp);

    const auto &decodedResponse = decodedMessage.value.xn_setup_resp();
    CHECK_EQ(decodedResponse->global_ng_ran_node_id.gnb().gnb_id.gnb_id().to_number(), TEST_GNB_ID);
    CHECK_EQ(decodedResponse->global_ng_ran_node_id.gnb().gnb_id.gnb_id().length(), TEST_GNB_ID_LENGTH);
    CHECK_EQ(decodedResponse->tai_support_list[0].tac.to_number(), 0x010203U);
}

nr::gnb::GnbConfig makeCodecConfig(uint32_t gnbId)
{
    nr::gnb::GnbConfig config{};
    config.gnbIdLength = 24;
    config.nci = static_cast<int64_t>(gnbId) << (36 - config.gnbIdLength);
    config.plmn = Plmn{1, 1, false};
    config.tac = 0x010203;

    SingleSlice slice{};
    slice.sst = 1;
    slice.sd = octet3{0x112233};
    config.nssai.slices.push_back(slice);
    return config;
}

TEST_CASE("gnb/xnap/codec: the helpers round-trip Xn Setup Request and Response")
{
    constexpr uint32_t REQUEST_GNB_ID = 0x654321;
    constexpr uint32_t RESPONSE_GNB_ID = 0x123456;

    auto requestConfig = makeCodecConfig(REQUEST_GNB_ID);
    auto request = nr::gnb::xnap::EncodeSetupRequest(requestConfig);
    CHECK(request.size() > 0);
    auto decodedRequest = nr::gnb::xnap::Decode(request.data(), request.size());
    REQUIRE(decodedRequest.has_value());
    CHECK(decodedRequest->type == nr::gnb::xnap::MessageType::XN_SETUP_REQUEST);
    CHECK_EQ(decodedRequest->peerGnbId, REQUEST_GNB_ID);

    auto responseConfig = makeCodecConfig(RESPONSE_GNB_ID);
    auto response = nr::gnb::xnap::EncodeSetupResponse(responseConfig);
    CHECK(response.size() > 0);
    auto decodedResponse = nr::gnb::xnap::Decode(response.data(), response.size());
    REQUIRE(decodedResponse.has_value());
    CHECK(decodedResponse->type == nr::gnb::xnap::MessageType::XN_SETUP_RESPONSE);
    CHECK_EQ(decodedResponse->peerGnbId, RESPONSE_GNB_ID);
}

TEST_CASE("gnb/xnap/codec: the helpers round-trip the handover messages")
{
    nr::gnb::xnap::HandoverData requestData{};
    requestData.sourceUeXnapId = 0x10203;
    requestData.amfUeNgapId = 0x123456789ULL;
    requestData.transparentContainer = {0x11, 0x22, 0x33, 0x44};
    nr::gnb::XnForwardingEndpoint proposal{};
    proposal.psi = 5;
    proposal.qosFlowIds = {9, 11};
    requestData.forwardingEndpoints.push_back(proposal);

    auto targetConfig = makeCodecConfig(0x456789);
    auto request = nr::gnb::xnap::EncodeHandoverRequest(requestData, targetConfig);
    CHECK(request.size() > 0);
    auto decodedRequest = nr::gnb::xnap::Decode(request.data(), request.size());
    REQUIRE(decodedRequest.has_value());
    CHECK(decodedRequest->type == nr::gnb::xnap::MessageType::HANDOVER_REQUEST);
    REQUIRE(decodedRequest->handover.has_value());
    CHECK_EQ(decodedRequest->handover->sourceUeXnapId, requestData.sourceUeXnapId);
    CHECK(decodedRequest->handover->amfUeNgapId == requestData.amfUeNgapId);
    CHECK(decodedRequest->handover->transparentContainer == requestData.transparentContainer);
    CHECK_EQ(decodedRequest->handover->forwardingEndpoints.size(), 1U);
    CHECK_EQ(decodedRequest->handover->forwardingEndpoints[0].psi, proposal.psi);
    CHECK(decodedRequest->handover->forwardingEndpoints[0].qosFlowIds == proposal.qosFlowIds);

    nr::gnb::xnap::HandoverData ackData{};
    ackData.sourceUeXnapId = requestData.sourceUeXnapId;
    ackData.targetUeXnapId = 0x40506;
    ackData.transparentContainer = {0xAA, 0xBB, 0xCC};
    nr::gnb::XnForwardingEndpoint forwarding{};
    forwarding.psi = proposal.psi;
    forwarding.tunnel.teid = 0xE0000123;
    forwarding.tunnel.address.appendOctet(10);
    forwarding.tunnel.address.appendOctet(22);
    forwarding.tunnel.address.appendOctet(103);
    forwarding.tunnel.address.appendOctet(142);
    forwarding.qosFlowIds = proposal.qosFlowIds;
    ackData.forwardingEndpoints.push_back(forwarding);
    auto ack = nr::gnb::xnap::EncodeHandoverRequestAck(ackData);
    CHECK(ack.size() > 0);
    auto decodedAck = nr::gnb::xnap::Decode(ack.data(), ack.size());
    REQUIRE(decodedAck.has_value());
    CHECK(decodedAck->type == nr::gnb::xnap::MessageType::HANDOVER_REQUEST_ACK);
    CHECK_EQ(decodedAck->handover->sourceUeXnapId, ackData.sourceUeXnapId);
    CHECK_EQ(decodedAck->handover->targetUeXnapId, ackData.targetUeXnapId);
    CHECK(decodedAck->handover->transparentContainer == ackData.transparentContainer);
    CHECK_EQ(decodedAck->handover->forwardingEndpoints.size(), 1U);
    CHECK_EQ(decodedAck->handover->forwardingEndpoints[0].psi, forwarding.psi);
    CHECK_EQ(decodedAck->handover->forwardingEndpoints[0].tunnel.teid, forwarding.tunnel.teid);
    // OctetString::operator== is not const, so keep doctest from capturing the operands as const references.
    CHECK((decodedAck->handover->forwardingEndpoints[0].tunnel.address == forwarding.tunnel.address));
    CHECK(decodedAck->handover->forwardingEndpoints[0].qosFlowIds == forwarding.qosFlowIds);

    nr::gnb::xnap::HandoverData failureData{};
    failureData.sourceUeXnapId = requestData.sourceUeXnapId;
    auto failure = nr::gnb::xnap::EncodeHandoverPreparationFailure(failureData);
    CHECK(failure.size() > 0);
    auto decodedFailure = nr::gnb::xnap::Decode(failure.data(), failure.size());
    REQUIRE(decodedFailure.has_value());
    CHECK(decodedFailure->type == nr::gnb::xnap::MessageType::HANDOVER_PREPARATION_FAILURE);
    CHECK_EQ(decodedFailure->handover->sourceUeXnapId, failureData.sourceUeXnapId);

    nr::gnb::xnap::HandoverData cancelData{};
    cancelData.sourceUeXnapId = requestData.sourceUeXnapId;
    cancelData.targetUeXnapId = ackData.targetUeXnapId;
    auto cancel = nr::gnb::xnap::EncodeHandoverCancel(cancelData);
    CHECK(cancel.size() > 0);
    auto decodedCancel = nr::gnb::xnap::Decode(cancel.data(), cancel.size());
    REQUIRE(decodedCancel.has_value());
    CHECK(decodedCancel->type == nr::gnb::xnap::MessageType::HANDOVER_CANCEL);
    CHECK_EQ(decodedCancel->handover->sourceUeXnapId, cancelData.sourceUeXnapId);
    CHECK_EQ(decodedCancel->handover->targetUeXnapId, cancelData.targetUeXnapId);

    nr::gnb::xnap::HandoverData releaseData{};
    releaseData.sourceUeXnapId = requestData.sourceUeXnapId;
    releaseData.targetUeXnapId = ackData.targetUeXnapId;
    auto release = nr::gnb::xnap::EncodeUeContextRelease(releaseData);
    CHECK(release.size() > 0);
    auto decodedRelease = nr::gnb::xnap::Decode(release.data(), release.size());
    REQUIRE(decodedRelease.has_value());
    CHECK(decodedRelease->type == nr::gnb::xnap::MessageType::UE_CONTEXT_RELEASE);
    CHECK_EQ(decodedRelease->handover->sourceUeXnapId, releaseData.sourceUeXnapId);
    CHECK_EQ(decodedRelease->handover->targetUeXnapId, releaseData.targetUeXnapId);
}

TEST_CASE("gnb/xnap/codec: a Cause survives UPER encode and decode")
{
    asn1::xnap::cause_c encodedCause;
    encodedCause.set_radio_network() = asn1::xnap::cause_radio_network_layer_opts::unspecified;

    srsran::byte_buffer buffer{srsran::byte_buffer::fallback_allocation_tag{}};
    asn1::bit_ref writer{srsran::byte_buffer_writer{buffer}};

    REQUIRE_EQ(encodedCause.pack(writer), asn1::SRSASN_SUCCESS);
    CHECK(!buffer.empty());

    asn1::xnap::cause_c decodedCause;
    asn1::cbit_ref reader{buffer};
    REQUIRE_EQ(decodedCause.unpack(reader), asn1::SRSASN_SUCCESS);
    CHECK_EQ(decodedCause.type().value, asn1::xnap::cause_c::types_opts::radio_network);
    CHECK_EQ(decodedCause.radio_network().value, asn1::xnap::cause_radio_network_layer_opts::unspecified);
}

} // namespace

namespace
{

nr::gnb::XnForwardingEndpoint makeProposal(int psi, PduSessionType type, std::vector<int> qfis)
{
    nr::gnb::XnForwardingEndpoint endpoint{};
    endpoint.psi = psi;
    endpoint.sessionType = type;
    endpoint.qosFlowIds = std::move(qfis);
    return endpoint;
}

} // namespace

TEST_CASE("gnb/xnap/codec: a Handover Request carries the type of each PDU session")
{
    nr::gnb::xnap::HandoverData requestData{};
    requestData.sourceUeXnapId = 1;
    requestData.transparentContainer = {0x01};
    requestData.forwardingEndpoints.push_back(makeProposal(5, PduSessionType::IPv4, {1}));
    requestData.forwardingEndpoints.push_back(makeProposal(6, PduSessionType::ETHERNET, {2}));

    auto request = nr::gnb::xnap::EncodeHandoverRequest(requestData, makeCodecConfig(0x456789));
    REQUIRE(request.size() > 0);
    auto decoded = nr::gnb::xnap::Decode(request.data(), request.size());
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->handover.has_value());

    const auto &endpoints = decoded->handover->forwardingEndpoints;
    REQUIRE_EQ(endpoints.size(), 2U);
    CHECK_EQ(endpoints[0].psi, 5);
    CHECK(endpoints[0].sessionType == PduSessionType::IPv4);
    CHECK_EQ(endpoints[1].psi, 6);
    CHECK(endpoints[1].sessionType == PduSessionType::ETHERNET);
}
