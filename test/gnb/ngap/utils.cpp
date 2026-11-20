//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <ext/doctest/doctest.h>
#include <gnb/ngap/utils.hpp>

TEST_CASE("gnb/ngap/utils: every NGAP PDU session type maps to the matching gNB type")
{
    using nr::gnb::ngap_utils::PduSessionTypeFromAsn;

    CHECK(PduSessionTypeFromAsn(ASN_NGAP_PDUSessionType_ipv4) == PduSessionType::IPv4);
    CHECK(PduSessionTypeFromAsn(ASN_NGAP_PDUSessionType_ipv6) == PduSessionType::IPv6);
    CHECK(PduSessionTypeFromAsn(ASN_NGAP_PDUSessionType_ipv4v6) == PduSessionType::IPv4v6);
    CHECK(PduSessionTypeFromAsn(ASN_NGAP_PDUSessionType_ethernet) == PduSessionType::ETHERNET);
    CHECK(PduSessionTypeFromAsn(ASN_NGAP_PDUSessionType_unstructured) == PduSessionType::UNSTRUCTURED);
}

TEST_CASE("gnb/ngap/utils: the gNB sets up and hands over IPv4 and Ethernet sessions only")
{
    using nr::gnb::ngap_utils::IsSupportedPduSessionType;

    CHECK(IsSupportedPduSessionType(PduSessionType::IPv4));
    CHECK(IsSupportedPduSessionType(PduSessionType::ETHERNET));
    CHECK_FALSE(IsSupportedPduSessionType(PduSessionType::IPv6));
    CHECK_FALSE(IsSupportedPduSessionType(PduSessionType::IPv4v6));
    CHECK_FALSE(IsSupportedPduSessionType(PduSessionType::UNSTRUCTURED));
}
