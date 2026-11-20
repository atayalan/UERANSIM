//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <cstdint>

#include <ext/doctest/doctest.h>
#include <gnb/ngap/encode.hpp>
#include <gnb/ngap/handover_identity.hpp>

#include <asn/ngap/ASN_NGAP_CellSize.h>
#include <asn/ngap/ASN_NGAP_LastVisitedCellItem.h>
#include <asn/ngap/ASN_NGAP_LastVisitedNGRANCellInformation.h>
#include <asn/ngap/ASN_NGAP_NGRAN-CGI.h>
#include <asn/ngap/ASN_NGAP_NR-CGI.h>
#include <asn/ngap/ASN_NGAP_SourceNGRANNode-ToTargetNGRANNode-TransparentContainer.h>

namespace
{

// Wraps the RRC bytes the way the source gNB does for the N2 Handover Required container.
OctetString makeSourceToTargetContainer(const OctetString &rrcBytes)
{
    auto *container = asn::New<ASN_NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer>();
    asn::SetOctetString(container->rRCContainer, rrcBytes);
    container->targetCell_ID.present = ASN_NGAP_NGRAN_CGI_PR_nR_CGI;
    container->targetCell_ID.choice.nR_CGI = asn::New<ASN_NGAP_NR_CGI>();
    asn::SetOctetString3(container->targetCell_ID.choice.nR_CGI->pLMNIdentity, octet3{0x99, 0xF9, 0x09});
    asn::SetBitStringLong<36>(0x100000ull, container->targetCell_ID.choice.nR_CGI->nRCellIdentity);

    // UE history must hold at least one cell.
    auto *cell = asn::New<ASN_NGAP_LastVisitedCellItem>();
    cell->lastVisitedCellInformation.present = ASN_NGAP_LastVisitedCellInformation_PR_nGRANCell;
    auto *ngran = asn::New<ASN_NGAP_LastVisitedNGRANCellInformation_t>();
    cell->lastVisitedCellInformation.choice.nGRANCell = ngran;
    ngran->globalCellID.present = ASN_NGAP_NGRAN_CGI_PR_nR_CGI;
    ngran->globalCellID.choice.nR_CGI = asn::New<ASN_NGAP_NR_CGI>();
    asn::SetOctetString3(ngran->globalCellID.choice.nR_CGI->pLMNIdentity, octet3{0x99, 0xF9, 0x09});
    asn::SetBitStringLong<36>(0x100000ull, ngran->globalCellID.choice.nR_CGI->nRCellIdentity);
    ngran->cellType.cellSize = ASN_NGAP_CellSize_medium;
    ngran->timeUEStayedInCell = 42;
    ASN_SEQUENCE_ADD(&container->uEHistoryInformation.list, cell);

    auto encoded = nr::gnb::ngap_encode::EncodeS(
        asn_DEF_ASN_NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer, container);
    asn::Free(asn_DEF_ASN_NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer, container);
    return encoded;
}

} // namespace

TEST_CASE("gnb/ngap/handover_identity: the UE's RLS identity survives the Handover Required container")
{
    auto rrc = nr::gnb::ngap_utils::EncodeHandoverPreparationInformation(0x0123456789ABCDEFull);
    REQUIRE(rrc.length() > 0);

    auto container = makeSourceToTargetContainer(rrc);
    REQUIRE(container.length() > 0);

    CHECK(nr::gnb::ngap_utils::RlsIdentityFromSourceToTarget(container) == 0x0123456789ABCDEFull);
}

TEST_CASE("gnb/ngap/handover_identity: no identity is carried for an unknown UE")
{
    auto rrc = nr::gnb::ngap_utils::EncodeHandoverPreparationInformation(0);
    REQUIRE(rrc.length() > 0);

    CHECK(nr::gnb::ngap_utils::RlsIdentityFromSourceToTarget(makeSourceToTargetContainer(rrc)) == 0);
}

TEST_CASE("gnb/ngap/handover_identity: a container from another implementation yields no identity")
{
    // Garbage, empty and a container whose RRC part is not a HandoverPreparationInformation.
    CHECK(nr::gnb::ngap_utils::RlsIdentityFromSourceToTarget(OctetString{}) == 0);
    CHECK(nr::gnb::ngap_utils::RlsIdentityFromSourceToTarget(OctetString::FromHex("deadbeef")) == 0);
    CHECK(nr::gnb::ngap_utils::RlsIdentityFromSourceToTarget(
              makeSourceToTargetContainer(OctetString::FromHex("00"))) == 0);
}
