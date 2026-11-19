#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <gnb/types.hpp>
#include <utils/unique_buffer.hpp>

namespace nr::gnb::xnap
{

enum class MessageType
{
    UNKNOWN,
    XN_SETUP_REQUEST,
    XN_SETUP_RESPONSE,
    XN_SETUP_FAILURE,
    HANDOVER_REQUEST,
    HANDOVER_REQUEST_ACK,
    HANDOVER_PREPARATION_FAILURE,
    HANDOVER_CANCEL,
    UE_CONTEXT_RELEASE,
};

struct HandoverData
{
    uint32_t sourceUeXnapId{};
    uint32_t targetUeXnapId{};
    std::optional<uint64_t> amfUeNgapId{};
    std::vector<uint8_t> transparentContainer{};
    std::vector<XnForwardingEndpoint> forwardingEndpoints{};
};

struct DecodedMessage
{
    MessageType type{MessageType::UNKNOWN};
    uint32_t peerGnbId{};
    std::optional<HandoverData> handover{};
};

UniqueBuffer EncodeSetupRequest(const GnbConfig &config);
UniqueBuffer EncodeSetupResponse(const GnbConfig &config);
std::optional<DecodedMessage> Decode(const uint8_t *data, size_t size);
UniqueBuffer EncodeHandoverRequest(const HandoverData &data, const GnbConfig &targetConfig);
UniqueBuffer EncodeHandoverRequestAck(const HandoverData &data);
UniqueBuffer EncodeHandoverPreparationFailure(const HandoverData &data);
UniqueBuffer EncodeHandoverCancel(const HandoverData &data);
UniqueBuffer EncodeUeContextRelease(const HandoverData &data);

} // namespace nr::gnb::xnap
