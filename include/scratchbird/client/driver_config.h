#pragma once

#include <map>
#include <string>

#include "scratchbird/client/network_client.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird {
namespace client {

network::SSLMode parseSslMode(const std::string& value);
protocol::AuthMethod parseAuthMethod(const std::string& value, bool* ok = nullptr);

core::Status parseKeyValueConnectionString(const std::string& conn_str,
                                           std::map<std::string, std::string>& params,
                                           core::ErrorContext* ctx = nullptr);
core::Status parseScratchbirdUrl(const std::string& url,
                                 NetworkClientConfig& config,
                                 core::ErrorContext* ctx = nullptr);
core::Status applyConnectionParams(const std::map<std::string, std::string>& params,
                                   NetworkClientConfig& config,
                                   core::ErrorContext* ctx = nullptr);
core::Status parseDriverConnectionString(const std::string& conn_str,
                                         NetworkClientConfig& config,
                                         core::ErrorContext* ctx = nullptr);

void applyDriverDefaults(NetworkClientConfig& config);

} // namespace client
} // namespace scratchbird
