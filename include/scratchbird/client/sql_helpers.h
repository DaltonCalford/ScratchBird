#pragma once

#include <string>
#include <vector>

#include "scratchbird/protocol/wire_protocol.h"

namespace scratchbird {
namespace client {

std::string substituteParameters(
    const std::string& sql,
    const std::vector<protocol::ProtocolCodec::ColumnValue>& params);

size_t countParameters(const std::string& sql);

} // namespace client
} // namespace scratchbird
