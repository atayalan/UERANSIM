//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include <stdexcept>

class LibError : public std::runtime_error
{
  private:
    int err;

  public:
    explicit LibError(const std::string &what);
    LibError(const std::string &what, int err);

    // The errno the error was raised with, or 0 if none was given.
    [[nodiscard]] int error() const noexcept;
};