//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <ext/doctest/doctest.h>
#include <utils/options.hpp>

TEST_CASE("utils/options: an option name with an underscore is accepted and one with a space is rejected")
{
    // A name like --mobi_update must be accepted when building the option set.
    opt::OptionItem config{'c', "config", "config file", "config-file"};
    opt::OptionItem mobilityUpdate{std::nullopt, "mobi_update", "mobility update test", std::nullopt};
    opt::OptionsDescription desc{{}, {}, {}, {}, "nr-ue", {}, {}, false, false};
    desc.items = {config, mobilityUpdate};

    opt::OptionsResult normal{std::vector<std::string>{"nr-ue", "-c", "ue.yaml"}, desc, nullptr};
    CHECK(!normal.hasFlag(mobilityUpdate));
    CHECK_EQ(normal.getOption(config), std::string{"ue.yaml"});

    opt::OptionsResult enabled{
        std::vector<std::string>{"nr-ue", "--mobi_update", "-c", "ue.yaml"}, desc, nullptr};
    CHECK(enabled.hasFlag(mobilityUpdate));
    CHECK_EQ(enabled.getOption(config), std::string{"ue.yaml"});

    bool rejected = false;
    try
    {
        opt::OptionItem invalid{std::nullopt, "mobi update", "invalid option", std::nullopt};
    }
    catch (const std::runtime_error &)
    {
        rejected = true;
    }
    CHECK(rejected);
}
