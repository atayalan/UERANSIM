//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include <bitset>
#include <vector>

namespace nas_sm
{

// Compares the PDU sessions the UE holds with the PDU session status the network reported, and returns the IDs
// (ascending) the network no longer lists. TS 24.501: the UE locally releases those; sessions the network lists but
// the UE does not hold are left alone. Bit 0 is not a PDU session ID and is ignored.
std::vector<int> SessionsToReleaseFromStatus(const std::bitset<16> &statusInUe, const std::bitset<16> &statusInNw);

} // namespace nas_sm
