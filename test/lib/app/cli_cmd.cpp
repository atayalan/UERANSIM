//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <string>
#include <vector>

#include <ext/doctest/doctest.h>
#include <lib/app/cli_cmd.hpp>

TEST_CASE("lib/app/cli_cmd: PDU session type names are matched regardless of letter case")
{
    for (const char *name : {"IPv4", "ipv4", "IPV4", "Ipv4", "IpV4", "iPv4"})
    {
        auto type = app::ParsePduSessionTypeName(name);
        REQUIRE(type.has_value());
        CHECK(*type == "IPv4");
    }

    for (const char *name : {"Ethernet", "ethernet", "ETHERNET", "EtherNet", "ETHERnet", "eThErNeT"})
    {
        auto type = app::ParsePduSessionTypeName(name);
        REQUIRE(type.has_value());
        CHECK(*type == "Ethernet");
    }
}

TEST_CASE("lib/app/cli_cmd: other PDU session type names are rejected")
{
    for (const char *name : {"", "IPv6", "IPv4v6", "Unstructured", "Ether", "Ethernet ", " IPv4", "ip v4"})
        CHECK_FALSE(app::ParsePduSessionTypeName(name).has_value());
}

TEST_CASE("lib/app/cli_cmd: ps-establish carries the canonical session type")
{
    std::string error, output;
    auto cmd = app::ParseUeCliCommand({"ps-establish", "ETHERNET", "--sst", "1"}, error, output);
    REQUIRE(cmd != nullptr);
    CHECK(cmd->present == app::UeCliCommand::PS_ESTABLISH);
    REQUIRE(cmd->sessionTypeStr.has_value());
    CHECK(*cmd->sessionTypeStr == "Ethernet");

    cmd = app::ParseUeCliCommand({"ps-establish", "ipv4", "--sst", "1"}, error, output);
    REQUIRE(cmd != nullptr);
    CHECK(*cmd->sessionTypeStr == "IPv4");
}

TEST_CASE("lib/app/cli_cmd: ps-establish refuses unsupported session types")
{
    std::string error, output;
    auto cmd = app::ParseUeCliCommand({"ps-establish", "IPv6"}, error, output);
    CHECK(cmd == nullptr);
    CHECK(error == "Only IPv4 and Ethernet are supported");
}
