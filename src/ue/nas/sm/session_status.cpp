//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "session_status.hpp"

namespace nas_sm
{

std::vector<int> SessionsToReleaseFromStatus(const std::bitset<16> &statusInUe, const std::bitset<16> &statusInNw)
{
    std::vector<int> result;
    for (int psi = 1; psi < 16; psi++)
        if (statusInUe[psi] && !statusInNw[psi])
            result.push_back(psi);
    return result;
}

} // namespace nas_sm
