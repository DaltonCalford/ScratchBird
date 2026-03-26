/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Client Library Implementation
 *
 * Client-side connection/runtime implementation used by native clients,
 * protocol adapters, and local control surfaces.
 */

#include "scratchbird/client/connection.h"
#include "scratchbird/client/sql_helpers.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/protocol/wire_protocol.h"
#include "scratchbird/core/firebird_datetime.h"
#include "scratchbird/parser/v3_compiler.h"
#include "scratchbird/security/scram_auth.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <iomanip>
#include <thread>
#include <iostream>
#include <limits>
#include <condition_variable>
#include <unordered_map>
#include <csignal>
#include <fcntl.h>
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
    #ifdef ERROR
        #undef ERROR
    #endif
    #ifdef DELETE
        #undef DELETE
    #endif
    #ifdef ABSOLUTE
        #undef ABSOLUTE
    #endif
    #ifdef OPTIONAL
        #undef OPTIONAL
    #endif
    #ifdef ID
        #undef ID
    #endif
#else
#include "scratchbird/core/posix_compat.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace scratchbird {
namespace client {

namespace {
constexpr const char* kServerNoticeChannel = "sb.notice";

bool myExecDebugEnabled() {
    static const bool enabled = []() {
        const char* value = std::getenv("SCRATCHBIRD_MY_DEBUG_EXEC");
        if (!value || value[0] == '\0') {
            return false;
        }
        std::string normalized(value);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        return normalized != "0" &&
               normalized != "FALSE" &&
               normalized != "NO" &&
               normalized != "OFF";
    }();
    return enabled;
}

const char* messageTypeName(protocol::MessageType type) {
    return protocol::messageTypeToString(type);
}

std::string bytesToHex(const std::vector<uint8_t>& data);
std::string formatUuidBytes(const std::vector<uint8_t>& data);
std::string describeUnexpectedResponse(const protocol::Message& response);

bool authDebugEnabled() {
    return true;
    const char* value = std::getenv("SCRATCHBIRD_AUTH_DEBUG");
    if (!value || value[0] == '\0') {
        return false;
    }
    return !(std::strcmp(value, "0") == 0 ||
             std::strcmp(value, "false") == 0 ||
             std::strcmp(value, "FALSE") == 0 ||
             std::strcmp(value, "no") == 0 ||
             std::strcmp(value, "NO") == 0);
}

std::string describeUnexpectedResponse(const protocol::Message& response) {
    std::string out = std::string("Unexpected response type: ") +
        protocol::messageTypeToString(response.getType());

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t error_code = 0;
        std::string sqlstate;
        std::string message;
        std::string detail;
        std::string hint;
        core::ErrorContext parse_ctx;
        if (protocol::ProtocolCodec::parseQueryError(
                response, error_code, sqlstate, message, detail, hint, &parse_ctx) ==
            core::Status::OK) {
            out += " [code=" + std::to_string(error_code);
            if (!sqlstate.empty()) {
                out += " sqlstate=" + sqlstate;
            }
            if (!message.empty()) {
                out += " message=\"" + message + "\"";
            }
            if (!detail.empty()) {
                out += " detail=\"" + detail + "\"";
            }
            if (!hint.empty()) {
                out += " hint=\"" + hint + "\"";
            }
            out += "]";
        } else if (!parse_ctx.message.empty()) {
            out += " [query_error_parse_failed=\"" + parse_ctx.message + "\"]";
        }
    }

    return out;
}

int64_t floorDiv(int64_t a, int64_t b) {
    if (b == 0) {
        return 0;
    }
    int64_t q = a / b;
    int64_t r = a % b;
    if ((r != 0) && ((r < 0) != (b < 0))) {
        --q;
    }
    return q;
}

bool parseTimeString(const std::string& text, int64_t& micros_out, int32_t& offset_seconds_out) {
    offset_seconds_out = 0;
    std::string time_part = text;
    size_t z_pos = time_part.find('Z');
    if (z_pos != std::string::npos) {
        time_part = time_part.substr(0, z_pos);
    }
    size_t offset_pos = time_part.find_last_of("+-");
    if (offset_pos != std::string::npos && offset_pos > 0 && time_part[offset_pos - 1] != 'e') {
        std::string offset = time_part.substr(offset_pos);
        time_part = time_part.substr(0, offset_pos);
        int sign = offset[0] == '-' ? -1 : 1;
        int hours = 0;
        int minutes = 0;
        if (std::sscanf(offset.c_str() + 1, "%d:%d", &hours, &minutes) >= 1) {
            offset_seconds_out = sign * (hours * 3600 + minutes * 60);
        }
    }

    int hour = 0;
    int minute = 0;
    int second = 0;
    int micros = 0;
    size_t dot = time_part.find('.');
    std::string base = time_part;
    std::string frac;
    if (dot != std::string::npos) {
        base = time_part.substr(0, dot);
        frac = time_part.substr(dot + 1);
    }
    if (std::sscanf(base.c_str(), "%d:%d:%d", &hour, &minute, &second) < 2) {
        return false;
    }
    if (!frac.empty()) {
        if (frac.size() > 6) {
            frac.resize(6);
        }
        while (frac.size() < 6) {
            frac.push_back('0');
        }
        micros = std::atoi(frac.c_str());
    }
    micros_out = (static_cast<int64_t>(hour) * 3600 +
                  static_cast<int64_t>(minute) * 60 +
                  static_cast<int64_t>(second)) * 1000000LL + micros;
    return true;
}

int32_t daysSince2000FromDateString(const std::string& text) {
    int32_t mjd = core::FirebirdDateTime::parseDate(text);
    if (mjd < 0) {
        return 0;
    }
    const int32_t base_mjd = core::FirebirdDateTime::dateToMJD(2000, 1, 1);
    return mjd - base_mjd;
}

std::string formatDateFromDaysSince2000(int32_t days_since_2000) {
    const int32_t base_mjd = core::FirebirdDateTime::dateToMJD(2000, 1, 1);
    int32_t mjd = base_mjd + days_since_2000;
    return core::FirebirdDateTime::formatDate(mjd);
}

std::string formatTimeFromMicros(int64_t micros) {
    const int64_t micros_per_day = 24LL * 60LL * 60LL * 1000000LL;
    int64_t normalized = micros % micros_per_day;
    if (normalized < 0) {
        normalized += micros_per_day;
    }
    int64_t total_seconds = normalized / 1000000;
    int64_t micro_remainder = normalized % 1000000;
    int hour = static_cast<int>(total_seconds / 3600);
    int minute = static_cast<int>((total_seconds / 60) % 60);
    int second = static_cast<int>(total_seconds % 60);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hour << ":"
        << std::setw(2) << minute << ":" << std::setw(2) << second;
    if (micro_remainder != 0) {
        oss << "." << std::setw(6) << micro_remainder;
    }
    return oss.str();
}

std::string formatTimestampFromMicros(int64_t micros) {
    int64_t seconds = floorDiv(micros, 1000000);
    int64_t micro_remainder = micros - seconds * 1000000;
    if (micro_remainder < 0) {
        micro_remainder += 1000000;
        --seconds;
    }
    int64_t days = floorDiv(seconds, 86400);
    int64_t seconds_of_day = seconds - days * 86400;
    if (seconds_of_day < 0) {
        seconds_of_day += 86400;
        --days;
    }
    int hour = static_cast<int>(seconds_of_day / 3600);
    int minute = static_cast<int>((seconds_of_day / 60) % 60);
    int second = static_cast<int>(seconds_of_day % 60);
    int32_t mjd = static_cast<int32_t>(days + core::FirebirdDateTime::UNIX_EPOCH_MJD);
    std::ostringstream oss;
    oss << core::FirebirdDateTime::formatDate(mjd) << " "
        << std::setfill('0') << std::setw(2) << hour << ":"
        << std::setw(2) << minute << ":" << std::setw(2) << second;
    if (micro_remainder != 0) {
        oss << "." << std::setw(6) << micro_remainder;
    }
    return oss.str();
}

std::string bytesToHex(const std::vector<uint8_t>& data) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size() * 2);
    for (uint8_t byte : data) {
        out.push_back(kHex[(byte >> 4) & 0x0F]);
        out.push_back(kHex[byte & 0x0F]);
    }
    return out;
}

std::string formatUuidBytes(const std::vector<uint8_t>& data) {
    if (data.size() != 16) {
        return "";
    }
    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  data[0], data[1], data[2], data[3],
                  data[4], data[5], data[6], data[7],
                  data[8], data[9], data[10], data[11],
                  data[12], data[13], data[14], data[15]);
    return std::string(buf);
}

bool supportsNegotiatedAuthMethod(protocol::AuthMethod method) {
    return method == protocol::AuthMethod::SCRAM_SHA_256 ||
           method == protocol::AuthMethod::SCRAM_SHA_512 ||
           method == protocol::AuthMethod::TOKEN ||
           method == protocol::AuthMethod::PEER ||
           method == protocol::AuthMethod::PASSWORD;
}

std::vector<protocol::AuthMethod> defaultPreferredAuthMethods() {
    return {
        protocol::AuthMethod::SCRAM_SHA_256,
        protocol::AuthMethod::SCRAM_SHA_512,
        protocol::AuthMethod::PEER,
        protocol::AuthMethod::PASSWORD,
        protocol::AuthMethod::MD5
    };
}

std::vector<protocol::AuthMethod> resolvePreferredAuthMethods(
    const std::vector<protocol::AuthMethod>& configured) {
    if (configured.empty()) {
        return defaultPreferredAuthMethods();
    }
    std::vector<protocol::AuthMethod> ordered = configured;
    const auto defaults = defaultPreferredAuthMethods();
    for (auto method : defaults) {
        if (std::find(ordered.begin(), ordered.end(), method) == ordered.end()) {
            ordered.push_back(method);
        }
    }
    return ordered;
}

bool hasConfiguredAuthToken(const ConnectionConfig& config) {
    return config.auth_token_authkey_id.size() == 16 &&
           !config.auth_token_secret.empty();
}

std::vector<protocol::AuthMethod> preferredMethodsForConfig(const ConnectionConfig& config) {
    std::vector<protocol::AuthMethod> ordered =
        resolvePreferredAuthMethods(config.preferred_auth_methods);

    if (hasConfiguredAuthToken(config)) {
        auto token_it = std::find(ordered.begin(),
                                  ordered.end(),
                                  protocol::AuthMethod::TOKEN);
        if (token_it != ordered.end()) {
            std::rotate(ordered.begin(), token_it, token_it + 1);
        } else {
            ordered.insert(ordered.begin(), protocol::AuthMethod::TOKEN);
        }
    }

    return ordered;
}

bool selectNegotiatedAuthMethod(const std::vector<protocol::AuthMethod>& allowed,
                                bool has_required,
                                protocol::AuthMethod required,
                                const std::vector<protocol::AuthMethod>& preferred,
                                protocol::AuthMethod& selected_out,
                                std::string& error_out) {
    error_out.clear();
    if (allowed.empty()) {
        error_out = "Server returned empty allowed auth methods";
        return false;
    }

    auto in_allowed = [&allowed](protocol::AuthMethod method) {
        return std::find(allowed.begin(), allowed.end(), method) != allowed.end();
    };

    if (has_required) {
        if (!in_allowed(required)) {
            error_out = "Server required auth method is not in allowed method set";
            return false;
        }
        if (!supportsNegotiatedAuthMethod(required)) {
            error_out = "Required auth method is not supported by this client";
            return false;
        }
        selected_out = required;
        return true;
    }

    for (auto method : preferred) {
        if (in_allowed(method) && supportsNegotiatedAuthMethod(method)) {
            selected_out = method;
            return true;
        }
    }

    error_out = "No mutually supported auth method";
    return false;
}

constexpr char kAuthRegistrySelectionMagic[] = "SBAPR1";

bool resolveAuthMethodSlot(const std::vector<protocol::AuthMethodRegistryEntry>& method_registry,
                           protocol::AuthMethod method,
                           uint16_t& slot_out) {
    const uint32_t legacy_wire_code = static_cast<uint32_t>(method);
    for (const auto& entry : method_registry) {
        if (!entry.has_legacy_wire_code) {
            continue;
        }
        if (entry.legacy_wire_code == legacy_wire_code) {
            slot_out = entry.method_slot;
            return true;
        }
    }
    return false;
}

std::vector<uint8_t> buildAuthRegistrySelectionPayload(uint16_t method_slot,
                                                       const std::vector<uint8_t>& auth_payload) {
    std::vector<uint8_t> payload;
    payload.reserve(8 + auth_payload.size());
    payload.insert(payload.end(),
                   kAuthRegistrySelectionMagic,
                   kAuthRegistrySelectionMagic + 6);
    payload.push_back(static_cast<uint8_t>(method_slot & 0xFFu));
    payload.push_back(static_cast<uint8_t>((method_slot >> 8) & 0xFFu));
    payload.insert(payload.end(), auth_payload.begin(), auth_payload.end());
    return payload;
}

bool timingSafeEqual(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    if (lhs.empty()) {
        return true;
    }
    return CRYPTO_memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

bool computeAuthKeyTokenProof(const std::string& token_secret,
                              const std::string& username,
                              const uint8_t authkey_id[16],
                              const std::vector<uint8_t>& binding,
                              std::vector<uint8_t>& proof_out) {
    proof_out.clear();
    if (token_secret.empty()) {
        return false;
    }

    static constexpr char kTokenProofPrefix[] = "SB-AUTHKEY-TOKEN-V1";

    std::vector<uint8_t> message;
    message.reserve(sizeof(kTokenProofPrefix) - 1 + username.size() + 16 + binding.size());
    message.insert(message.end(),
                   reinterpret_cast<const uint8_t*>(kTokenProofPrefix),
                   reinterpret_cast<const uint8_t*>(kTokenProofPrefix) + (sizeof(kTokenProofPrefix) - 1));
    message.insert(message.end(), username.begin(), username.end());
    message.insert(message.end(), authkey_id, authkey_id + 16);
    message.insert(message.end(), binding.begin(), binding.end());

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (!HMAC(EVP_sha256(),
              token_secret.data(),
              static_cast<int>(token_secret.size()),
              message.data(),
              message.size(),
              digest,
              &digest_len)) {
        return false;
    }

    proof_out.assign(digest, digest + digest_len);
    return true;
}

constexpr const char* kMfaPayloadPrefix = "SBMFA1";

struct MfaChallengeData {
    std::string challenge_id;
    std::string method_name;
    bool has_server_final = false;
    std::string server_final;
};

std::vector<std::string> splitPipeFields(const std::string& input) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= input.size()) {
        const size_t delim = input.find('|', start);
        if (delim == std::string::npos) {
            fields.push_back(input.substr(start));
            break;
        }
        fields.push_back(input.substr(start, delim - start));
        start = delim + 1;
    }
    return fields;
}

bool parseMfaChallengePayload(const std::vector<uint8_t>& payload,
                              MfaChallengeData& challenge_out,
                              std::string& error_out) {
    challenge_out = MfaChallengeData{};
    error_out.clear();
    if (payload.empty()) {
        error_out = "MFA challenge payload is empty";
        return false;
    }

    const std::string raw(payload.begin(), payload.end());
    const std::vector<std::string> parts = splitPipeFields(raw);
    if (parts.size() < 3 || parts[0] != kMfaPayloadPrefix) {
        error_out = "Invalid MFA challenge payload format";
        return false;
    }
    if (parts[1].empty() || parts[2].empty()) {
        error_out = "MFA challenge payload is incomplete";
        return false;
    }

    challenge_out.challenge_id = parts[1];
    challenge_out.method_name = parts[2];
    if (parts.size() >= 4 && !parts[3].empty()) {
        std::vector<uint8_t> decoded = security::base64Decode(parts[3]);
        challenge_out.server_final.assign(decoded.begin(), decoded.end());
        challenge_out.has_server_final = true;
    }
    return true;
}

std::vector<uint8_t> buildMfaResponsePayload(const std::string& challenge_id,
                                             const std::string& code) {
    const std::string payload =
        std::string(kMfaPayloadPrefix) + "|" + challenge_id + "|" + code;
    return std::vector<uint8_t>(payload.begin(), payload.end());
}

bool parseScramAttributes(const std::string& message,
                          std::unordered_map<char, std::string>& attrs_out) {
    attrs_out.clear();
    if (message.empty()) {
        return false;
    }

    size_t start = 0;
    while (start < message.size()) {
        size_t end = message.find(',', start);
        if (end == std::string::npos) {
            end = message.size();
        }
        if (end <= start + 1) {
            return false;
        }
        if (message[start + 1] != '=') {
            return false;
        }

        const char key = message[start];
        if (attrs_out.find(key) != attrs_out.end()) {
            return false;
        }
        attrs_out[key] = message.substr(start + 2, end - (start + 2));

        if (end == message.size()) {
            break;
        }
        start = end + 1;
    }

    return !attrs_out.empty();
}

bool parseUint32Strict(const std::string& text, uint32_t& value_out) {
    if (text.empty()) {
        return false;
    }

    try {
        size_t consumed = 0;
        unsigned long parsed = std::stoul(text, &consumed, 10);
        if (consumed != text.size() ||
            parsed > static_cast<unsigned long>(std::numeric_limits<uint32_t>::max())) {
            return false;
        }
        value_out = static_cast<uint32_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

struct ScramServerFirstData {
    std::string nonce;
    std::vector<uint8_t> salt;
    uint32_t iterations = 0;
};

bool parseScramServerFirstMessage(const std::string& server_first,
                                  const std::string& expected_client_nonce,
                                  ScramServerFirstData& parsed_out,
                                  std::string& error_out) {
    parsed_out = ScramServerFirstData{};
    error_out.clear();

    std::unordered_map<char, std::string> attrs;
    if (!parseScramAttributes(server_first, attrs)) {
        error_out = "Invalid SCRAM server-first message";
        return false;
    }

    auto nonce_it = attrs.find('r');
    auto salt_it = attrs.find('s');
    auto iteration_it = attrs.find('i');
    if (nonce_it == attrs.end() || salt_it == attrs.end() || iteration_it == attrs.end()) {
        error_out = "SCRAM server-first message missing required attributes";
        return false;
    }
    if (nonce_it->second.rfind(expected_client_nonce, 0) != 0) {
        error_out = "SCRAM server nonce does not include client nonce";
        return false;
    }

    uint32_t iterations = 0;
    if (!parseUint32Strict(iteration_it->second, iterations) || iterations == 0) {
        error_out = "SCRAM server-first iteration count is invalid";
        return false;
    }

    std::vector<uint8_t> salt = security::base64Decode(salt_it->second);
    if (salt.empty()) {
        error_out = "SCRAM server-first salt is invalid";
        return false;
    }

    parsed_out.nonce = nonce_it->second;
    parsed_out.iterations = iterations;
    parsed_out.salt = std::move(salt);
    return true;
}

bool parseScramServerFinalMessage(const std::string& server_final,
                                  std::string& verifier_out,
                                  std::string& error_out) {
    verifier_out.clear();
    error_out.clear();

    std::unordered_map<char, std::string> attrs;
    if (!parseScramAttributes(server_final, attrs)) {
        error_out = "Invalid SCRAM server-final message";
        return false;
    }
    auto error_it = attrs.find('e');
    if (error_it != attrs.end()) {
        error_out = "SCRAM server-final error: " + error_it->second;
        return false;
    }
    auto verifier_it = attrs.find('v');
    if (verifier_it == attrs.end() || verifier_it->second.empty()) {
        error_out = "SCRAM server-final verifier is missing";
        return false;
    }
    verifier_out = verifier_it->second;
    return true;
}

const EVP_MD* scramDigest(security::ScramAlgorithm algorithm) {
    return (algorithm == security::ScramAlgorithm::SHA_256) ? EVP_sha256() : EVP_sha512();
}

bool scramHmac(const std::vector<uint8_t>& key,
               const std::string& message,
               security::ScramAlgorithm algorithm,
               std::vector<uint8_t>& out) {
    unsigned int len = 0;
    unsigned char buf[EVP_MAX_MD_SIZE];
    if (!HMAC(scramDigest(algorithm),
              key.data(),
              static_cast<int>(key.size()),
              reinterpret_cast<const unsigned char*>(message.data()),
              message.size(),
              buf,
              &len)) {
        return false;
    }
    out.assign(buf, buf + len);
    return true;
}

bool scramHash(const std::vector<uint8_t>& input,
               security::ScramAlgorithm algorithm,
               std::vector<uint8_t>& out) {
    unsigned char buf[SHA512_DIGEST_LENGTH];
    if (algorithm == security::ScramAlgorithm::SHA_256) {
        SHA256(input.data(), input.size(), buf);
        out.assign(buf, buf + SHA256_DIGEST_LENGTH);
        return true;
    }
    SHA512(input.data(), input.size(), buf);
    out.assign(buf, buf + SHA512_DIGEST_LENGTH);
    return true;
}

bool buildScramClientFinalMessage(const std::string& password,
                                  security::ScramAlgorithm algorithm,
                                  const std::string& client_first_bare,
                                  const std::string& server_first,
                                  const ScramServerFirstData& parsed_server_first,
                                  std::string& client_final_out,
                                  std::string& expected_server_verifier_out,
                                  std::string& error_out) {
    client_final_out.clear();
    expected_server_verifier_out.clear();
    error_out.clear();

    std::vector<uint8_t> salted_password;
    core::Status status = security::calculateSaltedPassword(
        password,
        parsed_server_first.salt,
        parsed_server_first.iterations,
        algorithm,
        salted_password);
    if (status != core::Status::OK) {
        error_out = "Failed to derive SCRAM salted password";
        return false;
    }

    std::vector<uint8_t> client_key;
    if (!scramHmac(salted_password, "Client Key", algorithm, client_key)) {
        error_out = "Failed to compute SCRAM client key";
        return false;
    }

    std::vector<uint8_t> stored_key;
    if (!scramHash(client_key, algorithm, stored_key)) {
        error_out = "Failed to compute SCRAM stored key";
        return false;
    }

    std::vector<uint8_t> server_key;
    if (!scramHmac(salted_password, "Server Key", algorithm, server_key)) {
        error_out = "Failed to compute SCRAM server key";
        return false;
    }

    const std::string client_final_without_proof = "c=biws,r=" + parsed_server_first.nonce;
    const std::string auth_message = client_first_bare + "," + server_first + "," +
                                     client_final_without_proof;

    std::vector<uint8_t> client_signature;
    if (!scramHmac(stored_key, auth_message, algorithm, client_signature)) {
        error_out = "Failed to compute SCRAM client signature";
        return false;
    }

    std::vector<uint8_t> client_proof = client_key;
    security::xorBytes(client_proof, client_signature);
    client_final_out = client_final_without_proof + ",p=" + security::base64Encode(client_proof);

    std::vector<uint8_t> server_signature;
    if (!scramHmac(server_key, auth_message, algorithm, server_signature)) {
        error_out = "Failed to compute SCRAM server signature";
        return false;
    }
    expected_server_verifier_out = security::base64Encode(server_signature);

    std::fill(salted_password.begin(), salted_password.end(), 0);
    std::fill(client_key.begin(), client_key.end(), 0);
    std::fill(stored_key.begin(), stored_key.end(), 0);
    std::fill(server_key.begin(), server_key.end(), 0);
    std::fill(client_signature.begin(), client_signature.end(), 0);
    std::fill(client_proof.begin(), client_proof.end(), 0);
    std::fill(server_signature.begin(), server_signature.end(), 0);
    return true;
}

int64_t microsFromTimestampString(const std::string& text) {
    std::string date_part;
    std::string time_part;
    size_t split = text.find('T');
    if (split == std::string::npos) {
        split = text.find(' ');
    }
    if (split == std::string::npos) {
        return 0;
    }
    date_part = text.substr(0, split);
    time_part = text.substr(split + 1);

    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(date_part.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return 0;
    }

    int64_t time_micros = 0;
    int32_t offset_seconds = 0;
    if (!parseTimeString(time_part, time_micros, offset_seconds)) {
        return 0;
    }

    int32_t mjd = core::FirebirdDateTime::dateToMJD(year, month, day);
    int64_t base_seconds =
        static_cast<int64_t>(mjd - core::FirebirdDateTime::UNIX_EPOCH_MJD) * 86400;
    int64_t total_seconds = base_seconds + time_micros / 1000000;
    int64_t micros = (time_micros % 1000000);
    if (micros < 0) {
        micros += 1000000;
        --total_seconds;
    }
    total_seconds -= offset_seconds;
    return total_seconds * 1000000 + micros;
}
} // namespace

// Helper to check if status is OK
inline bool isOk(core::Status status) {
    return status == core::Status::OK;
}

// ============================================================================
// ResultSet Implementation
// ============================================================================

class ResultSetImpl {
public:
    std::vector<ColumnMeta> columns_;
    std::vector<std::vector<protocol::ProtocolCodec::ColumnValue>> rows_;
    std::vector<std::string> notices_;
    int64_t current_row_ = -1;
    int64_t row_count_ = -1;
    int64_t rows_affected_ = 0;
    std::string command_tag_;

    void clear() {
        columns_.clear();
        rows_.clear();
        notices_.clear();
        current_row_ = -1;
        row_count_ = -1;
        rows_affected_ = 0;
        command_tag_.clear();
    }
};

ResultSet::ResultSet() : impl_(std::make_unique<ResultSetImpl>()) {}
ResultSet::~ResultSet() = default;

ResultSet::ResultSet(ResultSet&& other) noexcept = default;
ResultSet& ResultSet::operator=(ResultSet&& other) noexcept = default;

size_t ResultSet::getColumnCount() const {
    return impl_->columns_.size();
}

std::string ResultSet::getColumnName(size_t index) const {
    if (index >= impl_->columns_.size()) return "";
    return impl_->columns_[index].name;
}

int ResultSet::getColumnIndex(const std::string& name) const {
    // Case-insensitive comparison
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    for (size_t i = 0; i < impl_->columns_.size(); ++i) {
        std::string col_lower = impl_->columns_[i].name;
        std::transform(col_lower.begin(), col_lower.end(), col_lower.begin(), ::tolower);
        if (col_lower == lower_name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

protocol::WireType ResultSet::getColumnType(size_t index) const {
    if (index >= impl_->columns_.size()) return protocol::WireType::UNKNOWN;
    return impl_->columns_[index].type;
}

const std::vector<ColumnMeta>& ResultSet::getColumns() const {
    return impl_->columns_;
}

int64_t ResultSet::getRowCount() const {
    return impl_->row_count_ >= 0 ? impl_->row_count_ : static_cast<int64_t>(impl_->rows_.size());
}

int64_t ResultSet::getRowsAffected() const {
    return impl_->rows_affected_;
}

bool ResultSet::isEmpty() const {
    return impl_->rows_.empty();
}

const std::vector<protocol::ProtocolCodec::ColumnValue>& ResultSet::getRowValues(size_t index) const {
    static const std::vector<protocol::ProtocolCodec::ColumnValue> kEmptyRow;
    if (index >= impl_->rows_.size()) {
        return kEmptyRow;
    }
    return impl_->rows_[index];
}

const std::string& ResultSet::getCommandTag() const {
    return impl_->command_tag_;
}

const std::vector<std::string>& ResultSet::getNotices() const {
    return impl_->notices_;
}

bool ResultSet::next() {
    if (impl_->current_row_ + 1 < static_cast<int64_t>(impl_->rows_.size())) {
        ++impl_->current_row_;
        return true;
    }
    return false;
}

void ResultSet::reset() {
    impl_->current_row_ = -1;
}

int64_t ResultSet::getCurrentRow() const {
    return impl_->current_row_;
}

bool ResultSet::isNull(size_t column) const {
    if (impl_->current_row_ < 0 ||
        impl_->current_row_ >= static_cast<int64_t>(impl_->rows_.size()) ||
        column >= impl_->rows_[impl_->current_row_].size()) {
        return true;
    }
    return impl_->rows_[impl_->current_row_][column].is_null;
}

bool ResultSet::getBool(size_t column) const {
    if (isNull(column)) return false;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.empty()) return false;
    return val.data[0] != 0;
}

int16_t ResultSet::getInt16(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 2) return 0;
    int16_t result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

int32_t ResultSet::getInt32(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 4) return 0;
    int32_t result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

int64_t ResultSet::getInt64(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 8) return 0;
    int64_t result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

float ResultSet::getFloat(size_t column) const {
    if (isNull(column)) return 0.0f;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 4) return 0.0f;
    float result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

double ResultSet::getDouble(size_t column) const {
    if (isNull(column)) return 0.0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 8) return 0.0;
    double result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

std::string ResultSet::getString(size_t column) const {
    if (isNull(column)) return "";
    const auto& val = impl_->rows_[impl_->current_row_][column];
    protocol::WireType type = protocol::WireType::UNKNOWN;
    if (column < impl_->columns_.size()) {
        type = impl_->columns_[column].type;
    }
    switch (type) {
        case protocol::WireType::BOOLEAN:
            return (!val.data.empty() && val.data[0]) ? "true" : "false";
        case protocol::WireType::INT16: {
            if (val.data.size() < sizeof(int16_t)) return "0";
            int16_t out;
            std::memcpy(&out, val.data.data(), sizeof(out));
            return std::to_string(out);
        }
        case protocol::WireType::INT32: {
            if (val.data.size() < sizeof(int32_t)) return "0";
            int32_t out;
            std::memcpy(&out, val.data.data(), sizeof(out));
            return std::to_string(out);
        }
        case protocol::WireType::INT64: {
            if (val.data.size() < sizeof(int64_t)) return "0";
            int64_t out;
            std::memcpy(&out, val.data.data(), sizeof(out));
            return std::to_string(out);
        }
        case protocol::WireType::FLOAT32:
        case protocol::WireType::FLOAT64: {
            if (val.data.size() < sizeof(double)) return "0";
            double out;
            std::memcpy(&out, val.data.data(), sizeof(out));
            std::ostringstream oss;
            oss.precision(17);
            oss << out;
            return oss.str();
        }
        case protocol::WireType::DATE:
            return formatDateFromDaysSince2000(getDate(column));
        case protocol::WireType::TIME:
            return formatTimeFromMicros(getTime(column));
        case protocol::WireType::TIMESTAMP:
        case protocol::WireType::TIMESTAMPTZ:
            return formatTimestampFromMicros(getTimestamp(column));
        case protocol::WireType::UUID:
            return getUUID(column);
        case protocol::WireType::BYTEA:
            return bytesToHex(val.data);
        default:
            break;
    }
    return std::string(reinterpret_cast<const char*>(val.data.data()), val.data.size());
}

std::vector<uint8_t> ResultSet::getBytes(size_t column) const {
    if (isNull(column)) return {};
    return impl_->rows_[impl_->current_row_][column].data;
}

int64_t ResultSet::getTimestamp(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    protocol::WireType type = protocol::WireType::UNKNOWN;
    if (column < impl_->columns_.size()) {
        type = impl_->columns_[column].type;
    }
    if (type != protocol::WireType::TIMESTAMP &&
        type != protocol::WireType::TIMESTAMPTZ &&
        val.data.size() >= sizeof(int64_t)) {
        int64_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    if (val.data.size() == sizeof(int64_t) &&
        (type == protocol::WireType::TIMESTAMP || type == protocol::WireType::TIMESTAMPTZ)) {
        int64_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    std::string text(reinterpret_cast<const char*>(val.data.data()), val.data.size());
    return microsFromTimestampString(text);
}

int32_t ResultSet::getDate(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    protocol::WireType type = protocol::WireType::UNKNOWN;
    if (column < impl_->columns_.size()) {
        type = impl_->columns_[column].type;
    }
    if (type != protocol::WireType::DATE && val.data.size() >= sizeof(int32_t)) {
        int32_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    if (val.data.size() == sizeof(int32_t) && type == protocol::WireType::DATE) {
        int32_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    std::string text(reinterpret_cast<const char*>(val.data.data()), val.data.size());
    return daysSince2000FromDateString(text);
}

int64_t ResultSet::getTime(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    protocol::WireType type = protocol::WireType::UNKNOWN;
    if (column < impl_->columns_.size()) {
        type = impl_->columns_[column].type;
    }
    if (type != protocol::WireType::TIME && val.data.size() >= sizeof(int64_t)) {
        int64_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    if (val.data.size() == sizeof(int64_t) && type == protocol::WireType::TIME) {
        int64_t out;
        std::memcpy(&out, val.data.data(), sizeof(out));
        return out;
    }
    std::string text(reinterpret_cast<const char*>(val.data.data()), val.data.size());
    int64_t micros = 0;
    int32_t offset_seconds = 0;
    if (!parseTimeString(text, micros, offset_seconds)) {
        return 0;
    }
    return micros - static_cast<int64_t>(offset_seconds) * 1000000LL;
}

std::string ResultSet::getUUID(size_t column) const {
    if (isNull(column)) return "";
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() == 16) {
        return formatUuidBytes(val.data);
    }
    return std::string(reinterpret_cast<const char*>(val.data.data()), val.data.size());
}

const uint8_t* ResultSet::getRaw(size_t column, size_t* length) const {
    if (isNull(column)) {
        if (length) *length = 0;
        return nullptr;
    }
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (length) *length = val.data.size();
    return val.data.data();
}

// By-name accessors
bool ResultSet::isNull(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx < 0 || isNull(static_cast<size_t>(idx));
}

bool ResultSet::getBool(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getBool(static_cast<size_t>(idx)) : false;
}

int16_t ResultSet::getInt16(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getInt16(static_cast<size_t>(idx)) : 0;
}

int32_t ResultSet::getInt32(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getInt32(static_cast<size_t>(idx)) : 0;
}

int64_t ResultSet::getInt64(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getInt64(static_cast<size_t>(idx)) : 0;
}

float ResultSet::getFloat(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getFloat(static_cast<size_t>(idx)) : 0.0f;
}

double ResultSet::getDouble(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getDouble(static_cast<size_t>(idx)) : 0.0;
}

std::string ResultSet::getString(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getString(static_cast<size_t>(idx)) : "";
}

std::vector<uint8_t> ResultSet::getBytes(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getBytes(static_cast<size_t>(idx)) : std::vector<uint8_t>{};
}

int64_t ResultSet::getTimestamp(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getTimestamp(static_cast<size_t>(idx)) : 0;
}

int32_t ResultSet::getDate(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getDate(static_cast<size_t>(idx)) : 0;
}

int64_t ResultSet::getTime(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getTime(static_cast<size_t>(idx)) : 0;
}

std::string ResultSet::getUUID(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getUUID(static_cast<size_t>(idx)) : "";
}

// ============================================================================
// PreparedStatement Implementation
// ============================================================================

class PreparedStatementImpl {
public:
    std::string sql_;
    uint32_t statement_id_ = 0;
    size_t param_count_ = 0;
    std::vector<protocol::ProtocolCodec::ColumnValue> params_;
    std::vector<protocol::WireType> param_types_;
    bool valid_ = false;
};

PreparedStatement::PreparedStatement() : impl_(std::make_unique<PreparedStatementImpl>()) {}
PreparedStatement::~PreparedStatement() = default;

PreparedStatement::PreparedStatement(PreparedStatement&& other) noexcept = default;
PreparedStatement& PreparedStatement::operator=(PreparedStatement&& other) noexcept = default;

const std::string& PreparedStatement::getSQL() const {
    return impl_->sql_;
}

size_t PreparedStatement::getParameterCount() const {
    return impl_->param_count_;
}

bool PreparedStatement::isValid() const {
    return impl_->valid_;
}

void PreparedStatement::setNull(size_t index) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue(nullptr);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::NULL_TYPE;
    }
}

void PreparedStatement::setNull(size_t index, protocol::WireType type) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue(nullptr);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = type;
    }
}

void PreparedStatement::setBool(size_t index, bool value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBool(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::BOOLEAN;
    }
}

void PreparedStatement::setInt16(size_t index, int16_t value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt32(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::INT16;
    }
}

void PreparedStatement::setInt32(size_t index, int32_t value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt32(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::INT32;
    }
}

void PreparedStatement::setInt64(size_t index, int64_t value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt64(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::INT64;
    }
}

void PreparedStatement::setFloat(size_t index, float value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromDouble(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::FLOAT32;
    }
}

void PreparedStatement::setDouble(size_t index, double value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromDouble(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::FLOAT64;
    }
}

void PreparedStatement::setString(size_t index, const std::string& value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromString(value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::VARCHAR;
    }
}

void PreparedStatement::setBytes(size_t index, const std::vector<uint8_t>& value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(value.data(), value.size());
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::BYTEA;
    }
}

void PreparedStatement::setBytes(size_t index, const uint8_t* data, size_t length) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(data, length);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::BYTEA;
    }
}

void PreparedStatement::setTimestamp(size_t index, int64_t microseconds) {
    setInt64(index, microseconds);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::TIMESTAMP;
    }
}

void PreparedStatement::setDate(size_t index, int32_t days) {
    setInt32(index, days);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::DATE;
    }
}

void PreparedStatement::setTime(size_t index, int64_t microseconds) {
    setInt64(index, microseconds);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::TIME;
    }
}

void PreparedStatement::setUUID(size_t index, const std::vector<uint8_t>& value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(value.data(), value.size());
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::UUID;
    }
}

void PreparedStatement::setUUID(size_t index, const std::string& value) {
    setString(index, value);
    if (index <= impl_->param_types_.size()) {
        impl_->param_types_[index - 1] = protocol::WireType::UUID;
    }
}

void PreparedStatement::clearParameters() {
    for (auto& p : impl_->params_) {
        p = protocol::ProtocolCodec::ColumnValue(nullptr);
    }
    for (auto& t : impl_->param_types_) {
        t = protocol::WireType::UNKNOWN;
    }
}

// ============================================================================
// Connection Implementation
// ============================================================================

class ConnectionImpl {
public:
    ConnectionConfig config_;
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    std::string last_error_;

    // IPC client and protocol session
    std::unique_ptr<server::IPCClient> ipc_client_;
    std::unique_ptr<protocol::ProtocolSession> protocol_session_;

    // Session info
    uint8_t session_id_[16] = {0};
    std::string server_version_;
    bool in_transaction_ = false;
    bool auto_commit_ = true;
    std::istream* copy_input_stream_ = nullptr;
    std::ostream* copy_output_stream_ = nullptr;
    std::function<void(uint64_t, uint64_t)> progress_callback_;
    bool auth_negotiation_ready_ = false;
    std::string auth_negotiation_username_;
    std::vector<protocol::AuthMethod> auth_negotiation_allowed_methods_;
    bool auth_negotiation_has_required_method_ = false;
    protocol::AuthMethod auth_negotiation_required_method_ =
        protocol::AuthMethod::SCRAM_SHA_256;
    uint8_t auth_negotiation_transport_mask_ = 0;
    std::vector<uint8_t> auth_negotiation_nonce_;
    std::vector<protocol::AuthMethodRegistryEntry> auth_negotiation_method_registry_;

    // ============================
    // Connection helpers
    // ============================

    core::Status doConnect(core::ErrorContext* ctx) {
        // Check if server is running, with retry for race conditions
        // Two clients might both see no server and both try to start one.
        // The first one wins the database lock, the second one should connect to it.
        if (config_.auto_start_server) {
            int max_retries = 3;
            for (int retry = 0; retry < max_retries; ++retry) {
                if (server::isServerRunning(config_.database_name)) {
                    break;  // Server is running, proceed to connect
                }

                auto status = Connection::startServer(
                    config_.database_name,
                    config_.server_executable,
                    config_.auto_start_timeout_ms,
                    ctx
                );

                if (isOk(status)) {
                    break;  // Server started successfully
                }

                // Server start failed - might be because another client started one
                // Wait briefly and check if a server is now running
                std::this_thread::sleep_for(std::chrono::milliseconds(200 * (retry + 1)));

                if (server::isServerRunning(config_.database_name)) {
                    break;  // Another client started the server, we can connect to it
                }

                // Last retry - report the failure
                if (retry == max_retries - 1) {
                    last_error_ = "Failed to auto-start server (database may be locked by another process)";
                    return status;
                }
            }
        }

        // Create IPC client
        server::IPCClientConfig ipc_config;
        ipc_config.database_name = config_.database_name;
        ipc_config.method = config_.ipc_method;
        ipc_config.tcp_port = config_.tcp_port;
        ipc_config.connect_timeout_ms = config_.connect_timeout_ms;
        ipc_config.read_timeout_ms = config_.read_timeout_ms;
        ipc_config.write_timeout_ms = config_.write_timeout_ms;
        if (!config_.socket_path.empty()) {
            ipc_config.socket_path = config_.socket_path;
        }

        ipc_client_ = server::IPCClient::create(ipc_config, ctx);
        if (!ipc_client_) {
            last_error_ = "Failed to create IPC client";
            return core::Status::CONNECTION_FAILURE;
        }

        // Connect to server
        state_ = ConnectionState::CONNECTING;
        auto status = ipc_client_->connect(ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to connect to server";
            state_ = ConnectionState::ERROR_STATE;
            return status;
        }

        // Create protocol session
        protocol_session_ = std::make_unique<protocol::ProtocolSession>(
            ipc_client_->getConnection()
        );

        // Send CONNECT_REQUEST
        const uint8_t* bound_db_uuid_ptr = nullptr;
        if (config_.has_bound_db_uuid) {
            bound_db_uuid_ptr = config_.bound_db_uuid.data();
        }
        const uint8_t* dormant_id_ptr = nullptr;
        const uint8_t* dormant_reattach_authkey_ptr = nullptr;
        uint16_t connect_flags = config_.connect_client_flags;
        if (!config_.auto_commit) {
            connect_flags |= protocol::CONNECT_FLAG_AUTOCOMMIT_OFF;
        }
        if (config_.has_dormant_reattach) {
            connect_flags |= protocol::CONNECT_FLAG_DORMANT_REATTACH;
            dormant_id_ptr = config_.dormant_id.data();
            dormant_reattach_authkey_ptr = config_.dormant_reattach_authkey.data();
        }
        auto connect_msg = protocol::ProtocolCodec::buildConnectRequest(
            config_.database_name,
            config_.client_name.empty() ? "scratchbird_client" : config_.client_name,
            getpid(),
            connect_flags,
            bound_db_uuid_ptr,
            dormant_id_ptr,
            dormant_reattach_authkey_ptr
        );

        status = protocol_session_->sendMessage(connect_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send connect request";
            state_ = ConnectionState::ERROR_STATE;
            return status;
        }

        // Receive CONNECT_RESPONSE
        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive connect response";
            state_ = ConnectionState::ERROR_STATE;
            return status;
        }

        if (response.getType() != protocol::MessageType::CONNECT_RESPONSE) {
            last_error_ = describeUnexpectedResponse(response);
            state_ = ConnectionState::ERROR_STATE;
            return core::Status::PROTOCOL_VIOLATION;
        }

        bool success;
        std::string error_msg;
        status = protocol::ProtocolCodec::parseConnectResponse(
            response, success, session_id_, error_msg, nullptr, ctx
        );
        if (!isOk(status) || !success) {
            last_error_ = error_msg.empty() ? "Connection refused" : error_msg;
            state_ = ConnectionState::ERROR_STATE;
            return core::Status::CONNECTION_FAILURE;
        }

        // Authenticate if credentials provided (or bootstrap on fresh DB).
        if (!config_.manual_auth) {
            if (config_.username.empty()) {
                config_.username = "bootstrap";
            }
            status = doAuthenticate(ctx);
            if (!isOk(status)) {
                return status;
            }
        }

        auto_commit_ = config_.auto_commit;
        in_transaction_ = !auto_commit_;
        state_ = in_transaction_ ? ConnectionState::IN_TRANSACTION
                                 : ConnectionState::CONNECTED;
        return core::Status::OK;
    }

    void clearAuthNegotiationState() {
        auth_negotiation_ready_ = false;
        auth_negotiation_username_.clear();
        auth_negotiation_allowed_methods_.clear();
        auth_negotiation_has_required_method_ = false;
        auth_negotiation_required_method_ = protocol::AuthMethod::SCRAM_SHA_256;
        auth_negotiation_transport_mask_ = 0;
        auth_negotiation_nonce_.clear();
        auth_negotiation_method_registry_.clear();
    }

    bool validateNegotiatedAuthMethod(protocol::AuthMethod method,
                                      std::string& error_out) const {
        error_out.clear();
        if (!auth_negotiation_ready_) {
            error_out = "Authentication negotiation state is not initialized";
            return false;
        }
        const bool allowed =
            std::find(auth_negotiation_allowed_methods_.begin(),
                      auth_negotiation_allowed_methods_.end(),
                      method) != auth_negotiation_allowed_methods_.end();
        if (!allowed) {
            error_out = "Requested authentication method is not allowed by server policy";
            return false;
        }
        if (auth_negotiation_has_required_method_ &&
            method != auth_negotiation_required_method_) {
            error_out = "Server policy requires a different authentication method";
            return false;
        }
        return true;
    }

    core::Status ensureAuthNegotiation(const std::string& username,
                                       core::ErrorContext* ctx) {
        if (!protocol_session_) {
            last_error_ = "Not connected";
            return core::Status::CONNECTION_FAILURE;
        }
        if (auth_negotiation_ready_ && auth_negotiation_username_ == username) {
            return core::Status::OK;
        }

        clearAuthNegotiationState();

        auto negotiate_msg = protocol::ProtocolCodec::buildAuthRequest(
            session_id_,
            username,
            protocol::AuthMethod::PASSWORD,
            {}
        );

        if (authDebugEnabled()) {
            std::fprintf(stderr,
                         "[auth_debug_client] sending negotiation request user=%s\n",
                         username.c_str());
        }

        auto status = protocol_session_->sendMessage(negotiate_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send auth negotiation request";
            return status;
        }

        protocol::Message negotiation_response;
        status = protocol_session_->receiveMessage(negotiation_response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive auth negotiation response";
            return status;
        }

        if (authDebugEnabled()) {
            std::fprintf(stderr,
                         "[auth_debug_client] negotiation response type=%u\n",
                         static_cast<unsigned>(negotiation_response.getType()));
        }

        if (negotiation_response.getType() == protocol::MessageType::AUTH_RESPONSE) {
            protocol::AuthStatus auth_status = protocol::AuthStatus::FAILURE;
            uint32_t user_id = 0;
            std::string error_msg;
            std::vector<uint8_t> data;
            status = protocol::ProtocolCodec::parseAuthResponse(
                negotiation_response, auth_status, user_id, error_msg, &data, ctx);
            if (!isOk(status)) {
                last_error_ = "Failed to parse auth negotiation fallback response";
                return status;
            }
            if (auth_status == protocol::AuthStatus::FAILURE) {
                last_error_ = error_msg.empty()
                    ? "Authentication negotiation failed"
                    : error_msg;
                return core::Status::INVALID_AUTHORIZATION;
            }
            last_error_ = "Server did not return an auth negotiation challenge";
            return core::Status::PROTOCOL_VIOLATION;
        }

        if (negotiation_response.getType() != protocol::MessageType::AUTH_CHALLENGE) {
            last_error_ = "Unexpected auth negotiation response type";
            return core::Status::PROTOCOL_VIOLATION;
        }

        uint8_t challenge_session_id[16];
        std::string challenge_username;
        std::vector<protocol::AuthMethod> allowed_methods;
        bool has_required_method = false;
        protocol::AuthMethod required_method = protocol::AuthMethod::SCRAM_SHA_256;
        uint8_t allowed_transport_mask = 0;
        std::vector<uint8_t> challenge_nonce;
        std::vector<protocol::AuthMethodRegistryEntry> method_registry;
        status = protocol::ProtocolCodec::parseAuthChallenge(
            negotiation_response,
            challenge_session_id,
            challenge_username,
            allowed_methods,
            has_required_method,
            required_method,
            allowed_transport_mask,
            challenge_nonce,
            ctx,
            &method_registry);
        if (!isOk(status)) {
            last_error_ = "Failed to parse auth negotiation payload";
            if (authDebugEnabled()) {
                std::fprintf(stderr,
                             "[auth_debug_client] parse auth challenge failed status=%d msg=%s\n",
                             static_cast<int>(status),
                             ctx && !ctx->message.empty() ? ctx->message.c_str() : "none");
            }
            return status;
        }

        if (std::memcmp(challenge_session_id, session_id_, sizeof(session_id_)) != 0 ||
            challenge_username != username) {
            last_error_ = "Auth negotiation payload/session mismatch";
            if (authDebugEnabled()) {
                std::fprintf(stderr,
                             "[auth_debug_client] challenge mismatch user=%s expected_user=%s\n",
                             challenge_username.c_str(),
                             username.c_str());
            }
            return core::Status::PROTOCOL_VIOLATION;
        }

        if (authDebugEnabled()) {
            std::fprintf(stderr,
                         "[auth_debug_client] challenge parsed allowed=%zu required=%d required_method=%u nonce_len=%zu\n",
                         allowed_methods.size(),
                         has_required_method ? 1 : 0,
                         static_cast<unsigned>(required_method),
                         challenge_nonce.size());
        }

        auth_negotiation_ready_ = true;
        auth_negotiation_username_ = challenge_username;
        auth_negotiation_allowed_methods_ = std::move(allowed_methods);
        auth_negotiation_has_required_method_ = has_required_method;
        auth_negotiation_required_method_ = required_method;
        auth_negotiation_transport_mask_ = allowed_transport_mask;
        auth_negotiation_nonce_ = std::move(challenge_nonce);
        auth_negotiation_method_registry_ = std::move(method_registry);
        return core::Status::OK;
    }

    core::Status doCompleteMfaChallenge(protocol::AuthMethod method,
                                        const MfaChallengeData& challenge,
                                        core::ErrorContext* ctx) {
        if (config_.mfa_code.empty()) {
            last_error_ = "Server requires MFA but connection config has no mfa_code";
            return core::Status::INVALID_AUTHORIZATION;
        }

        MfaChallengeData current = challenge;
        constexpr uint32_t kMaxMfaRoundTrips = 3;
        for (uint32_t attempt = 0; attempt < kMaxMfaRoundTrips; ++attempt) {
            std::vector<uint8_t> payload = buildMfaResponsePayload(
                current.challenge_id, config_.mfa_code);

            Connection::AuthResponse auth_response;
            core::Status status = doSendAuthRequest(method, payload, auth_response, ctx);
            if (status != core::Status::OK) {
                return status;
            }

            if (auth_response.status == protocol::AuthStatus::OK) {
                return core::Status::OK;
            }
            if (auth_response.status == protocol::AuthStatus::CONTINUE) {
                MfaChallengeData retry;
                std::string challenge_error;
                if (!parseMfaChallengePayload(auth_response.data, retry, challenge_error)) {
                    last_error_ = challenge_error.empty()
                        ? "Invalid MFA retry challenge payload"
                        : challenge_error;
                    return core::Status::PROTOCOL_VIOLATION;
                }
                current = std::move(retry);
                continue;
            }

            last_error_ = auth_response.error_message.empty()
                ? "MFA authentication failed"
                : auth_response.error_message;
            return core::Status::INVALID_PASSWORD;
        }

        last_error_ = "MFA authentication attempts exhausted";
        return core::Status::INVALID_PASSWORD;
    }

    core::Status doAuthenticateWithPassword(core::ErrorContext* ctx) {
        std::vector<uint8_t> payload(config_.password.begin(), config_.password.end());
        Connection::AuthResponse auth_response;
        core::Status status = doSendAuthRequest(
            protocol::AuthMethod::PASSWORD, payload, auth_response, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        if (auth_response.status == protocol::AuthStatus::OK) {
            return core::Status::OK;
        }
        if (auth_response.status == protocol::AuthStatus::CONTINUE) {
            MfaChallengeData challenge;
            std::string challenge_error;
            if (!parseMfaChallengePayload(auth_response.data, challenge, challenge_error)) {
                last_error_ = challenge_error.empty()
                    ? "Unexpected multi-step password authentication response"
                    : challenge_error;
                return core::Status::PROTOCOL_VIOLATION;
            }
            return doCompleteMfaChallenge(protocol::AuthMethod::PASSWORD, challenge, ctx);
        }
        last_error_ = auth_response.error_message.empty()
            ? "Authentication failed"
            : auth_response.error_message;
        return core::Status::INVALID_PASSWORD;
    }

    core::Status doAuthenticateWithToken(core::ErrorContext* ctx) {
        if (!hasConfiguredAuthToken(config_)) {
            last_error_ = "TOKEN authentication requires auth_token_authkey_id (16 bytes) and auth_token_secret";
            return core::Status::INVALID_ARGUMENT;
        }

        uint8_t authkey_id[16];
        std::memcpy(authkey_id, config_.auth_token_authkey_id.data(), sizeof(authkey_id));

        std::vector<uint8_t> proof;
        if (!computeAuthKeyTokenProof(config_.auth_token_secret,
                                      config_.username,
                                      authkey_id,
                                      config_.auth_token_binding,
                                      proof)) {
            last_error_ = "Failed to build TOKEN authentication proof";
            return core::Status::INTERNAL_ERROR;
        }

        std::vector<uint8_t> payload = protocol::ProtocolCodec::buildTokenAuthPayload(
            authkey_id,
            proof,
            config_.auth_token_binding);

        Connection::AuthResponse auth_response;
        core::Status status = doSendAuthRequest(
            protocol::AuthMethod::TOKEN, payload, auth_response, ctx);
        if (status != core::Status::OK) {
            return status;
        }

        if (auth_response.status == protocol::AuthStatus::OK) {
            return core::Status::OK;
        }
        if (auth_response.status == protocol::AuthStatus::CONTINUE) {
            MfaChallengeData challenge;
            std::string challenge_error;
            if (!parseMfaChallengePayload(auth_response.data, challenge, challenge_error)) {
                last_error_ = challenge_error.empty()
                    ? "Unexpected multi-step TOKEN authentication response"
                    : challenge_error;
                return core::Status::PROTOCOL_VIOLATION;
            }
            return doCompleteMfaChallenge(protocol::AuthMethod::TOKEN, challenge, ctx);
        }
        last_error_ = auth_response.error_message.empty()
            ? "TOKEN authentication failed"
            : auth_response.error_message;
        return core::Status::INVALID_PASSWORD;
    }

    core::Status doAuthenticateWithPeer(core::ErrorContext* ctx) {
        std::vector<uint8_t> payload;
        Connection::AuthResponse auth_response;
        core::Status status = doSendAuthRequest(
            protocol::AuthMethod::PEER, payload, auth_response, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        if (auth_response.status == protocol::AuthStatus::OK) {
            return core::Status::OK;
        }
        if (auth_response.status == protocol::AuthStatus::CONTINUE) {
            MfaChallengeData challenge;
            std::string challenge_error;
            if (!parseMfaChallengePayload(auth_response.data, challenge, challenge_error)) {
                last_error_ = challenge_error.empty()
                    ? "Unexpected multi-step PEER authentication response"
                    : challenge_error;
                return core::Status::PROTOCOL_VIOLATION;
            }
            return doCompleteMfaChallenge(protocol::AuthMethod::PEER, challenge, ctx);
        }
        last_error_ = auth_response.error_message.empty()
            ? "PEER authentication failed"
            : auth_response.error_message;
        return core::Status::INVALID_PASSWORD;
    }

    core::Status doAuthenticateWithScram(protocol::AuthMethod method,
                                         core::ErrorContext* ctx) {
        const security::ScramAlgorithm algorithm =
            (method == protocol::AuthMethod::SCRAM_SHA_512)
                ? security::ScramAlgorithm::SHA_512
                : security::ScramAlgorithm::SHA_256;

        const std::string client_nonce = security::generateNonce();
        const std::string client_first_bare =
            "n=" + security::normalizeUsername(config_.username) + ",r=" + client_nonce;
        const std::string client_first = "n,," + client_first_bare;

        std::vector<uint8_t> payload(client_first.begin(), client_first.end());
        Connection::AuthResponse auth_response;
        core::Status status = doSendAuthRequest(method, payload, auth_response, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        if (auth_response.status != protocol::AuthStatus::CONTINUE) {
            last_error_ = auth_response.error_message.empty()
                ? "SCRAM authentication failed"
                : auth_response.error_message;
            return core::Status::INVALID_PASSWORD;
        }

        const std::string server_first(auth_response.data.begin(), auth_response.data.end());
        ScramServerFirstData parsed_server_first;
        std::string scram_error;
        if (!parseScramServerFirstMessage(server_first,
                                          client_nonce,
                                          parsed_server_first,
                                          scram_error)) {
            last_error_ = scram_error.empty()
                ? "Invalid SCRAM server-first message"
                : scram_error;
            return core::Status::INVALID_AUTHORIZATION;
        }

        std::string client_final;
        std::string expected_server_verifier;
        if (!buildScramClientFinalMessage(config_.password,
                                          algorithm,
                                          client_first_bare,
                                          server_first,
                                          parsed_server_first,
                                          client_final,
                                          expected_server_verifier,
                                          scram_error)) {
            last_error_ = scram_error.empty()
                ? "Failed to construct SCRAM client-final message"
                : scram_error;
            return core::Status::INTERNAL_ERROR;
        }

        payload.assign(client_final.begin(), client_final.end());
        status = doSendAuthRequest(method, payload, auth_response, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        if (auth_response.status == protocol::AuthStatus::FAILURE) {
            last_error_ = auth_response.error_message.empty()
                ? "SCRAM authentication failed"
                : auth_response.error_message;
            return core::Status::INVALID_PASSWORD;
        }

        if (auth_response.status == protocol::AuthStatus::OK) {
            const std::string server_final(auth_response.data.begin(), auth_response.data.end());
            std::string server_verifier;
            if (!parseScramServerFinalMessage(server_final, server_verifier, scram_error)) {
                last_error_ = scram_error.empty()
                    ? "Invalid SCRAM server-final message"
                    : scram_error;
                return core::Status::INVALID_AUTHORIZATION;
            }
            if (!timingSafeEqual(server_verifier, expected_server_verifier)) {
                last_error_ = "SCRAM server signature verification failed";
                return core::Status::INVALID_AUTHORIZATION;
            }
            return core::Status::OK;
        }

        MfaChallengeData challenge;
        std::string challenge_error;
        if (!parseMfaChallengePayload(auth_response.data, challenge, challenge_error)) {
            last_error_ = challenge_error.empty()
                ? "Unexpected SCRAM continuation payload"
                : challenge_error;
            return core::Status::PROTOCOL_VIOLATION;
        }
        if (!challenge.has_server_final) {
            last_error_ = "SCRAM+MFA continuation missing server-final signature";
            return core::Status::PROTOCOL_VIOLATION;
        }

        std::string server_verifier;
        if (!parseScramServerFinalMessage(challenge.server_final, server_verifier, scram_error)) {
            last_error_ = scram_error.empty()
                ? "Invalid SCRAM server-final message in MFA continuation"
                : scram_error;
            return core::Status::INVALID_AUTHORIZATION;
        }
        if (!timingSafeEqual(server_verifier, expected_server_verifier)) {
            last_error_ = "SCRAM server signature verification failed";
            return core::Status::INVALID_AUTHORIZATION;
        }

        return doCompleteMfaChallenge(method, challenge, ctx);
    }

    core::Status doAuthenticate(core::ErrorContext* ctx) {
        auto status = ensureAuthNegotiation(config_.username, ctx);
        if (status != core::Status::OK) {
            return status;
        }

        protocol::AuthMethod selected_method = protocol::AuthMethod::SCRAM_SHA_256;
        std::string selection_error;
        if (!selectNegotiatedAuthMethod(auth_negotiation_allowed_methods_,
                                        auth_negotiation_has_required_method_,
                                        auth_negotiation_required_method_,
                                        preferredMethodsForConfig(config_),
                                        selected_method,
                                        selection_error)) {
            last_error_ = selection_error.empty()
                ? "No compatible authentication method negotiated"
                : selection_error;
            return core::Status::NOT_SUPPORTED;
        }

        if (authDebugEnabled()) {
            std::fprintf(stderr,
                         "[auth_debug_client] selected auth method=%u user=%s\n",
                         static_cast<unsigned>(selected_method),
                         config_.username.c_str());
        }

        if (selected_method == protocol::AuthMethod::TOKEN) {
            return doAuthenticateWithToken(ctx);
        }
        if (selected_method == protocol::AuthMethod::PEER) {
            return doAuthenticateWithPeer(ctx);
        }
        if (selected_method == protocol::AuthMethod::PASSWORD) {
            return doAuthenticateWithPassword(ctx);
        }
        if (selected_method == protocol::AuthMethod::SCRAM_SHA_256 ||
            selected_method == protocol::AuthMethod::SCRAM_SHA_512) {
            return doAuthenticateWithScram(selected_method, ctx);
        }

        last_error_ = "Negotiated authentication method is not supported by the client";
        return core::Status::NOT_SUPPORTED;
    }

    core::Status doSendAuthRequest(protocol::AuthMethod method,
                                   const std::vector<uint8_t>& payload,
                                   Connection::AuthResponse& response,
                                   core::ErrorContext* ctx) {
        if (!protocol_session_) {
            last_error_ = "Not connected";
            return core::Status::CONNECTION_FAILURE;
        }

        auto status = ensureAuthNegotiation(config_.username, ctx);
        if (status != core::Status::OK) {
            return status;
        }

        std::string negotiation_error;
        if (!validateNegotiatedAuthMethod(method, negotiation_error)) {
            last_error_ = negotiation_error.empty()
                ? "Requested authentication method denied by policy"
                : negotiation_error;
            return core::Status::INVALID_AUTHORIZATION;
        }

        std::vector<uint8_t> outbound_payload = payload;
        const bool plugin_registry_enabled =
            (config_.connect_client_flags & protocol::FEATURE_AUTH_PLUGIN_REGISTRY) != 0 &&
            !auth_negotiation_method_registry_.empty();
        if (plugin_registry_enabled) {
            uint16_t selected_slot = 0;
            if (!resolveAuthMethodSlot(auth_negotiation_method_registry_, method, selected_slot)) {
                last_error_ = "AUTH_METHOD_NOT_ALLOWED: missing method slot mapping";
                return core::Status::INVALID_AUTHORIZATION;
            }
            outbound_payload = buildAuthRegistrySelectionPayload(selected_slot, payload);
        }

        auto auth_msg = protocol::ProtocolCodec::buildAuthRequest(
            session_id_,
            config_.username,
            method,
            outbound_payload
        );

        status = protocol_session_->sendMessage(auth_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send auth request";
            return status;
        }

        protocol::Message response_msg;
        status = protocol_session_->receiveMessage(response_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive auth response";
            return status;
        }

        if (response_msg.getType() != protocol::MessageType::AUTH_RESPONSE) {
            last_error_ = describeUnexpectedResponse(response_msg);
            return core::Status::PROTOCOL_VIOLATION;
        }

        protocol::AuthStatus auth_status = protocol::AuthStatus::FAILURE;
        uint32_t user_id = 0;
        std::string error_msg;
        std::vector<uint8_t> data;
        status = protocol::ProtocolCodec::parseAuthResponse(
            response_msg, auth_status, user_id, error_msg, &data, ctx
        );
        if (!isOk(status)) {
            last_error_ = "Failed to parse auth response";
            return status;
        }

        response.status = auth_status;
        response.user_id = user_id;
        response.error_message = error_msg;
        response.data = std::move(data);

        if (auth_status == protocol::AuthStatus::OK) {
            clearAuthNegotiationState();
        } else if (auth_status == protocol::AuthStatus::FAILURE &&
                   error_msg == "AUTH_POLICY_NEGOTIATION_REQUIRED") {
            clearAuthNegotiationState();
        }
        return core::Status::OK;
    }

    core::Status doExecuteQueryMessage(const protocol::Message& query_msg,
                                       ResultSet* results,
                                       core::ErrorContext* ctx) {
        if (results) {
            results->impl_->clear();
        }
        if (ctx)
        {
            ctx->code = core::Status::OK;
            ctx->sqlstate = core::SQLSTATE_SUCCESS;
            ctx->message.clear();
        }

        auto status = protocol_session_->sendMessage(query_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send query";
            std::fprintf(stderr,
                         "[ipc_debug] client send query failed status=%d msg=%s\n",
                         static_cast<int>(status),
                         ctx && !ctx->message.empty() ? ctx->message.c_str() : "none");
            return status;
        }

        const uint32_t copy_window = config_.copy_window_bytes == 0 ? 65536 : config_.copy_window_bytes;
        const uint32_t copy_chunk = config_.copy_chunk_bytes == 0 ? 16384 : config_.copy_chunk_bytes;

        auto handle_copy_out = [&]() -> core::Status {
            std::ostream* out = copy_output_stream_ ? copy_output_stream_ : &std::cout;
            uint32_t window = 0;
            bool stream_ready = false;

            while (true) {
                protocol::Message response;
                auto status = protocol_session_->receiveMessage(response, ctx);
                if (!isOk(status)) {
                    last_error_ = "Failed to receive COPY OUT response";
                    return status;
                }

                switch (response.getType()) {
                    case protocol::MessageType::STREAM_READY: {
                        stream_ready = true;
                        window = copy_window;
                        auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                            protocol::StreamControlType::START, window, 0);
                        status = protocol_session_->sendMessage(ctrl, ctx);
                        if (!isOk(status)) {
                            last_error_ = "Failed to send STREAM_CONTROL";
                            return status;
                        }
                        break;
                    }
                    case protocol::MessageType::COPY_DATA: {
                        const uint8_t* data = nullptr;
                        size_t len = 0;
                        protocol::ProtocolCodec::parseCopyData(response, &data, &len, ctx);
                        if (len > 0) {
                            out->write(reinterpret_cast<const char*>(data),
                                       static_cast<std::streamsize>(len));
                            if (!(*out)) {
                                last_error_ = "COPY OUT write failed";
                                return core::Status::IO_ERROR;
                            }
                        }
                        if (window > 0) {
                            if (len >= window) {
                                window = 0;
                            } else {
                                window -= static_cast<uint32_t>(len);
                            }
                        }
                        if (stream_ready && window == 0) {
                            window = copy_window;
                            auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                                protocol::StreamControlType::ACK, window, 0);
                            status = protocol_session_->sendMessage(ctrl, ctx);
                            if (!isOk(status)) {
                                last_error_ = "Failed to send STREAM_CONTROL ACK";
                                return status;
                            }
                        }
                        break;
                    }
                    case protocol::MessageType::COPY_DONE:
                        return core::Status::OK;

                    case protocol::MessageType::COPY_FAIL: {
                        std::string message;
                        protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                        last_error_ = message.empty() ? "COPY OUT failed" : message;
                        return core::Status::INTERNAL_ERROR;
                    }
                    case protocol::MessageType::QUERY_ERROR: {
                        uint32_t error_code;
                        std::string sqlstate, message, detail, hint;
                        protocol::ProtocolCodec::parseQueryError(
                            response, error_code, sqlstate, message, detail, hint, ctx
                        );
                        if (ctx)
                        {
                            ctx->set(static_cast<core::Status>(error_code),
                                     message.c_str(), __FILE__, __LINE__, __func__);
                        }
                        last_error_ = message;
                        if (!detail.empty()) last_error_ += " (" + detail + ")";
                        return static_cast<core::Status>(error_code);
                    }
                    case protocol::MessageType::STREAM_END:
                        break;

                    default:
                        break;
                }
            }
        };

        auto handle_copy_in = [&]() -> core::Status {
            std::istream* in = copy_input_stream_ ? copy_input_stream_ : &std::cin;
            uint32_t window = 0;
            bool done = false;
            bool stream_started = false;

            while (!done) {
                if (window == 0) {
                    protocol::Message response;
                    auto status = protocol_session_->receiveMessage(response, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to receive COPY IN control";
                        return status;
                    }

                    switch (response.getType()) {
                        case protocol::MessageType::STREAM_READY:
                            if (!stream_started) {
                                window = copy_window;
                                auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                                    protocol::StreamControlType::START, window, 0);
                                status = protocol_session_->sendMessage(ctrl, ctx);
                                if (!isOk(status)) {
                                    last_error_ = "Failed to send STREAM_CONTROL START";
                                    return status;
                                }
                                stream_started = true;
                            }
                            break;
                        case protocol::MessageType::STREAM_CONTROL: {
                            protocol::StreamControlType control;
                            uint32_t new_window = 0;
                            uint32_t timeout_ms = 0;
                            status = protocol::ProtocolCodec::parseStreamControl(
                                response, control, new_window, timeout_ms, ctx);
                            if (!isOk(status)) {
                                last_error_ = "Malformed STREAM_CONTROL";
                                return status;
                            }
                            (void)timeout_ms;
                            if (control == protocol::StreamControlType::PAUSE) {
                                break;
                            }
                            if (control == protocol::StreamControlType::CANCEL) {
                                last_error_ = "COPY IN canceled by server";
                                return core::Status::CANCELLED;
                            }
                            window += new_window;
                            break;
                        }
                        case protocol::MessageType::COPY_FAIL: {
                            std::string message;
                            protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                            last_error_ = message.empty() ? "COPY IN failed" : message;
                            return core::Status::INTERNAL_ERROR;
                        }
                        case protocol::MessageType::QUERY_ERROR: {
                            uint32_t error_code;
                            std::string sqlstate, message, detail, hint;
                            protocol::ProtocolCodec::parseQueryError(
                                response, error_code, sqlstate, message, detail, hint, ctx
                            );
                            if (ctx)
                            {
                                ctx->set(static_cast<core::Status>(error_code),
                                         message.c_str(), __FILE__, __LINE__, __func__);
                            }
                            last_error_ = message;
                            if (!detail.empty()) last_error_ += " (" + detail + ")";
                            return static_cast<core::Status>(error_code);
                        }
                        default:
                            break;
                    }
                    if (window == 0) {
                        continue;
                    }
                }

                size_t to_read = std::min<size_t>(window, copy_chunk);
                std::string buffer(to_read, '\0');
                in->read(buffer.data(), static_cast<std::streamsize>(to_read));
                std::streamsize got = in->gcount();

                if (got > 0) {
                    auto msg = protocol::ProtocolCodec::buildCopyData(
                        reinterpret_cast<const uint8_t*>(buffer.data()),
                        static_cast<size_t>(got));
                    auto status = protocol_session_->sendMessage(msg, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to send COPY_DATA";
                        return status;
                    }
                    if (got >= static_cast<std::streamsize>(window)) {
                        window = 0;
                    } else {
                        window -= static_cast<uint32_t>(got);
                    }
                } else {
                    auto msg = protocol::ProtocolCodec::buildCopyDone();
                    auto status = protocol_session_->sendMessage(msg, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to send COPY_DONE";
                        return status;
                    }
                    done = true;
                }
            }
            return core::Status::OK;
        };

        struct StreamBuffer {
            std::vector<uint8_t> data;
            uint64_t expected_bytes = 0;
            bool complete = false;
        };
        std::unordered_map<uint64_t, StreamBuffer> stream_buffers;
        std::unordered_map<uint64_t, std::vector<std::pair<size_t, size_t>>> stream_bindings;
        uint32_t stream_window = 0;

        auto apply_stream_data = [&](uint64_t stream_id) {
            if (!results) {
                return;
            }
            auto buffer_it = stream_buffers.find(stream_id);
            if (buffer_it == stream_buffers.end() || !buffer_it->second.complete) {
                return;
            }
            auto binding_it = stream_bindings.find(stream_id);
            if (binding_it == stream_bindings.end()) {
                return;
            }
            const auto& data = buffer_it->second.data;
            for (const auto& binding : binding_it->second) {
                if (binding.first >= results->impl_->rows_.size()) {
                    continue;
                }
                auto& row = results->impl_->rows_[binding.first];
                if (binding.second >= row.size()) {
                    continue;
                }
                row[binding.second].data = data;
                row[binding.second].is_stream = false;
            }
            stream_bindings.erase(binding_it);
            stream_buffers.erase(buffer_it);
        };

        while (true) {
            protocol::Message response;
            status = protocol_session_->receiveMessage(response, ctx);
            if (!isOk(status)) {
                last_error_ = "Failed to receive response";
                std::fprintf(stderr,
                             "[ipc_debug] client receive response failed status=%d msg=%s\n",
                             static_cast<int>(status),
                             ctx && !ctx->message.empty() ? ctx->message.c_str() : "none");
                return status;
            }

            if (myExecDebugEnabled()) {
                std::fprintf(stderr,
                             "[my_exec] client received type=%s (%u)\n",
                             messageTypeName(response.getType()),
                             static_cast<unsigned>(response.getType()));
                std::fflush(stderr);
            }

            switch (response.getType()) {
                case protocol::MessageType::QUERY_ERROR: {
                    uint32_t error_code;
                    std::string sqlstate, message, detail, hint;
                    protocol::ProtocolCodec::parseQueryError(
                        response, error_code, sqlstate, message, detail, hint, ctx
                    );
                    last_error_ = message;
                    if (!detail.empty()) last_error_ += " (" + detail + ")";
                    return static_cast<core::Status>(error_code);
                }

                case protocol::MessageType::ROW_DESCRIPTION: {
                    if (results) {
                        std::vector<protocol::ProtocolCodec::ColumnInfo> cols;
                        protocol::ProtocolCodec::parseRowDescription(response, cols, ctx);
                        results->impl_->columns_.clear();
                        for (size_t i = 0; i < cols.size(); ++i) {
                            ColumnMeta meta;
                            meta.name = cols[i].name;
                            meta.type = cols[i].type;
                            meta.type_modifier = cols[i].type_modifier;
                            meta.index = i;
                            results->impl_->columns_.push_back(meta);
                        }
                    }
                    break;
                }

                case protocol::MessageType::ROW_DATA: {
                    if (results) {
                        std::vector<protocol::ProtocolCodec::ColumnValue> values;
                        protocol::ProtocolCodec::parseRowData(response, values, ctx);
                        const size_t row_index = results->impl_->rows_.size();
                        for (size_t i = 0; i < values.size(); ++i) {
                            if (!values[i].is_stream) {
                                continue;
                            }
                            stream_bindings[values[i].stream_id].push_back({row_index, i});
                            auto buffer_it = stream_buffers.find(values[i].stream_id);
                            if (buffer_it != stream_buffers.end() && buffer_it->second.complete) {
                                values[i].data = buffer_it->second.data;
                                values[i].is_stream = false;
                            }
                        }
                        results->impl_->rows_.push_back(std::move(values));
                    }
                    break;
                }

                case protocol::MessageType::COMMAND_COMPLETE: {
                    if (results) {
                        std::string tag;
                        int64_t rows_affected;
                        protocol::ProtocolCodec::parseCommandComplete(
                            response, tag, rows_affected, ctx
                        );
                        results->impl_->command_tag_ = tag;
                        results->impl_->rows_affected_ = rows_affected;
                    }
                    break;
                }

                case protocol::MessageType::END_OF_RESULTS: {
                    if (results) {
                        results->impl_->row_count_ = static_cast<int64_t>(results->impl_->rows_.size());
                    }
                    if (auto_commit_) {
                        in_transaction_ = false;
                        state_ = ConnectionState::CONNECTED;
                    } else {
                        in_transaction_ = true;
                        state_ = ConnectionState::IN_TRANSACTION;
                    }
                    return core::Status::OK;
                }
                case protocol::MessageType::PORTAL_SUSPENDED: {
                    if (results) {
                        results->impl_->row_count_ = static_cast<int64_t>(results->impl_->rows_.size());
                    }
                    if (auto_commit_) {
                        in_transaction_ = false;
                        state_ = ConnectionState::CONNECTED;
                    } else {
                        in_transaction_ = true;
                        state_ = ConnectionState::IN_TRANSACTION;
                    }
                    return core::Status::OK;
                }

                case protocol::MessageType::COPY_IN_RESPONSE: {
                    status = handle_copy_in();
                    if (!isOk(status)) {
                        return status;
                    }
                    break;
                }
                case protocol::MessageType::COPY_OUT_RESPONSE: {
                    status = handle_copy_out();
                    if (!isOk(status)) {
                        return status;
                    }
                    break;
                }
                case protocol::MessageType::COPY_FAIL: {
                    std::string message;
                    protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                    last_error_ = message.empty() ? "COPY failed" : message;
                    return core::Status::INTERNAL_ERROR;
                }
                case protocol::MessageType::STREAM_READY: {
                    uint64_t stream_id = 0;
                    uint64_t total_rows = 0;
                    uint64_t estimated_bytes = 0;
                    protocol::ProtocolCodec::parseStreamReady(response, stream_id,
                                                              total_rows, estimated_bytes, ctx);
                    (void)total_rows;
                    stream_window = copy_window;
                    auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                        protocol::StreamControlType::START, stream_window, 0);
                    status = protocol_session_->sendMessage(ctrl, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to send STREAM_CONTROL";
                        return status;
                    }
                    if (results) {
                        auto& buffer = stream_buffers[stream_id];
                        buffer.expected_bytes = estimated_bytes;
                    }
                    break;
                }
                case protocol::MessageType::STREAM_DATA: {
                    uint64_t stream_id = 0;
                    uint32_t chunk_rows = 0;
                    const uint8_t* data = nullptr;
                    size_t len = 0;
                    protocol::ProtocolCodec::parseStreamData(
                        response, stream_id, chunk_rows, &data, &len, ctx);
                    (void)chunk_rows;
                    if (results && data && len > 0) {
                        auto& buffer = stream_buffers[stream_id];
                        buffer.data.insert(buffer.data.end(), data, data + len);
                    }
                    if (stream_window > 0) {
                        if (len >= stream_window) {
                            stream_window = 0;
                        } else {
                            stream_window -= static_cast<uint32_t>(len);
                        }
                    }
                    if (stream_window == 0) {
                        stream_window = copy_window;
                        auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                            protocol::StreamControlType::ACK, stream_window, 0);
                        status = protocol_session_->sendMessage(ctrl, ctx);
                        if (!isOk(status)) {
                            last_error_ = "Failed to send STREAM_CONTROL ACK";
                            return status;
                        }
                    }
                    break;
                }
                case protocol::MessageType::STREAM_END: {
                    uint64_t stream_id = 0;
                    uint64_t total_rows = 0;
                    uint64_t total_bytes = 0;
                    protocol::ProtocolCodec::parseStreamEnd(response, stream_id,
                                                            total_rows, total_bytes, ctx);
                    (void)total_rows;
                    (void)total_bytes;
                    if (results) {
                        auto& buffer = stream_buffers[stream_id];
                        buffer.complete = true;
                        apply_stream_data(stream_id);
                    }
                    stream_window = 0;
                    break;
                }
                case protocol::MessageType::QUERY_PROGRESS: {
                    uint64_t rows = 0;
                    uint64_t bytes = 0;
                    protocol::ProtocolCodec::parseQueryProgress(response, rows, bytes, ctx);
                    if (progress_callback_) {
                        progress_callback_(rows, bytes);
                    }
                    break;
                }

                case protocol::MessageType::TRANSACTION_STATUS: {
                    // NET-L1: Parse transaction status and update connection state
                    if (response.getPayloadSize() >= sizeof(protocol::TransactionStatusPayload)) {
                        const auto* ts_payload = reinterpret_cast<const protocol::TransactionStatusPayload*>(
                            response.getPayload());
                        // Status: 0 = idle, 1 = in transaction, 2 = failed
                        in_transaction_ = (ts_payload->status == 1);
                        if (ts_payload->status == 2) {
                            // Transaction failed - reset to idle
                            state_ = ConnectionState::CONNECTED;
                        } else if (ts_payload->status == 1) {
                            state_ = ConnectionState::IN_TRANSACTION;
                        } else {
                            state_ = ConnectionState::CONNECTED;
                        }
                    }
                    break;
                }
                case protocol::MessageType::NOTIFICATION: {
                    uint32_t process_id = 0;
                    std::string channel;
                    std::vector<uint8_t> payload;
                    uint8_t change_type = 0;
                    uint64_t row_id = 0;
                    auto parse_status = protocol::ProtocolCodec::parseNotification(
                        response,
                        process_id,
                        channel,
                        payload,
                        change_type,
                        row_id,
                        ctx);
                    if (!isOk(parse_status)) {
                        last_error_ = "Malformed NOTIFICATION payload";
                        return parse_status;
                    }
                    (void)process_id;
                    (void)change_type;
                    (void)row_id;

                    if (results && channel == kServerNoticeChannel) {
                        results->impl_->notices_.emplace_back(payload.begin(), payload.end());
                    }
                    break;
                }

                default:
                    // Ignore unexpected messages
                    break;
            }
        }
    }

    core::Status doExecuteQuery(const std::string& sql, uint8_t flags, ResultSet* results,
                                core::ErrorContext* ctx) {
        parser::v3::Compiler compiler;
        auto compile_result = compiler.compile(sql);
        if (!compile_result.ok) {
            last_error_ = compile_result.error.empty()
                ? "Compilation failed before submit"
                : compile_result.error;
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT, last_error_.c_str(),
                         __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        auto query_msg = protocol::ProtocolCodec::buildQueryBytecode(
            session_id_, compile_result.bytecode, sql, flags);
        return doExecuteQueryMessage(query_msg, results, ctx);
    }

    core::Status doExecuteBytecode(const std::vector<uint8_t>& bytecode,
                                   const std::string& sql,
                                   ResultSet* results,
                                   core::ErrorContext* ctx) {
        auto query_msg = protocol::ProtocolCodec::buildQueryBytecode(session_id_, bytecode, sql, 0);
        return doExecuteQueryMessage(query_msg, results, ctx);
    }

    core::Status doBeginTransaction(core::ErrorContext* ctx) {
        auto msg = protocol::ProtocolCodec::buildBeginTransaction(session_id_, 0, false);
        auto status = protocol_session_->sendMessage(msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send BEGIN";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive response";
            return status;
        }

        if (response.getType() == protocol::MessageType::QUERY_ERROR) {
            uint32_t code;
            std::string sqlstate, message, detail, hint;
            protocol::ProtocolCodec::parseQueryError(
                response, code, sqlstate, message, detail, hint, ctx
            );
            last_error_ = message;
            return static_cast<core::Status>(code);
        }

        in_transaction_ = true;
        state_ = ConnectionState::IN_TRANSACTION;
        return core::Status::OK;
    }

    core::Status doCommit(core::ErrorContext* ctx) {
        auto msg = protocol::ProtocolCodec::buildCommit(session_id_);
        auto status = protocol_session_->sendMessage(msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send COMMIT";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive response";
            return status;
        }

        if (response.getType() == protocol::MessageType::QUERY_ERROR) {
            uint32_t code;
            std::string sqlstate, message, detail, hint;
            protocol::ProtocolCodec::parseQueryError(
                response, code, sqlstate, message, detail, hint, ctx
            );
            last_error_ = message;
            return static_cast<core::Status>(code);
        }

        // ScratchBird keeps a new transaction active after COMMIT.
        in_transaction_ = true;
        state_ = ConnectionState::IN_TRANSACTION;
        return core::Status::OK;
    }

    core::Status doRollback(core::ErrorContext* ctx) {
        auto msg = protocol::ProtocolCodec::buildRollback(session_id_);
        auto status = protocol_session_->sendMessage(msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send ROLLBACK";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive response";
            return status;
        }

        if (response.getType() == protocol::MessageType::QUERY_ERROR) {
            uint32_t code;
            std::string sqlstate, message, detail, hint;
            protocol::ProtocolCodec::parseQueryError(
                response, code, sqlstate, message, detail, hint, ctx
            );
            last_error_ = message;
            return static_cast<core::Status>(code);
        }

        // ScratchBird keeps a new transaction active after ROLLBACK.
        in_transaction_ = true;
        state_ = ConnectionState::IN_TRANSACTION;
        return core::Status::OK;
    }
};

// ============================================================================
// Connection Public API
// ============================================================================

Connection::Connection() : impl_(std::make_unique<ConnectionImpl>()) {}
Connection::~Connection() {
    if (impl_) {
        disconnect();
    }
}

Connection::Connection(Connection&& other) noexcept = default;
Connection& Connection::operator=(Connection&& other) noexcept = default;

core::Status Connection::connect(const std::string& database,
                                 const std::string& username,
                                 const std::string& password,
                                 core::ErrorContext* ctx) {
    ConnectionConfig config;
    config.database_name = database;
    config.username = username;
    config.password = password;
    return connect(config, ctx);
}

core::Status Connection::connect(const ConnectionConfig& config,
                                 core::ErrorContext* ctx) {
    if (isConnected()) {
        disconnect();
    }

    impl_->config_ = config;
    return impl_->doConnect(ctx);
}

void Connection::disconnect() {
    if (!isConnected()) return;

    // Rollback any active transaction
    if (impl_->in_transaction_) {
        rollback();
    }

    // Send disconnect message
    if (impl_->protocol_session_) {
        auto msg = protocol::ProtocolCodec::buildDisconnect();
        impl_->protocol_session_->sendMessage(msg);
    }

    // Close IPC client
    if (impl_->ipc_client_) {
        impl_->ipc_client_->disconnect();
    }

    impl_->protocol_session_.reset();
    impl_->ipc_client_.reset();
    impl_->state_ = ConnectionState::DISCONNECTED;
    impl_->in_transaction_ = false;
}

bool Connection::isConnected() const {
    if (!impl_) return false;
    return impl_->state_ == ConnectionState::CONNECTED ||
           impl_->state_ == ConnectionState::IN_TRANSACTION;
}

ConnectionState Connection::getState() const {
    if (!impl_) return ConnectionState::DISCONNECTED;
    return impl_->state_;
}

std::string Connection::getLastError() const {
    return impl_->last_error_;
}

core::Status Connection::ping(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    auto msg = protocol::ProtocolCodec::buildPing(timestamp, 0);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send ping";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive pong";
        return status;
    }

    if (response.getType() != protocol::MessageType::PONG) {
        impl_->last_error_ = "Unexpected response to ping";
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

core::Status Connection::cancelQuery(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildQueryCancel();
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send cancel request";
        return status;
    }

    return core::Status::OK;
}

core::Status Connection::requestStatus(protocol::StatusRequestType request_type,
                                       StatusResponse* out,
                                       core::ErrorContext* ctx) {
    if (!out) {
        return core::Status::INVALID_ARGUMENT;
    }
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildStatusRequest(request_type);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send status request";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive status response";
        return status;
    }

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t error_code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, error_code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(error_code);
    }

    if (response.getType() != protocol::MessageType::STATUS_RESPONSE) {
        impl_->last_error_ = "Unexpected response to status request";
        return core::Status::PROTOCOL_VIOLATION;
    }

    out->entries.clear();
    protocol::StatusRequestType parsed_type = request_type;
    std::vector<protocol::ProtocolCodec::StatusEntry> entries;
    status = protocol::ProtocolCodec::parseStatusResponse(response, parsed_type, entries, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to parse status response";
        return status;
    }

    out->request_type = parsed_type;
    out->entries.reserve(entries.size());
    for (const auto& entry : entries) {
        StatusEntry out_entry;
        out_entry.key = entry.key;
        out_entry.value = entry.value;
        out->entries.push_back(std::move(out_entry));
    }

    return core::Status::OK;
}

core::Status Connection::subscribe(const std::string& channel,
                                   const std::string& filter,
                                   uint8_t subscribe_type,
                                   core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }
    auto msg = protocol::ProtocolCodec::buildSubscribe(subscribe_type, channel, filter);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send subscribe request";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive subscribe response";
        return status;
    }
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t error_code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, error_code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(error_code);
    }
    return core::Status::OK;
}

core::Status Connection::unsubscribe(const std::string& channel,
                                     core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }
    auto msg = protocol::ProtocolCodec::buildUnsubscribe(channel);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send unsubscribe request";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive unsubscribe response";
        return status;
    }
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t error_code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, error_code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(error_code);
    }
    return core::Status::OK;
}

core::Status Connection::receiveNotification(Notification* out,
                                             core::ErrorContext* ctx) {
    if (!out) {
        return core::Status::INVALID_ARGUMENT;
    }
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    protocol::Message response;
    auto status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive notification";
        return status;
    }

    if (response.getType() != protocol::MessageType::NOTIFICATION) {
        impl_->last_error_ = "No notification";
        return core::Status::NOT_FOUND;
    }

    protocol::ProtocolCodec::parseNotification(response, out->processId, out->channel,
                                               out->payload, out->changeType,
                                               out->rowId, ctx);
    return core::Status::OK;
}

void Connection::setProgressCallback(std::function<void(uint64_t, uint64_t)> callback) {
    if (!impl_) {
        return;
    }
    impl_->progress_callback_ = std::move(callback);
}

void Connection::setCopyInputStream(std::istream* in) {
    if (!impl_) {
        return;
    }
    impl_->copy_input_stream_ = in;
}

void Connection::setCopyOutputStream(std::ostream* out) {
    if (!impl_) {
        return;
    }
    impl_->copy_output_stream_ = out;
}

core::Status Connection::sendAuthRequest(protocol::AuthMethod method,
                                         const std::vector<uint8_t>& payload,
                                         AuthResponse& response,
                                         core::ErrorContext* ctx) {
    if (!impl_) {
        return core::Status::INVALID_ARGUMENT;
    }

    if (impl_->state_ != ConnectionState::CONNECTED &&
        impl_->state_ != ConnectionState::CONNECTING) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    return impl_->doSendAuthRequest(method, payload, response, ctx);
}

core::Status Connection::executeQuery(const std::string& sql,
                                       ResultSet* results,
                                       core::ErrorContext* ctx) {
    return executeQuery(sql, results, 0, ctx);
}

core::Status Connection::executeQuery(const std::string& sql,
                                       ResultSet* results,
                                       uint8_t flags,
                                       core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto status = impl_->doExecuteQuery(sql, flags, results, ctx);
    if (!isOk(status) && ctx && ctx->message.empty() && !impl_->last_error_.empty())
    {
        ctx->set(status, impl_->last_error_.c_str(), __FILE__, __LINE__, __func__);
    }
    return status;
}

core::Status Connection::executeBytecode(const std::vector<uint8_t>& bytecode,
                                         const std::string& sql,
                                         ResultSet* results,
                                         core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto status = impl_->doExecuteBytecode(bytecode, sql, results, ctx);
    if (!isOk(status) && ctx && ctx->message.empty() && !impl_->last_error_.empty())
    {
        ctx->set(status, impl_->last_error_.c_str(), __FILE__, __LINE__, __func__);
    }
    return status;
}

core::Status Connection::executeBytecode(const std::vector<uint8_t>& bytecode,
                                         ResultSet* results,
                                         core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto status = impl_->doExecuteBytecode(bytecode, std::string(), results, ctx);
    if (!isOk(status) && ctx && ctx->message.empty() && !impl_->last_error_.empty())
    {
        ctx->set(status, impl_->last_error_.c_str(), __FILE__, __LINE__, __func__);
    }
    return status;
}

core::Status Connection::execute(const std::string& sql,
                                  int64_t* rows_affected,
                                  core::ErrorContext* ctx) {
    ResultSet results;
    auto status = executeQuery(sql, &results, ctx);
    if (isOk(status) && rows_affected) {
        *rows_affected = results.getRowsAffected();
    }
    return status;
}

core::Status Connection::prepare(const std::string& sql,
                                  PreparedStatement* stmt,
                                  core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    size_t param_count = countParameters(sql);

    stmt->impl_->sql_ = sql;
    stmt->impl_->param_count_ = param_count;
    stmt->impl_->params_.resize(param_count);
    stmt->impl_->param_types_.assign(param_count, protocol::WireType::UNKNOWN);
    stmt->impl_->valid_ = true;

    // Note: In a full implementation, we would send PREPARE to server
    // For now, we prepare client-side and substitute parameters

    return core::Status::OK;
}

core::Status Connection::executeQuery(PreparedStatement& stmt,
                                       ResultSet* results,
                                       core::ErrorContext* ctx) {
    if (!stmt.isValid()) {
        impl_->last_error_ = "Invalid prepared statement";
        return core::Status::INVALID_ARGUMENT;
    }

    // NET-1: Parameter substitution with proper escaping for SQL injection prevention
    // Substitute $1, $2, etc. with escaped parameter values
    std::string sql = substituteParameters(stmt.impl_->sql_, stmt.impl_->params_,
                                           stmt.impl_->param_types_);

    return executeQuery(sql, results, ctx);
}

core::Status Connection::execute(PreparedStatement& stmt,
                                  int64_t* rows_affected,
                                  core::ErrorContext* ctx) {
    ResultSet results;
    auto status = executeQuery(stmt, &results, ctx);
    if (isOk(status) && rows_affected) {
        *rows_affected = results.getRowsAffected();
    }
    return status;
}

void Connection::closeStatement(PreparedStatement& stmt) {
    stmt.impl_->valid_ = false;
    stmt.impl_->params_.clear();
}

core::Status Connection::beginTransaction(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    if (impl_->in_transaction_) {
        impl_->last_error_ = "Already in transaction";
        return core::Status::INVALID_TRANSACTION_STATE;
    }

    return impl_->doBeginTransaction(ctx);
}

core::Status Connection::commit(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    if (!impl_->in_transaction_) {
        impl_->last_error_ = "Not in transaction";
        return core::Status::INVALID_TRANSACTION_STATE;
    }

    return impl_->doCommit(ctx);
}

core::Status Connection::rollback(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    if (!impl_->in_transaction_) {
        // Silently succeed if not in transaction
        return core::Status::OK;
    }

    return impl_->doRollback(ctx);
}

core::Status Connection::detachToDormant(core::ID& dormant_id_out,
                                         core::ID& reattach_authkey_out,
                                         core::ErrorContext* ctx) {
    dormant_id_out = core::ID{};
    reattach_authkey_out = core::ID{};

    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildDormantDetach();
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send DORMANT_DETACH";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive DORMANT_DETACH_RESULT";
        return status;
    }

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        std::string message;
        std::string detail;
        std::string hint;
        uint32_t error_code = 0;
        std::string sqlstate;
        protocol::ProtocolCodec::parseQueryError(
            response, error_code, sqlstate, message, detail, hint, ctx);
        impl_->last_error_ = message.empty() ? "Dormant detach failed" : message;
        return static_cast<core::Status>(error_code);
    }

    if (response.getType() != protocol::MessageType::DORMANT_DETACH_RESULT) {
        impl_->last_error_ = describeUnexpectedResponse(response);
        return core::Status::PROTOCOL_VIOLATION;
    }

    std::array<uint8_t, 16> dormant_id_bytes{};
    std::array<uint8_t, 16> reattach_authkey_bytes{};
    status = protocol::ProtocolCodec::parseDormantDetachResult(
        response, dormant_id_bytes, reattach_authkey_bytes, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to parse DORMANT_DETACH_RESULT";
        return status;
    }

    std::copy(dormant_id_bytes.begin(), dormant_id_bytes.end(), dormant_id_out.bytes.begin());
    std::copy(reattach_authkey_bytes.begin(),
              reattach_authkey_bytes.end(),
              reattach_authkey_out.bytes.begin());
    impl_->in_transaction_ = false;
    impl_->state_ = ConnectionState::CONNECTED;
    return core::Status::OK;
}

core::Status Connection::savepoint(const std::string& name,
                                    core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildSavepoint(impl_->session_id_, name);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send SAVEPOINT";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive response";
        return status;
    }

    return core::Status::OK;
}

core::Status Connection::releaseSavepoint(const std::string& name,
                                           core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildReleaseSavepoint(impl_->session_id_, name);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send RELEASE SAVEPOINT";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive response";
        return status;
    }

    // NET-M2: Proper response validation
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(code);
    }

    return core::Status::OK;
}

core::Status Connection::rollbackTo(const std::string& name,
                                     core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildRollbackTo(impl_->session_id_, name);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send ROLLBACK TO";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive response";
        return status;
    }

    // NET-M3: Proper response validation
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(code);
    }

    return core::Status::OK;
}

bool Connection::inTransaction() const {
    return impl_->in_transaction_;
}

void Connection::setAutoCommit(bool enabled) {
    impl_->auto_commit_ = enabled;
}

bool Connection::getAutoCommit() const {
    return impl_->auto_commit_;
}

std::string Connection::getDatabaseName() const {
    return impl_->config_.database_name;
}

std::string Connection::getUsername() const {
    return impl_->config_.username;
}

std::string Connection::getServerVersion() const {
    return impl_->server_version_;
}

const ConnectionConfig& Connection::getConfig() const {
    return impl_->config_;
}

// ============================================================================
// Static Server Control
// ============================================================================

core::Status Connection::startServer(const std::string& database_name,
                                       const std::string& server_path,
                                       uint32_t timeout_ms,
                                       core::ErrorContext* ctx) {
    if (isServerRunning(database_name)) {
        return core::Status::OK;  // Already running
    }

    // Find sb_server executable
    std::string exe_path = server_path;
    if (exe_path.empty()) {
        // Search common locations
        const char* path_env = std::getenv("PATH");
        if (path_env) {
            std::string path_str = path_env;
            std::stringstream ss(path_str);
            std::string dir;
            while (std::getline(ss, dir, ':')) {
                std::string candidate = dir + "/sb_server";
                std::ifstream f(candidate);
                if (f.good()) {
                    exe_path = candidate;
                    break;
                }
            }
        }

        // Also check relative to current directory
        if (exe_path.empty()) {
            std::ifstream f("./sb_server");
            if (f.good()) exe_path = "./sb_server";
        }
        if (exe_path.empty()) {
            std::ifstream f("./build/bin/sb_server");
            if (f.good()) exe_path = "./build/bin/sb_server";
        }
    }

    if (exe_path.empty()) {
        if (ctx) ctx->message = "sb_server not found in PATH";
        return core::Status::NOT_FOUND;
    }

#ifdef _WIN32
    // Windows implementation
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    std::string cmdline = exe_path + " --database=" + database_name + " --daemon";

    if (!CreateProcess(NULL, const_cast<char*>(cmdline.c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        if (ctx) ctx->message = "Failed to start server process";
        return core::Status::INTERNAL_ERROR;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    // Unix implementation
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - redirect output to /dev/null
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) {
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }

        // Create new session (detach from parent)
        setsid();

        // Execute server
        execl(exe_path.c_str(), "sb_server",
              "--database", database_name.c_str(),
              "--daemon", nullptr);

        // execl failed
        _exit(1);
    } else if (pid < 0) {
        if (ctx) ctx->message = "fork() failed";
        return core::Status::INTERNAL_ERROR;
    }
#endif

    // Wait for server to start (poll PID file)
    // Note: If another client also tried to start a server for the same database,
    // our forked process will fail to acquire the database lock and exit.
    // We detect this by checking if a different server (different PID) started.
    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        if (isServerRunning(database_name)) {
            // Server is running - could be ours or another client's
            // Either way, we can connect to it
            return core::Status::OK;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();

        if (static_cast<uint32_t>(elapsed) >= timeout_ms) {
            // Our server didn't start - check if our child process is still running
            // If not, it might have failed due to database lock conflict
            if (ctx) ctx->message = "Server failed to start within timeout (database may be locked by another process)";
            return core::Status::LOCK_TIMEOUT;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

core::Status Connection::stopServer(const std::string& database_name,
                                     core::ErrorContext* ctx) {
    int32_t pid = getServerPID(database_name);
    if (pid <= 0) {
        return core::Status::OK;  // Not running
    }

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    }
#else
    kill(pid, SIGTERM);
#endif

    // Wait for server to stop
    for (int i = 0; i < 50; ++i) {
        if (!isServerRunning(database_name)) {
            return core::Status::OK;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Force kill
#ifndef _WIN32
    kill(pid, SIGKILL);
#endif

    return core::Status::OK;
}

bool Connection::isServerRunning(const std::string& database_name) {
    return server::isServerRunning(database_name);
}

int32_t Connection::getServerPID(const std::string& database_name) {
    std::string pid_path = server::getPIDFilePath(database_name);
    std::ifstream f(pid_path);
    if (!f.is_open()) return 0;

    int32_t pid = 0;
    f >> pid;
    return pid;
}

// ============================================================================
// PooledConnection Implementation
// ============================================================================

PooledConnection::PooledConnection(Connection* conn, std::function<void(Connection*)> returner)
    : connection_(conn), returner_(std::move(returner)) {}

PooledConnection::~PooledConnection() {
    if (connection_ && returner_) {
        returner_(connection_);
    }
}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : connection_(other.connection_), returner_(std::move(other.returner_)) {
    other.connection_ = nullptr;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this != &other) {
        if (connection_ && returner_) {
            returner_(connection_);
        }
        connection_ = other.connection_;
        returner_ = std::move(other.returner_);
        other.connection_ = nullptr;
    }
    return *this;
}

// ============================================================================
// ConnectionPool Implementation
// ============================================================================

class ConnectionPool::PoolImpl {
public:
    PoolConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::unique_ptr<Connection>> available_;
    std::atomic<uint32_t> in_use_{0};
    std::atomic<uint32_t> total_{0};
    std::atomic<uint64_t> acquisitions_{0};
    std::atomic<uint64_t> releases_{0};
    std::atomic<uint64_t> waits_{0};
    std::atomic<bool> shutdown_{false};

    void returnConnection(Connection* conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        --in_use_;
        ++releases_;

        if (!shutdown_ && conn->isConnected()) {
            available_.push(std::unique_ptr<Connection>(conn));
            cv_.notify_one();
        } else {
            delete conn;
            --total_;
        }
    }
};

ConnectionPool::ConnectionPool(const PoolConfig& config)
    : impl_(std::make_unique<PoolImpl>()) {
    impl_->config_ = config;

    // Pre-create minimum connections
    for (uint32_t i = 0; i < config.min_connections; ++i) {
        auto conn = std::make_unique<Connection>();
        if (isOk(conn->connect(config.connection_config))) {
            impl_->available_.push(std::move(conn));
            ++impl_->total_;
        }
    }
}

ConnectionPool::~ConnectionPool() {
    shutdown();
}

PooledConnection ConnectionPool::acquire(core::ErrorContext* ctx) {
    std::unique_lock<std::mutex> lock(impl_->mutex_);
    ++impl_->acquisitions_;

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(impl_->config_.acquire_timeout_ms);

    while (impl_->available_.empty()) {
        // Can we create a new connection?
        if (impl_->total_ < impl_->config_.max_connections) {
            ++impl_->total_;
            lock.unlock();

            auto conn = std::make_unique<Connection>();
            if (isOk(conn->connect(impl_->config_.connection_config, ctx))) {
                ++impl_->in_use_;
                return PooledConnection(conn.release(),
                    [this](Connection* c) { impl_->returnConnection(c); });
            }

            lock.lock();
            --impl_->total_;
        }

        // Wait for a connection to be returned
        ++impl_->waits_;
        if (impl_->cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            if (ctx) ctx->message = "Connection pool timeout";
            return PooledConnection(nullptr, nullptr);
        }
    }

    auto conn = std::move(impl_->available_.front());
    impl_->available_.pop();
    ++impl_->in_use_;

    // Validate connection if configured
    if (impl_->config_.validate_on_acquire) {
        lock.unlock();
        if (!isOk(conn->ping())) {
            // Connection is dead, try to reconnect
            conn->disconnect();
            if (!isOk(conn->connect(impl_->config_.connection_config, ctx))) {
                lock.lock();
                --impl_->in_use_;
                --impl_->total_;
                return acquire(ctx);  // Try again
            }
        }
    } else {
        lock.unlock();
    }

    return PooledConnection(conn.release(),
        [this](Connection* c) { impl_->returnConnection(c); });
}

ConnectionPool::Stats ConnectionPool::getStats() const {
    Stats stats;
    stats.total_connections = impl_->total_;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        stats.available_connections = static_cast<uint32_t>(impl_->available_.size());
    }
    stats.in_use_connections = impl_->in_use_;
    stats.total_acquisitions = impl_->acquisitions_;
    stats.total_releases = impl_->releases_;
    stats.wait_count = impl_->waits_;
    return stats;
}

void ConnectionPool::shutdown() {
    impl_->shutdown_ = true;

    std::lock_guard<std::mutex> lock(impl_->mutex_);
    while (!impl_->available_.empty()) {
        impl_->available_.pop();
        --impl_->total_;
    }
    impl_->cv_.notify_all();
}

} // namespace client
} // namespace scratchbird
