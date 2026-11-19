//
// This file is a part of UERANSIM project.
// Copyright (c) 2023 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <gnb/types.hpp>

namespace nr::gnb
{

// The Xn handover snapshot is the transparent container of the Xn Handover Request: the NGAP-side state the target gNB
// needs (UE AMBR, security capabilities, PDU sessions) plus the RLS identity (sti) of the UE, which the target uses to
// find the id it gave that UE itself, since ids are allocated per gNB and differ between gNBs.

// Returns nullopt when the snapshot is too large or holds values the layout cannot carry.
std::optional<std::vector<uint8_t>> SerializeHandoverSnapshot(const XnHandoverNgapSnapshot &snapshot);

// Returns nullptr for anything that is not exactly one snapshot of the current layout.
std::unique_ptr<XnHandoverNgapSnapshot> DeserializeHandoverSnapshot(const std::vector<uint8_t> &data);

} // namespace nr::gnb
