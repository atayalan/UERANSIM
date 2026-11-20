//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#pragma once

#include "utils.hpp"

#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <gnb/nts.hpp>
#include <lib/udp/server_task.hpp>
#include <utils/logger.hpp>
#include <utils/nts.hpp>

namespace nr::gnb
{

class GtpTask : public NtsTask
{
  public:
    struct SessionSnapshot
    {
        int psi{};
        GtpTunnel downTunnel{};
        std::vector<int> qosFlowIds{};
    };

  private:
    TaskBase *m_base;
    std::unique_ptr<Logger> m_logger;

    udp::UdpServerTask *m_udpServer;
    std::unordered_map<int, std::unique_ptr<GtpUeContext>> m_ueContexts;
    std::unique_ptr<IRateLimiter> m_rateLimiter;
    std::unordered_map<uint64_t, std::unique_ptr<PduSessionResource>> m_pduSessions;
    mutable std::mutex m_pduSessionMutex;
    PduSessionTree m_sessionTree;
    std::unordered_map<uint64_t, XnForwardingEndpoint> m_sourceForwarding;
    std::unordered_map<uint32_t, uint64_t> m_targetForwarding;
    uint32_t m_nextForwardingTeid{0xE0000000u};

    friend class GnbCmdHandler;

  public:
    explicit GtpTask(TaskBase *base);
    ~GtpTask() override = default;
    [[nodiscard]] std::vector<SessionSnapshot> sessionSnapshot(int ueId) const;
    [[nodiscard]] std::vector<XnHandoverPduSessionSnapshot> handoverSessionSnapshot(int ueId) const;
    bool updateSessionUplinkTunnel(int ueId, int psi, uint32_t teid, const OctetString &address);

  protected:
    void onStart() override;
    void onLoop() override;
    void onQuit() override;

  private:
    void handleUdpReceive(const udp::NwUdpServerReceive &msg);
    void handleUeContextUpdate(const GtpUeContextUpdate &msg);
    void handleSessionCreate(PduSessionResource *session);
    void handleSessionRelease(int ueId, int psi);
    void handleUeContextDelete(int ueId);
    void handleUplinkData(int ueId, int psi, OctetString &&data);
    void handleAllocateTargetForwarding(int ueId);
    void handleActivateSourceForwarding(int ueId, const std::vector<XnForwardingEndpoint> &endpoints);
    void handleCancelForwarding(int ueId);
    void forwardDownlink(uint64_t sessionInd, const OctetString &pdu, int qfi);
    void sendEndMarker(const XnForwardingEndpoint &endpoint);
    void clearSourceForwardingForUe(int ueId, bool sendEndMarkers);
    void clearTargetForwardingForUe(int ueId);
    void clearForwardingForSession(uint64_t sessionInd);
    [[nodiscard]] uint32_t allocateForwardingTeid();
    void reportForwardingFailure(int ueId, const std::string &cause);

    void updateAmbrForUe(int ueId);
    void updateAmbrForSession(uint64_t pduSession);
};

} // namespace nr::gnb
