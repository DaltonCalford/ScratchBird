#include "scratchbird/client/driver_config.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace scratchbird {
namespace client {

namespace {
std::string toLower(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return out;
}

std::string trim(std::string value) {
    auto first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    auto last = value.find_last_not_of(" \t\n\r");
    value.erase(last + 1);
    value.erase(0, first);
    return value;
}

bool parseUint32(const std::string& value, uint32_t& out) {
    try {
        size_t idx = 0;
        unsigned long parsed = std::stoul(value, &idx);
        if (idx != value.size()) {
            return false;
        }
        out = static_cast<uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseBool(const std::string& value, bool& out) {
    std::string lower = toLower(trim(value));
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        out = true;
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        out = false;
        return true;
    }
    return false;
}

uint32_t normalizeTimeoutMs(const std::string& key, const std::string& value) {
    uint32_t parsed = 0;
    if (!parseUint32(value, parsed)) {
        return 0;
    }
    std::string lower = toLower(key);
    if (lower.find("_ms") != std::string::npos || lower.find("ms") != std::string::npos) {
        return parsed;
    }
    return parsed * 1000;
}
}

network::SSLMode parseSslMode(const std::string& value) {
    auto mode = toLower(value);
    if (mode == "disable" || mode == "disabled") {
        return network::SSLMode::DISABLED;
    }
    if (mode == "allow") {
        return network::SSLMode::ALLOW;
    }
    if (mode == "prefer") {
        return network::SSLMode::PREFER;
    }
    if (mode == "require") {
        return network::SSLMode::REQUIRE;
    }
    if (mode == "verify-ca" || mode == "verify_ca") {
        return network::SSLMode::VERIFY_CA;
    }
    if (mode == "verify-full" || mode == "verify_full") {
        return network::SSLMode::VERIFY_FULL;
    }
    return network::SSLMode::REQUIRE;
}

protocol::AuthMethod parseAuthMethod(const std::string& value, bool* ok) {
    std::string lower = toLower(value);
    if (lower == "scram-sha-256" || lower == "scram_sha_256" || lower == "scram256") {
        if (ok) {
            *ok = true;
        }
        return protocol::AuthMethod::SCRAM_SHA_256;
    }
    if (lower == "scram-sha-512" || lower == "scram_sha_512" || lower == "scram512") {
        if (ok) {
            *ok = true;
        }
        return protocol::AuthMethod::SCRAM_SHA_512;
    }
    if (lower == "password" || lower == "plain") {
        if (ok) {
            *ok = true;
        }
        return protocol::AuthMethod::PASSWORD;
    }
    if (lower == "md5") {
        if (ok) {
            *ok = true;
        }
        return protocol::AuthMethod::MD5;
    }
    if (ok) {
        *ok = false;
    }
    return protocol::AuthMethod::SCRAM_SHA_256;
}

core::Status parseKeyValueConnectionString(const std::string& conn_str,
                                           std::map<std::string, std::string>& params,
                                           core::ErrorContext* ctx) {
    params.clear();

    size_t start = 0;
    while (start < conn_str.size()) {
        size_t end = conn_str.find(';', start);
        if (end == std::string::npos) {
            end = conn_str.size();
        }
        std::string token = conn_str.substr(start, end - start);
        start = end + 1;
        token = trim(token);
        if (token.empty()) {
            continue;
        }
        size_t eq = token.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = trim(token.substr(0, eq));
        std::string value = trim(token.substr(eq + 1));
        if (value.size() >= 2 && value.front() == '{' && value.back() == '}') {
            value = value.substr(1, value.size() - 2);
        }
        if (!key.empty()) {
            params[toLower(key)] = value;
        }
    }

    if (params.empty() && ctx) {
        ctx->message = "Empty connection string";
    }
    return core::Status::OK;
}

core::Status parseScratchbirdUrl(const std::string& url,
                                 NetworkClientConfig& config,
                                 core::ErrorContext* ctx) {
    const std::string kPrefix = "scratchbird://";
    if (url.rfind(kPrefix, 0) != 0) {
        if (ctx) {
            ctx->message = "Unsupported URL scheme";
        }
        return core::Status::INVALID_ARGUMENT;
    }

    std::string remainder = url.substr(kPrefix.size());
    std::string query;
    size_t query_pos = remainder.find('?');
    if (query_pos != std::string::npos) {
        query = remainder.substr(query_pos + 1);
        remainder = remainder.substr(0, query_pos);
    }

    std::string userinfo;
    std::string hostport;
    std::string database;

    size_t at_pos = remainder.find('@');
    if (at_pos != std::string::npos) {
        userinfo = remainder.substr(0, at_pos);
        hostport = remainder.substr(at_pos + 1);
    } else {
        hostport = remainder;
    }

    size_t slash_pos = hostport.find('/');
    if (slash_pos != std::string::npos) {
        database = hostport.substr(slash_pos + 1);
        hostport = hostport.substr(0, slash_pos);
    }

    std::string host = hostport;
    std::string port_str;
    size_t colon_pos = hostport.rfind(':');
    if (colon_pos != std::string::npos && colon_pos + 1 < hostport.size()) {
        host = hostport.substr(0, colon_pos);
        port_str = hostport.substr(colon_pos + 1);
    }

    if (!userinfo.empty()) {
        size_t colon = userinfo.find(':');
        if (colon == std::string::npos) {
            config.username = userinfo;
        } else {
            config.username = userinfo.substr(0, colon);
            config.password = userinfo.substr(colon + 1);
        }
    }

    if (!host.empty()) {
        config.host = host;
    }
    if (!port_str.empty()) {
        uint32_t port_val = 0;
        if (!parseUint32(port_str, port_val) || port_val > 65535) {
            if (ctx) {
                ctx->message = "Invalid port in URL";
            }
            return core::Status::INVALID_ARGUMENT;
        }
        config.port = static_cast<uint16_t>(port_val);
    }
    if (!database.empty()) {
        config.database = database;
    }

    if (!query.empty()) {
        std::map<std::string, std::string> params;
        std::stringstream ss(query);
        std::string pair;
        while (std::getline(ss, pair, '&')) {
            size_t eq = pair.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            std::string key = toLower(trim(pair.substr(0, eq)));
            std::string value = trim(pair.substr(eq + 1));
            if (!key.empty()) {
                params[key] = value;
            }
        }
        auto status = applyConnectionParams(params, config, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    return core::Status::OK;
}

core::Status applyConnectionParams(const std::map<std::string, std::string>& params,
                                   NetworkClientConfig& config,
                                   core::ErrorContext* ctx) {
    for (const auto& entry : params) {
        const std::string& key = entry.first;
        const std::string& value = entry.second;
        if (key == "server" || key == "host") {
            config.host = value;
        } else if (key == "port") {
            uint32_t parsed = 0;
            if (!parseUint32(value, parsed) || parsed > 65535) {
                if (ctx) {
                    ctx->message = "Invalid port";
                }
                return core::Status::INVALID_ARGUMENT;
            }
            config.port = static_cast<uint16_t>(parsed);
        } else if (key == "database" || key == "db") {
            config.database = value;
        } else if (key == "uid" || key == "user" || key == "username") {
            config.username = value;
        } else if (key == "pwd" || key == "password") {
            config.password = value;
        } else if (key == "applicationname" || key == "application_name" || key == "app") {
            config.application_name = value;
        } else if (key == "ssl" || key == "sslmode") {
            config.ssl_mode = parseSslMode(value);
        } else if (key == "sslcert") {
            config.ssl_cert = value;
        } else if (key == "sslkey") {
            config.ssl_key = value;
        } else if (key == "sslrootcert") {
            config.ssl_root_cert = value;
        } else if (key == "connecttimeout" || key == "connect_timeout" || key == "timeout") {
            uint32_t ms = normalizeTimeoutMs(key, value);
            if (ms > 0) {
                config.connect_timeout_ms = ms;
            }
        } else if (key == "readtimeout" || key == "read_timeout") {
            uint32_t ms = normalizeTimeoutMs(key, value);
            if (ms > 0) {
                config.read_timeout_ms = ms;
            }
        } else if (key == "writetimeout" || key == "write_timeout") {
            uint32_t ms = normalizeTimeoutMs(key, value);
            if (ms > 0) {
                config.write_timeout_ms = ms;
            }
        } else if (key == "querytimeout" || key == "query_timeout") {
            uint32_t ms = normalizeTimeoutMs(key, value);
            if (ms > 0) {
                config.read_timeout_ms = ms;
                config.write_timeout_ms = ms;
            }
        } else if (key == "authmethod" || key == "auth_method") {
            bool ok = false;
            config.auth_method = parseAuthMethod(value, &ok);
            if (!ok && ctx) {
                ctx->message = "Unsupported auth_method";
            }
            if (!ok) {
                return core::Status::INVALID_ARGUMENT;
            }
        } else if (key == "allowpasswordfallback") {
            bool parsed = false;
            if (parseBool(value, parsed)) {
                config.allow_password_fallback = parsed;
            }
        } else if (key == "dsn" && config.host.empty()) {
            config.host = value;
        }
    }

    return core::Status::OK;
}

core::Status parseDriverConnectionString(const std::string& conn_str,
                                         NetworkClientConfig& config,
                                         core::ErrorContext* ctx) {
    std::string trimmed = trim(conn_str);
    if (trimmed.rfind("scratchbird://", 0) == 0) {
        auto status = parseScratchbirdUrl(trimmed, config, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    } else {
        std::map<std::string, std::string> params;
        auto status = parseKeyValueConnectionString(trimmed, params, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        status = applyConnectionParams(params, config, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    applyDriverDefaults(config);
    return core::Status::OK;
}

void applyDriverDefaults(NetworkClientConfig& config) {
    if (config.host.empty()) {
        config.host = "127.0.0.1";
    }
    if (config.application_name.empty()) {
        config.application_name = "scratchbird_driver";
    }
    applyDriverDefaultsFromEnv(config);
}

} // namespace client
} // namespace scratchbird
