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

TEST_CASE("lib/app/cli_cmd: mobility-update parses with no arguments")
{
    std::string error, output;
    auto cmd = app::ParseUeCliCommand({"mobility-update"}, error, output);
    REQUIRE(cmd != nullptr);
    CHECK(cmd->present == app::UeCliCommand::MOBILITY_UPDATE);
}

TEST_CASE("lib/app/cli_cmd: handover commands map to the Xn and N2 procedures")
{
    std::string error, output;
    auto xn = app::ParseGnbCliCommand({"xn-handover", "1", "2"}, error, output);
    REQUIRE(xn != nullptr);
    CHECK(xn->present == app::GnbCliCommand::XN_HANDOVER);
    CHECK(xn->ueId == 1);
    CHECK(xn->gnbId == 2);

    auto legacy = app::ParseGnbCliCommand({"handover", "3", "4"}, error, output);
    REQUIRE(legacy != nullptr);
    CHECK(legacy->present == app::GnbCliCommand::XN_HANDOVER);

    auto n2 = app::ParseGnbCliCommand({"n2-handover", "3", "4"}, error, output);
    REQUIRE(n2 != nullptr);
    CHECK(n2->present == app::GnbCliCommand::N2_HANDOVER);
    CHECK(n2->ueId == 3);
    CHECK(n2->gnbId == 4);
}

TEST_CASE("lib/app/cli_cmd: handover commands reject bad arguments with the matching message")
{
    struct Row
    {
        std::vector<std::string> args;
        const char *message;
    };
    const std::vector<Row> rows = {
        {{"xn-handover", "1"}, "UE ID and target gNB ID are expected"},
        {{"xn-handover"}, "UE ID and target gNB ID are expected"},
        {{"xn-handover", "1", "2", "3"}, "Only UE ID and target gNB ID are expected"},
        {{"xn-handover", "0", "2"}, "Invalid UE ID"},
        {{"xn-handover", "abc", "2"}, "Invalid UE ID"},
        {{"n2-handover", "1", "x"}, "Invalid target gNB ID"},
        // A leading "-" makes the option parser treat the token as an option, so the message comes from there.
        {{"n2-handover", "1", "-5"}, nullptr},
        {{"n2-handover", "1", "0"}, "Invalid target gNB ID"},
    };
    for (const auto &row : rows)
    {
        CAPTURE(row.args.back());
        std::string error, output;
        auto args = row.args;
        auto cmd = app::ParseGnbCliCommand(std::move(args), error, output);
        CHECK(cmd == nullptr);
        if (row.message != nullptr)
            CHECK(error == row.message);
        else
            CHECK_FALSE(error.empty());
    }
}

TEST_CASE("lib/app/cli_cmd: xn status commands parse and unknown xn commands do not")
{
    std::string error, output;
    auto status = app::ParseGnbCliCommand({"xn-status"}, error, output);
    REQUIRE(status != nullptr);
    CHECK(status->present == app::GnbCliCommand::XN_STATUS);

    auto neighbors = app::ParseGnbCliCommand({"xn-neighbor-list"}, error, output);
    REQUIRE(neighbors != nullptr);
    CHECK(neighbors->present == app::GnbCliCommand::XN_NEIGHBOR_LIST);

    CHECK(app::ParseGnbCliCommand({"xn-foo"}, error, output) == nullptr);
}
