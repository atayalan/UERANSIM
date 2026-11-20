//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "udp_task.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>

#include <ue/nts.hpp>
#include <utils/common.hpp>
#include <utils/constants.hpp>

static constexpr const int BUFFER_SIZE = 16384;
static constexpr const int LOOP_PERIOD = 1000;
static constexpr const int RECEIVE_TIMEOUT = 200;
static constexpr const int HEARTBEAT_THRESHOLD = 10000; // 容許忙碌時的 heartbeat 延遲，單位為毫秒。

static constexpr const size_t MAX_CELL_COUNT = 256;

namespace nr::ue
{

RlsUdpTask::RlsUdpTask(TaskBase *base, RlsSharedContext *shCtx, const std::vector<std::string> &searchSpace)
    : m_server{}, m_ctlTask{}, m_shCtx{shCtx}, m_searchSpace{}, m_cells{}, m_cellIdToSti{}, m_lastLoop{},
      m_cellIdCounter{}
{
    m_logger = base->logBase->makeUniqueLogger(base->config->getLoggerPrefix() + "rls-udp");

    m_server = std::make_unique<udp::UdpServer>();
    if (!m_server->HasIpv6())
        m_logger->warn("IPv6 is not available on this host, RLS will reach IPv4 gNBs only");

    for (auto &ip : searchSpace)
        m_searchSpace.emplace_back(ip, cons::RadioLinkPort);

    m_simPos = Vector3{};
}

void RlsUdpTask::onStart()
{
}

void RlsUdpTask::onLoop()
{
    auto current = utils::CurrentTimeMillis();
    if (current - m_lastLoop > LOOP_PERIOD)
    {
        m_lastLoop = current;
        heartbeatCycle(current, m_simPos);
    }

    uint8_t buffer[BUFFER_SIZE];
    InetAddress peerAddress;

    int size = m_server->Receive(buffer, BUFFER_SIZE, RECEIVE_TIMEOUT, peerAddress);
    if (size > 0)
    {
        auto rlsMsg = rls::DecodeRlsMessage(OctetView{buffer, static_cast<size_t>(size)});
        if (rlsMsg == nullptr)
            m_logger->err("Unable to decode RLS message");
        else
            receiveRlsPdu(peerAddress, std::move(rlsMsg));
    }
}

void RlsUdpTask::onQuit()
{
    m_server.reset();
}

void RlsUdpTask::sendRlsPdu(const InetAddress &addr, const rls::RlsMessage &msg)
{
    try
    {
        OctetString stream;
        rls::EncodeRlsMessage(msg, stream);
        m_server->Send(addr, stream.data(), static_cast<size_t>(stream.length()));
    }
    catch (const std::exception &e)
    {
        m_logger->err("RLS PDU send failed: %s", e.what());
    }
}

void RlsUdpTask::send(int cellId, const rls::RlsMessage &msg)
{
    InetAddress address;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto itSti = m_cellIdToSti.find(cellId);
        if (itSti == m_cellIdToSti.end())
            return;
        auto itCell = m_cells.find(itSti->second);
        if (itCell == m_cells.end())
            return;
        address = itCell->second.address;
    }
    sendRlsPdu(address, msg);
}

void RlsUdpTask::receiveRlsPdu(const InetAddress &addr, std::unique_ptr<rls::RlsMessage> &&msg)
{
    if (msg->msgType == rls::EMessageType::HEARTBEAT_ACK)
    {
        // Only a gNB in gnbSearchList can become a cell. Anyone else who can reach this socket could otherwise pose as
        // the strongest cell, and talk RRC and NAS to the UE.
        if (std::find(m_searchSpace.begin(), m_searchSpace.end(), addr) == m_searchSpace.end())
        {
            m_logger->warn("Ignoring a heartbeat ack from %s, which is not in gnbSearchList", addr.toString().c_str());
            return;
        }

        int cellId = 0;
        int oldDbm = INT32_MIN;
        int newDbm = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_cells.count(msg->sti))
            {
                if (m_cells.size() >= MAX_CELL_COUNT)
                {
                    m_logger->warn("Max cell count reached, ignoring new cell");
                    return;
                }

                // 保留 N2／Xn 所需的穩定 gNB ID；舊版 peer 仍依發現順序編號。
                const auto &heartbeat = static_cast<const rls::RlsHeartBeatAck &>(*msg);
                m_cells[msg->sti].cellId =
                    heartbeat.gnbId == 0 ? ++m_cellIdCounter : static_cast<int>(heartbeat.gnbId);
                m_cellIdToSti[m_cells[msg->sti].cellId] = msg->sti;
            }

            if (m_cells.count(msg->sti))
                oldDbm = m_cells[msg->sti].dbm;

            m_cells[msg->sti].address = addr;
            m_cells[msg->sti].lastSeen = utils::CurrentTimeMillis();

            newDbm = ((const rls::RlsHeartBeatAck &)*msg).dbm;
            m_cells[msg->sti].dbm = newDbm;
            cellId = m_cells[msg->sti].cellId;
        }

        if (oldDbm != newDbm)
            onSignalChangeOrLost(cellId);
        return;
    }

    // Anything else is taken only from a cell, and only from the address the cell answers heartbeats from.
    int cellId = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cells.find(msg->sti);
        if (it == m_cells.end() || it->second.address != addr)
            return;
        cellId = it->second.cellId;
    }

    auto w = std::make_unique<NmUeRlsToRls>(NmUeRlsToRls::RECEIVE_RLS_MESSAGE);
    w->cellId = cellId;
    w->msg = std::move(msg);
    m_ctlTask->push(std::move(w));
}

void RlsUdpTask::onSignalChangeOrLost(int cellId)
{
    int dbm = INT32_MIN;
    m_logger->debug("[RLS]Signal changed for cell[%d]", cellId);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto itSti = m_cellIdToSti.find(cellId);
        if (itSti != m_cellIdToSti.end())
        {
            auto itCell = m_cells.find(itSti->second);
            if (itCell != m_cells.end())
                dbm = itCell->second.dbm;
        }
    }

    auto w = std::make_unique<NmUeRlsToRls>(NmUeRlsToRls::SIGNAL_CHANGED);
    w->cellId = cellId;
    w->dbm = dbm;
    m_ctlTask->push(std::move(w));
}

void RlsUdpTask::heartbeatCycle(uint64_t time, const Vector3 &simPos)
{
    std::set<std::pair<uint64_t, int>> toRemove;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto &cell : m_cells)
        {
            auto delta = time - cell.second.lastSeen;
            if (delta > HEARTBEAT_THRESHOLD)
                toRemove.insert({cell.first, cell.second.cellId});
        }

        for (auto cell : toRemove)
        {
            m_cells.erase(cell.first);
            m_cellIdToSti.erase(cell.second);
        }
    }

    for (auto cell : toRemove)
        onSignalChangeOrLost(cell.second);

    if (m_firstWindow && !m_searchSpace.empty())
    {
        if (m_firstWindowStart == 0) // first time sending heartbeat
            m_firstWindowStart = time;

        // HB only to the first gNB
        rls::RlsHeartBeat hb{m_shCtx->sti};
        hb.simPos = simPos;
        sendRlsPdu(m_searchSpace.front(), hb);

        if (time - m_firstWindowStart >= 500)
            m_firstWindow = false; // window is closed
    }
    else
    {
        // normal diffusion
        for (auto &addr : m_searchSpace)
        {
            rls::RlsHeartBeat msg{m_shCtx->sti};
            msg.simPos = simPos;
            sendRlsPdu(addr, msg);
        }
    }
}

void RlsUdpTask::initialize(NtsTask *ctlTask)
{
    m_ctlTask = ctlTask;
}

} // namespace nr::ue
