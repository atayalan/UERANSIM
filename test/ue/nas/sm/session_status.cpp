//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <bitset>
#include <vector>

#include <ext/doctest/doctest.h>
#include <ue/nas/sm/session_status.hpp>

TEST_CASE("ue/nas/sm/session_status: a session the network still lists is kept")
{
    std::bitset<16> ue, nw;
    ue[1] = true;
    nw[1] = true;

    CHECK(nas_sm::SessionsToReleaseFromStatus(ue, nw).empty());
}

TEST_CASE("ue/nas/sm/session_status: a session the network no longer lists is released")
{
    std::bitset<16> ue, nw;
    ue[1] = true;
    ue[2] = true;
    nw[2] = true;

    CHECK(nas_sm::SessionsToReleaseFromStatus(ue, nw) == std::vector<int>{1});
}

TEST_CASE("ue/nas/sm/session_status: a session only the network lists is not touched")
{
    std::bitset<16> ue, nw;
    nw[3] = true;

    CHECK(nas_sm::SessionsToReleaseFromStatus(ue, nw).empty());
}

TEST_CASE("ue/nas/sm/session_status: bit 0 is not a PDU session and is never released")
{
    std::bitset<16> ue, nw;
    ue[0] = true;

    CHECK(nas_sm::SessionsToReleaseFromStatus(ue, nw).empty());
}

TEST_CASE("ue/nas/sm/session_status: several lost sessions come back in ascending order")
{
    std::bitset<16> ue, nw;
    ue[5] = true;
    ue[2] = true;
    ue[15] = true;
    nw[2] = true;

    CHECK(nas_sm::SessionsToReleaseFromStatus(ue, nw) == std::vector<int>{5, 15});
}
