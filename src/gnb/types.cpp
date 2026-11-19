//
// This file is a part of UERANSIM project.
// Copyright (c) 2027 ALİ GÜNGÖR.
//
// https://github.com/aligungr/UERANSIM/
// See README, LICENSE, and CONTRIBUTING files for licensing details.
//

#include <sstream>

#include <gnb/types.hpp>
#include <utils/common.hpp>

namespace nr::gnb
{

Json ToJson(const GnbStatusInfo &v)
{
    return Json::Obj({{"is-ngap-up", v.isNgapUp}});
}

Json ToJson(const GnbConfig &v)
{
    return Json::Obj({
        {"name", v.name},
        {"nci", v.nci},
        {"plmn", ToJson(v.plmn)},
        {"tac", v.tac},
        {"nssai", ToJson(v.nssai)},
        {"ngap-ip", v.ngapIp},
        {"gtp-ip", v.gtpIp},
        {"xn", ToJson(v.xn)},
        {"paging-drx", ToJson(v.pagingDrx)},
        {"ignore-sctp-id", v.ignoreStreamIds},
    });
}

Json ToJson(const NgapAmfContext &v)
{
    auto isIp6 = utils::GetIpVersion(v.address) == 6;
    auto address = isIp6 ? "[" + v.address + "]" : v.address;

    return Json::Obj({
        {"id", v.ctxId},
        {"name", v.amfName},
        {"address", address + ":" + std::to_string(v.port)},
        {"state", ToJson(v.state).str()},
        {"capacity", v.relativeCapacity},
        {"association", ToJson(v.association)},
        {"served-guami", ::ToJson(v.servedGuamiList)},
        {"served-plmn", ::ToJson(v.plmnSupportList)},
    });
}

Json ToJson(const EAmfState &v)
{
    switch (v)
    {
    case EAmfState::NOT_CONNECTED:
        return "NOT_CONNECTED";
    case EAmfState::WAITING_NG_SETUP:
        return "WAITING_NG_SETUP";
    case EAmfState::CONNECTED:
        return "CONNECTED";
    default:
        return "?";
    }
}

Json ToJson(const EPagingDrx &v)
{
    switch (v)
    {
    case EPagingDrx::V32:
        return "v32";
    case EPagingDrx::V64:
        return "v64";
    case EPagingDrx::V128:
        return "v128";
    case EPagingDrx::V256:
        return "v256";
    default:
        return "?";
    }
}

Json ToJson(const SctpAssociation &v)
{
    return Json::Obj({{"id", v.associationId}, {"rx-num", v.inStreams}, {"tx-num", v.outStreams}});
}

Json ToJson(const XnNeighborConfig &v)
{
    return Json::Obj({
        {"gnb-id", static_cast<int64_t>(v.gnbId)},
        {"address", v.address},
        {"port", static_cast<int>(v.port)},
    });
}

Json ToJson(const XnConfig &v)
{
    Json neighbors = Json::Arr({});
    for (const auto &neighbor : v.neighbors)
        neighbors.push(ToJson(neighbor));

    return Json::Obj({
        {"enabled", v.enabled},
        {"bind-ip", v.bindIp},
        {"port", static_cast<int>(v.port)},
        {"neighbors", neighbors},
    });
}

Json ToJson(const EXnNeighborState &v)
{
    switch (v)
    {
    case EXnNeighborState::DISCONNECTED:
        return "DISCONNECTED";
    case EXnNeighborState::SCTP_CONNECTING:
        return "SCTP_CONNECTING";
    case EXnNeighborState::XN_SETUP_PENDING:
        return "XN_SETUP_PENDING";
    case EXnNeighborState::CONNECTED:
        return "CONNECTED";
    case EXnNeighborState::FAILED:
        return "FAILED";
    default:
        return "?";
    }
}

Json ToJson(const XnNeighborContext &v)
{
    return Json::Obj({
        {"gnb-id", static_cast<int64_t>(v.gnbId)},
        {"ctx-id", v.ctxId},
        {"sctp-client-id", v.sctpClientId},
        {"address", v.address},
        {"port", static_cast<int>(v.port)},
        {"state", ToJson(v.state).str()},
        {"association", ToJson(v.association)},
    });
}

Json ToJson(const ServedGuami &v)
{
    return Json::Obj({{"guami", ToJson(v.guami)}, {"backup-amf", v.backupAmfName}});
}

Json ToJson(const Guami &v)
{
    return Json::Obj({
        {"plmn", ToJson(v.plmn)},
        {"region-id", ::ToJson(v.amfRegionId)},
        {"set-id", ::ToJson(v.amfSetId)},
        {"pointer", ::ToJson(v.amfPointer)},
    });
}

} // namespace nr::gnb
