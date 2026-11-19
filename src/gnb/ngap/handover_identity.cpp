//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "handover_identity.hpp"

#include "encode.hpp"

#include <array>

#include <lib/rrc/encode.hpp>

#include <asn/ngap/ASN_NGAP_SourceNGRANNode-ToTargetNGRANNode-TransparentContainer.h>
#include <asn/rrc/ASN_RRC_AS-Context.h>
#include <asn/rrc/ASN_RRC_HandoverPreparationInformation-IEs.h>
#include <asn/rrc/ASN_RRC_HandoverPreparationInformation.h>

namespace nr::gnb::ngap_utils
{

namespace
{

constexpr std::array<uint8_t, 4> MARKER = {'U', 'E', 'I', 'D'};
constexpr size_t STI_BYTES = 8;
constexpr size_t PAYLOAD_BYTES = MARKER.size() + STI_BYTES;

OctetString EncodeIdentity(uint64_t sti)
{
    std::vector<uint8_t> bytes(MARKER.begin(), MARKER.end());
    for (int shift = 56; shift >= 0; shift -= 8)
        bytes.push_back(static_cast<uint8_t>(sti >> shift));
    return OctetString{std::move(bytes)};
}

uint64_t DecodeIdentity(const OCTET_STRING_t &payload)
{
    if (payload.size != PAYLOAD_BYTES || !std::equal(MARKER.begin(), MARKER.end(), payload.buf))
        return 0;

    uint64_t sti = 0;
    for (size_t i = MARKER.size(); i < PAYLOAD_BYTES; i++)
        sti = (sti << 8) | payload.buf[i];
    return sti;
}

} // namespace

OctetString EncodeHandoverPreparationInformation(uint64_t sti)
{
    auto *info = asn::New<ASN_RRC_HandoverPreparationInformation>();
    info->criticalExtensions.present = ASN_RRC_HandoverPreparationInformation__criticalExtensions_PR_c1;
    info->criticalExtensions.choice.c1 = asn::NewFor(info->criticalExtensions.choice.c1);
    info->criticalExtensions.choice.c1->present =
        ASN_RRC_HandoverPreparationInformation__criticalExtensions__c1_PR_handoverPreparationInformation;
    auto *ies = asn::New<ASN_RRC_HandoverPreparationInformation_IEs>();
    info->criticalExtensions.choice.c1->choice.handoverPreparationInformation = ies;

    // This simulator collects no real UE capability: an empty list is allowed, and test bytes must not be declared as
    // a UE-NR-Capability.
    if (sti != 0)
    {
        ies->as_Context = asn::New<ASN_RRC_AS_Context>();
        ies->as_Context->ext2 = asn::NewFor(ies->as_Context->ext2);
        ies->as_Context->ext2->ueAssistanceInformation = asn::New<OCTET_STRING_t>();
        asn::SetOctetString(*ies->as_Context->ext2->ueAssistanceInformation, EncodeIdentity(sti));
    }

    auto encoded = rrc::encode::EncodeS(asn_DEF_ASN_RRC_HandoverPreparationInformation, info);
    asn::Free(asn_DEF_ASN_RRC_HandoverPreparationInformation, info);
    return encoded;
}

uint64_t RlsIdentityFromSourceToTarget(const OctetString &container)
{
    if (container.length() == 0)
        return 0;

    auto *outer = ngap_encode::Decode<ASN_NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer>(
        asn_DEF_ASN_NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer, container.data(),
        container.length());
    if (outer == nullptr)
        return 0;

    uint64_t sti = 0;
    auto *info = rrc::encode::Decode<ASN_RRC_HandoverPreparationInformation>(
        asn_DEF_ASN_RRC_HandoverPreparationInformation, outer->rRCContainer);
    if (info != nullptr)
    {
        if (info->criticalExtensions.present == ASN_RRC_HandoverPreparationInformation__criticalExtensions_PR_c1 &&
            info->criticalExtensions.choice.c1 != nullptr &&
            info->criticalExtensions.choice.c1->present ==
                ASN_RRC_HandoverPreparationInformation__criticalExtensions__c1_PR_handoverPreparationInformation)
        {
            auto *ies = info->criticalExtensions.choice.c1->choice.handoverPreparationInformation;
            if (ies != nullptr && ies->as_Context != nullptr && ies->as_Context->ext2 != nullptr &&
                ies->as_Context->ext2->ueAssistanceInformation != nullptr)
                sti = DecodeIdentity(*ies->as_Context->ext2->ueAssistanceInformation);
        }
        asn::Free(asn_DEF_ASN_RRC_HandoverPreparationInformation, info);
    }

    asn::Free(asn_DEF_ASN_NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer, outer);
    return sti;
}

} // namespace nr::gnb::ngap_utils
