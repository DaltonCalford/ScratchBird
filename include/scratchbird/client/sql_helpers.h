/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
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
