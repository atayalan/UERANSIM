//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "server.hpp"
#include "internal.hpp"

sctp::SctpServer::SctpServer(const std::string &address, uint16_t port) : sd(0)
{
    try
    {
        sd = CreateSocket(address);
        BindSocket(sd, address, port);
        SetInitOptions(sd, 10, 10, 10, 10 * 1000);
        SetEventOptions(sd);
        StartListening(sd);
    }
    catch (const SctpError &e)
    {
        CloseSocket(sd);
        throw;
    }
}

sctp::SctpServer::~SctpServer()
{
    CloseSocket(sd);
}

void sctp::SctpServer::start()
{
    CloseSocket(Accept(sd));
}

std::unique_ptr<sctp::SctpClient> sctp::SctpServer::accept(PayloadProtocolId ppid)
{
    return std::make_unique<SctpClient>(ppid, Accept(sd));
}
