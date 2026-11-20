//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include "task.hpp"

#include <gnb/ngap/task.hpp>
#include <lib/rrc/encode.hpp>

namespace nr::gnb
{

RrcUeContext *GnbRrcTask::createUe(int id)
{
    auto *ctx = new RrcUeContext(id);
    m_ueCtx[id] = ctx;
    return ctx;
}

RrcUeContext *GnbRrcTask::tryFindUe(int id)
{
    if (m_ueCtx.count(id))
        return m_ueCtx[id];
    return nullptr;
}

RrcUeContext *GnbRrcTask::findUe(int id)
{
    auto *ue = tryFindUe(id);
    if (ue == nullptr)
    {
        m_logger->err("UE context with ID[%d] not found", id);
        return ue;
    }
    return ue;
}

void GnbRrcTask::releaseHandoverSource(int ueId)
{
    auto it = m_ueCtx.find(ueId);
    if (it == m_ueCtx.end())
    {
        m_logger->debug("Source RRC UE context already absent for UE[%d]", ueId);
        return;
    }

    // The UE has already moved to the target cell. Remove only local state: do not
    // transmit RRC Release and do not route this cleanup through the RLF path.
    delete it->second;
    m_ueCtx.erase(it);
    m_logger->debug("Silently released source RRC context after Xn handover for UE[%d]", ueId);
}

} // namespace nr::gnb
