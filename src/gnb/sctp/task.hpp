//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

#include <gnb/nts.hpp>
#include <lib/sctp/sctp.hpp>
#include <utils/logger.hpp>
#include <utils/nts.hpp>
#include <utils/scoped_thread.hpp>

namespace nr::gnb
{

class SctpTask : public NtsTask
{
  private:
    struct ClientEntry
    {
        int id;
        sctp::SctpClient *client;
        ScopedThread *receiverThread;
        sctp::ISctpHandler *handler;
        NtsTask *associatedTask;
    };

    struct ListenerEntry
    {
        int id;
        sctp::SctpServer *server;
        ScopedThread *acceptThread;
        NtsTask *associatedTask;
        sctp::PayloadProtocolId ppid;
    };

  private:
    TaskBase *m_base;
    std::unique_ptr<Logger> m_logger;
    std::unordered_map<int, ClientEntry *> m_clients;
    std::unordered_map<int, ListenerEntry *> m_listeners;
    int m_acceptedClientId{100000};

    friend class GnbCmdHandler;

  public:
    explicit SctpTask(TaskBase *base);
    ~SctpTask() override = default;

  protected:
    void onStart() override;
    void onLoop() override;
    void onQuit() override;

  private:
    static void DeleteClientEntry(ClientEntry *entry);
    static void DeleteListenerEntry(ListenerEntry *entry);
    void registerClient(int clientId, std::unique_ptr<sctp::SctpClient> client, NtsTask *associatedTask);

  private:
    void receiveSctpConnectionSetupRequest(int clientId, const std::string &localAddress, uint16_t localPort,
                                           const std::string &remoteAddress, uint16_t remotePort,
                                           sctp::PayloadProtocolId ppid, NtsTask *associatedTask);
    void receiveListenRequest(int listenerId, const std::string &localAddress, uint16_t localPort,
                              sctp::PayloadProtocolId ppid, NtsTask *associatedTask);
    void receiveAcceptedConnection(int listenerId, std::unique_ptr<sctp::SctpClient> client);
    void receiveAssociationSetup(int clientId, int associationId, int inStreams, int outStreams);
    void receiveAssociationShutdown(int clientId);
    void receiveClientReceive(int clientId, uint16_t stream, UniqueBuffer &&buffer);
    void receiveUnhandledNotification(int clientId);
    void receiveConnectionClose(int clientId);
    void receiveSendMessage(int clientId, uint16_t stream, UniqueBuffer &&buffer);
};

} // namespace nr::gnb
