//
// This file is a part of UERANSIM project.
// Copyright (c) 2023 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "snapshot.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace nr::gnb
{

namespace
{

constexpr std::array<uint8_t, 4> HANDOVER_SNAPSHOT_MAGIC = {'X', 'N', 'H', 'O'};
constexpr uint8_t HANDOVER_SNAPSHOT_VERSION = 2;
constexpr size_t MAX_HANDOVER_SNAPSHOT_SIZE = 64 * 1024;
constexpr uint16_t MAX_HANDOVER_SESSIONS = 256;
constexpr uint16_t MAX_HANDOVER_QOS_FLOWS = 64;
constexpr uint16_t MAX_HANDOVER_TUNNEL_ADDRESS = 256;

void PutU8(std::vector<uint8_t> &out, uint8_t value)
{
    out.push_back(value);
}

void PutU16(std::vector<uint8_t> &out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<uint8_t>(value & 0xffU));
}

void PutU32(std::vector<uint8_t> &out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>((value >> 24U) & 0xffU));
    out.push_back(static_cast<uint8_t>((value >> 16U) & 0xffU));
    out.push_back(static_cast<uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<uint8_t>(value & 0xffU));
}

void PutU64(std::vector<uint8_t> &out, uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<uint8_t>((value >> static_cast<unsigned>(shift)) & 0xffU));
}

class SnapshotReader
{
  private:
    const std::vector<uint8_t> &m_data;
    size_t m_offset{};

  public:
    explicit SnapshotReader(const std::vector<uint8_t> &data) : m_data(data)
    {
    }

    bool getU8(uint8_t &value)
    {
        if (m_offset + 1 > m_data.size())
            return false;
        value = m_data[m_offset++];
        return true;
    }

    bool getU16(uint16_t &value)
    {
        if (m_offset + 2 > m_data.size())
            return false;
        value = static_cast<uint16_t>((static_cast<uint16_t>(m_data[m_offset]) << 8U) |
                                      static_cast<uint16_t>(m_data[m_offset + 1]));
        m_offset += 2;
        return true;
    }

    bool getU32(uint32_t &value)
    {
        if (m_offset + 4 > m_data.size())
            return false;
        value = (static_cast<uint32_t>(m_data[m_offset]) << 24U) |
                (static_cast<uint32_t>(m_data[m_offset + 1]) << 16U) |
                (static_cast<uint32_t>(m_data[m_offset + 2]) << 8U) |
                static_cast<uint32_t>(m_data[m_offset + 3]);
        m_offset += 4;
        return true;
    }

    bool getU64(uint64_t &value)
    {
        if (m_offset + 8 > m_data.size())
            return false;
        value = 0;
        for (int i = 0; i < 8; ++i)
            value = (value << 8U) | static_cast<uint64_t>(m_data[m_offset + static_cast<size_t>(i)]);
        m_offset += 8;
        return true;
    }

    bool getBytes(size_t length, const uint8_t *&data)
    {
        if (length > m_data.size() - m_offset)
            return false;
        data = m_data.data() + m_offset;
        m_offset += length;
        return true;
    }

    [[nodiscard]] bool atEnd() const
    {
        return m_offset == m_data.size();
    }
};

} // namespace

std::optional<std::vector<uint8_t>> SerializeHandoverSnapshot(const XnHandoverNgapSnapshot &snapshot)
{
    if (snapshot.pduSessions.size() > MAX_HANDOVER_SESSIONS)
        return std::nullopt;

    std::vector<const XnHandoverPduSessionSnapshot *> sessions;
    sessions.reserve(snapshot.pduSessions.size());
    for (const auto &session : snapshot.pduSessions)
        sessions.push_back(&session);
    std::sort(sessions.begin(), sessions.end(), [](const auto *left, const auto *right) { return left->psi < right->psi; });

    std::vector<uint8_t> out;
    out.reserve(128);
    out.insert(out.end(), HANDOVER_SNAPSHOT_MAGIC.begin(), HANDOVER_SNAPSHOT_MAGIC.end());
    PutU8(out, HANDOVER_SNAPSHOT_VERSION);
    PutU64(out, snapshot.sti);
    PutU64(out, snapshot.ueAmbr.dlAmbr);
    PutU64(out, snapshot.ueAmbr.ulAmbr);
    PutU8(out, snapshot.securityCapabilities.valid ? 1U : 0U);
    PutU16(out, snapshot.securityCapabilities.nrEncryptionAlgorithms);
    PutU16(out, snapshot.securityCapabilities.nrIntegrityProtectionAlgorithms);
    PutU16(out, snapshot.securityCapabilities.eutraEncryptionAlgorithms);
    PutU16(out, snapshot.securityCapabilities.eutraIntegrityProtectionAlgorithms);
    PutU16(out, static_cast<uint16_t>(sessions.size()));

    for (const auto *session : sessions)
    {
        if (session->psi < 0 || session->psi > std::numeric_limits<uint16_t>::max())
            return std::nullopt;
        if (session->upTunnel.address.length() < 0 ||
            session->upTunnel.address.length() > static_cast<int>(MAX_HANDOVER_TUNNEL_ADDRESS))
            return std::nullopt;
        if (session->qosFlowIds.size() > MAX_HANDOVER_QOS_FLOWS)
            return std::nullopt;

        PutU16(out, static_cast<uint16_t>(session->psi));
        PutU8(out, static_cast<uint8_t>(session->sessionType));
        PutU64(out, session->sessionAmbr.dlAmbr);
        PutU64(out, session->sessionAmbr.ulAmbr);
        PutU32(out, session->upTunnel.teid);
        PutU16(out, static_cast<uint16_t>(session->upTunnel.address.length()));
        if (session->upTunnel.address.length() > 0)
        {
            const auto *address = session->upTunnel.address.data();
            out.insert(out.end(), address, address + session->upTunnel.address.length());
        }

        auto qfis = session->qosFlowIds;
        std::sort(qfis.begin(), qfis.end());
        PutU16(out, static_cast<uint16_t>(qfis.size()));
        for (int qfi : qfis)
        {
            if (qfi < 0 || qfi > 255)
                return std::nullopt;
            PutU8(out, static_cast<uint8_t>(qfi));
        }

        if (out.size() > MAX_HANDOVER_SNAPSHOT_SIZE)
            return std::nullopt;
    }

    return out;
}

std::unique_ptr<XnHandoverNgapSnapshot> DeserializeHandoverSnapshot(const std::vector<uint8_t> &data)
{
    if (data.size() < HANDOVER_SNAPSHOT_MAGIC.size() + 1 || data.size() > MAX_HANDOVER_SNAPSHOT_SIZE)
        return nullptr;

    SnapshotReader reader{data};
    const uint8_t *magic = nullptr;
    if (!reader.getBytes(HANDOVER_SNAPSHOT_MAGIC.size(), magic) ||
        !std::equal(HANDOVER_SNAPSHOT_MAGIC.begin(), HANDOVER_SNAPSHOT_MAGIC.end(), magic))
        return nullptr;

    uint8_t version = 0;
    if (!reader.getU8(version) || version != HANDOVER_SNAPSHOT_VERSION)
        return nullptr;

    auto snapshot = std::make_unique<XnHandoverNgapSnapshot>();
    uint8_t securityValid = 0;
    uint16_t sessionCount = 0;
    if (!reader.getU64(snapshot->sti) || !reader.getU64(snapshot->ueAmbr.dlAmbr) || !reader.getU64(snapshot->ueAmbr.ulAmbr) ||
        !reader.getU8(securityValid) || securityValid > 1 ||
        !reader.getU16(snapshot->securityCapabilities.nrEncryptionAlgorithms) ||
        !reader.getU16(snapshot->securityCapabilities.nrIntegrityProtectionAlgorithms) ||
        !reader.getU16(snapshot->securityCapabilities.eutraEncryptionAlgorithms) ||
        !reader.getU16(snapshot->securityCapabilities.eutraIntegrityProtectionAlgorithms) ||
        !reader.getU16(sessionCount) || sessionCount > MAX_HANDOVER_SESSIONS)
        return nullptr;
    snapshot->securityCapabilities.valid = securityValid != 0;

    snapshot->pduSessions.reserve(sessionCount);
    for (uint16_t i = 0; i < sessionCount; ++i)
    {
        XnHandoverPduSessionSnapshot session{};
        uint16_t psi = 0;
        uint8_t sessionType = 0;
        uint16_t addressLength = 0;
        uint16_t qfiCount = 0;
        if (!reader.getU16(psi) || !reader.getU8(sessionType) ||
            sessionType > static_cast<uint8_t>(PduSessionType::UNSTRUCTURED) ||
            !reader.getU64(session.sessionAmbr.dlAmbr) || !reader.getU64(session.sessionAmbr.ulAmbr) ||
            !reader.getU32(session.upTunnel.teid) || !reader.getU16(addressLength) ||
            addressLength > MAX_HANDOVER_TUNNEL_ADDRESS)
            return nullptr;

        const uint8_t *address = nullptr;
        if (!reader.getBytes(addressLength, address) || !reader.getU16(qfiCount) || qfiCount > MAX_HANDOVER_QOS_FLOWS)
            return nullptr;

        session.psi = psi;
        session.sessionType = static_cast<PduSessionType>(sessionType);
        session.upTunnel.address = OctetString::FromArray(address, addressLength);
        session.qosFlowIds.reserve(qfiCount);
        for (uint16_t qfiIndex = 0; qfiIndex < qfiCount; ++qfiIndex)
        {
            uint8_t qfi = 0;
            if (!reader.getU8(qfi))
                return nullptr;
            session.qosFlowIds.push_back(static_cast<int>(qfi));
        }
        snapshot->pduSessions.push_back(std::move(session));
    }

    if (!reader.atEnd())
        return nullptr;
    return snapshot;
}

} // namespace nr::gnb
