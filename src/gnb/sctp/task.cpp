//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "task.hpp"

#include <chrono>
#include <cstring>
#include <exception>
#include <thread>
#include <utility>

// #define MOCKED_PACKETS

#ifdef MOCKED_PACKETS
static std::string MOCK_LIST[] = {
    std::string(
        "201500320000040001000e05806f70656e3567732d616d663000600008000009f10702004000564001ff005000080009f10700000008"),

    std::string("0004403e000003000a000200060055000200010026002b2a7e005600020000215660aed893ca993801ad60ba50575cb020101c"
                "016820d4f980"
                "004ff8987b0bfcbbb8"),

    std::string("00044029000003000a0002000600550002000100260016157e03cbc05c7b007e005d020004f0f0f0f0e1360102"),

    std::string("000e00809e000009000a00020006005500020001006e000a0c3e800000303e800000001c00070009f107020040000000020001"
                "007700091c00"
                "0e000700038000005e002091f9908eefe559d08b9f958b4d7c1a94094918dedcf40bb57c55428a00d4dee90022400835693803"
                "56fffff00026"
                "402f2e7e02687a0bd3017e0042010177000bf209f107020040e700acbe54072009f10700000115020101210201005e0129"),

    std::string("0004403c000003000a0002000600550002000100260029287e02f41c3d78027e0054430f10004f00700065006e003500470053"
                "462147121062"
                "41547021490100"),
};
static int MOCK_INDEX = -1;
#endif

namespace nr::gnb
{

class SctpHandler : public sctp::ISctpHandler
{
  private:
    SctpTask *const sctpTask;
    int clientId;

  public:
    SctpHandler(SctpTask *const sctpTask, int clientId) : sctpTask(sctpTask), clientId(clientId)
    {
    }

  private:
    void onAssociationSetup(int associationId, int inStreams, int outStreams) override
    {
        auto w = std::make_unique<NmGnbSctp>(NmGnbSctp::ASSOCIATION_SETUP);
        w->clientId = clientId;
        w->associationId = associationId;
        w->inStreams = inStreams;
        w->outStreams = outStreams;
        sctpTask->push(std::move(w));
    }

    void onAssociationShutdown() override
    {
        auto w = std::make_unique<NmGnbSctp>(NmGnbSctp::ASSOCIATION_SHUTDOWN);
        w->clientId = clientId;
        sctpTask->push(std::move(w));
    }

    void onMessage(const uint8_t *buffer, size_t length, uint16_t stream) override
    {
        auto *data = new uint8_t[length];
        std::memcpy(data, buffer, length);

        auto w = std::make_unique<NmGnbSctp>(NmGnbSctp::RECEIVE_MESSAGE);
        w->clientId = clientId;
        w->buffer = UniqueBuffer{data, length};
        w->stream = stream;
        sctpTask->push(std::move(w));
    }

    void onUnhandledNotification() override
    {
        auto w = std::make_unique<NmGnbSctp>(NmGnbSctp::UNHANDLED_NOTIFICATION);
        w->clientId = clientId;
        sctpTask->push(std::move(w));
    }

    void onConnectionReset() override
    {
        auto w = std::make_unique<NmGnbSctp>(NmGnbSctp::UNHANDLED_NOTIFICATION);
        w->clientId = clientId;
        sctpTask->push(std::move(w));
    }
};

[[noreturn]] static void ReceiverThread(std::pair<sctp::SctpClient *, sctp::ISctpHandler *> *args)
{
    sctp::SctpClient *client = args->first;
    sctp::ISctpHandler *handler = args->second;

    delete args;

    while (true)
        client->receive(handler);
}

SctpTask::SctpTask(TaskBase *base) : m_base{base}, m_clients{}
{
    m_logger = base->logBase->makeUniqueLogger("sctp");
}

void SctpTask::onStart()
{
}

void SctpTask::onLoop()
{
    auto msg = take();
    if (!msg)
        return;

    switch (msg->msgType)
    {
    case NtsMessageType::GNB_SCTP: {
        auto& w = dynamic_cast<NmGnbSctp &>(*msg);
        switch (w.present)
        {
        case NmGnbSctp::CONNECTION_REQUEST: {
            receiveSctpConnectionSetupRequest(w.clientId, w.localAddress, w.localPort, w.remoteAddress,
                                              w.remotePort, w.ppid, w.associatedTask);
            break;
        }
        case NmGnbSctp::LISTEN_REQUEST: {
            receiveListenRequest(w.listenerId, w.localAddress, w.localPort, w.ppid, w.associatedTask);
            break;
        }
        case NmGnbSctp::CONNECTION_ACCEPTED: {
            receiveAcceptedConnection(w.listenerId, std::move(w.acceptedClient));
            break;
        }
        case NmGnbSctp::CONNECTION_CLOSE: {
            receiveConnectionClose(w.clientId);
            break;
        }
        case NmGnbSctp::ASSOCIATION_SETUP: {
            receiveAssociationSetup(w.clientId, w.associationId, w.inStreams, w.outStreams);
            break;
        }
        case NmGnbSctp::ASSOCIATION_SHUTDOWN: {
            receiveAssociationShutdown(w.clientId);
            break;
        }
        case NmGnbSctp::RECEIVE_MESSAGE: {
            receiveClientReceive(w.clientId, w.stream, std::move(w.buffer));
            break;
        }
        case NmGnbSctp::SEND_MESSAGE: {
            receiveSendMessage(w.clientId, w.stream, std::move(w.buffer));
            break;
        }
        case NmGnbSctp::UNHANDLED_NOTIFICATION: {
            receiveUnhandledNotification(w.clientId);
            break;
        }
        default:
            m_logger->unhandledNts(*msg);
            break;
        }
        break;
    }
    default:
        m_logger->unhandledNts(*msg);
        break;
    }
}

void SctpTask::onQuit()
{
    for (auto &client : m_clients)
    {
        ClientEntry *entry = client.second;
        DeleteClientEntry(entry);
    }
    m_clients.clear();
    for (auto &listener : m_listeners)
        DeleteListenerEntry(listener.second);
    m_listeners.clear();
}

void SctpTask::DeleteClientEntry(ClientEntry *entry)
{
    entry->associatedTask = nullptr;
    delete entry->receiverThread;
    delete entry->client;
    delete entry->handler;
    delete entry;
}

void SctpTask::DeleteListenerEntry(ListenerEntry *entry)
{
    entry->associatedTask = nullptr;
    delete entry->acceptThread;
    delete entry->server;
    delete entry;
}

void SctpTask::registerClient(int clientId, std::unique_ptr<sctp::SctpClient> client, NtsTask *associatedTask)
{
    auto *rawClient = client.release();
    auto *handler = new SctpHandler(this, clientId);
    auto *entry = new ClientEntry;
    entry->id = clientId;
    entry->client = rawClient;
    entry->handler = handler;
    entry->associatedTask = associatedTask;
    entry->receiverThread = new ScopedThread(
        [](void *arg) { ReceiverThread(reinterpret_cast<std::pair<sctp::SctpClient *, sctp::ISctpHandler *> *>(arg)); },
        new std::pair<sctp::SctpClient *, sctp::ISctpHandler *>(rawClient, handler));
    m_clients[clientId] = entry;
}

void SctpTask::receiveSctpConnectionSetupRequest(int clientId, const std::string &localAddress, uint16_t localPort,
                                                 const std::string &remoteAddress, uint16_t remotePort,
                                                 sctp::PayloadProtocolId ppid, NtsTask *associatedTask)
{
    m_logger->info("Trying to establish SCTP connection... (%s:%d)", remoteAddress.c_str(), remotePort);

    auto *client = new sctp::SctpClient(ppid, localAddress);

    try
    {
        client->bind(localAddress, localPort);
    }
    catch (const sctp::SctpError &exc)
    {
        m_logger->err("Binding to %s:%d failed. %s", localAddress.c_str(), localPort, exc.what());
        delete client;
        auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::CONNECTION_FAILURE);
        msg->clientId = clientId;
        msg->error = exc.what();
        associatedTask->push(std::move(msg));
        return;
    }

    try
    {
        client->connect(remoteAddress, remotePort);
    }
    catch (const sctp::SctpError &exc)
    {
        m_logger->err("Connecting to %s:%d failed. %s", remoteAddress.c_str(), remotePort, exc.what());
        delete client;
        auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::CONNECTION_FAILURE);
        msg->clientId = clientId;
        msg->error = exc.what();
        associatedTask->push(std::move(msg));
        return;
    }

    m_logger->info("SCTP connection established (%s:%d)", remoteAddress.c_str(), remotePort);

    registerClient(clientId, std::unique_ptr<sctp::SctpClient>(client), associatedTask);
}

void SctpTask::receiveListenRequest(int listenerId, const std::string &localAddress, uint16_t localPort,
                                    sctp::PayloadProtocolId ppid, NtsTask *associatedTask)
{
    if (m_listeners.count(listenerId) != 0)
    {
        m_logger->warn("SCTP listener already exists with id: %d", listenerId);
        return;
    }

    auto *entry = new ListenerEntry;
    entry->id = listenerId;
    entry->associatedTask = associatedTask;
    entry->ppid = ppid;

    try
    {
        entry->server = new sctp::SctpServer(localAddress, localPort);
    }
    catch (const sctp::SctpError &exc)
    {
        m_logger->err("Listening on %s:%d failed. %s", localAddress.c_str(), localPort, exc.what());
        delete entry;
        return;
    }

    entry->acceptThread = new ScopedThread(
        [](void *arg) {
            auto *args = reinterpret_cast<std::pair<SctpTask *, ListenerEntry *> *>(arg);
            SctpTask *task = args->first;
            ListenerEntry *listener = args->second;
            delete args;

            while (true)
            {
                // A failed accept (for example out of file descriptors, or a connection reset before accept) must not
                // end the thread: an exception leaving it would terminate the whole gNB.
                std::unique_ptr<sctp::SctpClient> client;
                try
                {
                    client = listener->server->accept(listener->ppid);
                }
                catch (const std::exception &e)
                {
                    task->m_logger->err("SCTP accept failed: %s", e.what());
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }
                auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::CONNECTION_ACCEPTED);
                msg->listenerId = listener->id;
                msg->acceptedClient = std::move(client);
                task->push(std::move(msg));
            }
        },
        new std::pair<SctpTask *, ListenerEntry *>(this, entry));

    m_listeners[listenerId] = entry;
    m_logger->info("SCTP listener started (%s:%d)", localAddress.c_str(), localPort);
}

void SctpTask::receiveAcceptedConnection(int listenerId, std::unique_ptr<sctp::SctpClient> client)
{
    auto it = m_listeners.find(listenerId);
    if (it == m_listeners.end() || client == nullptr)
        return;

    int clientId = m_acceptedClientId++;
    const std::string remoteAddress = client->peerAddress();
    registerClient(clientId, std::move(client), it->second->associatedTask);

    auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::CONNECTION_ACCEPTED);
    msg->listenerId = listenerId;
    msg->clientId = clientId;
    msg->remoteAddress = remoteAddress;
    it->second->associatedTask->push(std::move(msg));
}

void SctpTask::receiveAssociationSetup(int clientId, int associationId, int inStreams, int outStreams)
{
    m_logger->debug("SCTP association setup ascId[%d]", associationId);

    ClientEntry *entry = m_clients[clientId];
    if (entry == nullptr)
    {
        m_logger->warn("Client entry not found for id: %d", clientId);
        return;
    }

    // Notify the relevant task
    auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::ASSOCIATION_SETUP);
    msg->clientId = clientId;
    msg->associationId = associationId;
    msg->inStreams = inStreams;
    msg->outStreams = outStreams;
    entry->associatedTask->push(std::move(msg));
}

void SctpTask::receiveAssociationShutdown(int clientId)
{
    m_logger->debug("SCTP association shutdown (clientId: %d)", clientId);

    ClientEntry *entry = m_clients[clientId];
    if (entry == nullptr)
    {
        m_logger->warn("Client entry not found for id: %d", clientId);
        return;
    }

    // Notify the relevant task
    auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::ASSOCIATION_SHUTDOWN);
    msg->clientId = clientId;
    entry->associatedTask->push(std::move(msg));
}

void SctpTask::receiveClientReceive(int clientId, uint16_t stream, UniqueBuffer &&buffer)
{
    ClientEntry *entry = m_clients[clientId];
    if (entry == nullptr)
    {
        m_logger->warn("Client entry not found for id: %d", clientId);
        return;
    }

    // Notify the relevant task
    auto msg = std::make_unique<NmGnbSctp>(NmGnbSctp::RECEIVE_MESSAGE);
    msg->clientId = clientId;
    msg->stream = stream;
    msg->buffer = std::move(buffer);
    entry->associatedTask->push(std::move(msg));
}

void SctpTask::receiveUnhandledNotification(int clientId)
{
    // NOTE: For unhandled notifications, "clientId" may be invalid for some notifications.
    // Because some notification may be received after shutdown.

    // Print warning
    m_logger->warn("Unhandled SCTP notification received");
}

void SctpTask::receiveConnectionClose(int clientId)
{
    auto it = m_clients.find(clientId);
    if (it == m_clients.end() || it->second == nullptr)
    {
        m_logger->warn("Client entry not found for id: %d", clientId);
        return;
    }

    ClientEntry *entry = it->second;
    m_clients.erase(it);
    DeleteClientEntry(entry);
}

void SctpTask::receiveSendMessage(int clientId, uint16_t stream, UniqueBuffer &&buffer)
{
    ClientEntry *entry = m_clients[clientId];
    if (entry == nullptr)
    {
        m_logger->warn("Client entry not found for id: %d", clientId);
        return;
    }

#ifdef MOCKED_PACKETS
    {
        std::string ss = MOCK_LIST[++MOCK_INDEX];
        OctetString data = OctetString::FromHex(ss);
        auto *copy = new uint8_t[data.length()];
        std::memcpy(copy, data.data(), data.length());
        receiveClientReceive(clientId, 0, copy, data.length());
    }
#else
    entry->client->send(stream, buffer.data(), 0, buffer.size());
#endif
}

} // namespace nr::gnb
