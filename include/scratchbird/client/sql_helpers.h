#pragma once

#include <string>
#include <vector>

#include "scratchbird/protocol/wire_protocol.h"

namespace scratchbird {
namespace client {

std::string substituteParameters(
    const std::string& sql,
    const std::vector<protocol::ProtocolCodec::ColumnValue>& params);

std::string substituteParameters(
    const std::string& sql,
    const std::vector<protocol::ProtocolCodec::ColumnValue>& params,
    const std::vector<protocol::WireType>& param_types);

size_t countParameters(const std::string& sql);

} // namespace client
} // namespace scratchbird
