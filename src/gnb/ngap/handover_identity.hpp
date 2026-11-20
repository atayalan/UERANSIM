//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include <cstdint>

#include <utils/octet_string.hpp>

// UE ids are allocated per gNB by the radio link, so an N2 target gNB cannot use anything the AMF sends to find the
// UE it is about to receive. The source gNB therefore carries the UE's RLS identity (sti, the same on every gNB) in
// the RRC HandoverPreparationInformation inside the Source NG-RAN to Target NG-RAN transparent container, which the
// AMF passes on untouched. The identity travels in AS-Context.ueAssistanceInformation, behind a private marker.

namespace nr::gnb::ngap_utils
{

// Encodes a HandoverPreparationInformation with an empty UE capability list, carrying sti when it is not 0.
// Returns an empty string when encoding fails.
OctetString EncodeHandoverPreparationInformation(uint64_t sti);

// Reads the sti back out of an encoded Source NG-RAN to Target NG-RAN transparent container.
// Returns 0 when the container carries no identity or is not decodable.
uint64_t RlsIdentityFromSourceToTarget(const OctetString &container);

} // namespace nr::gnb::ngap_utils
