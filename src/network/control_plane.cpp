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
 * Control Plane Protocol Implementation
 */
// Section 32 invariant: this file implements a bounded internal control-plane
// seam for listener and parser or orchestration coordination. It is not a
// general public extension ABI and must stay distinct from wire-protocol truth.

#include "scratchbird/network/control_plane.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include "scratchbird/core/posix_compat.h"
#endif

namespace scratchbird::network {

namespace {

constexpr size_t CONTROL_PLANE_HEADER_SIZE = 28;
constexpr size_t CONTROL_PLANE_MAX_MESSAGE = 1024;
constexpr size_t DBBT_MIN_WIRE_BYTES = 1 + DBBT_UUID_BYTES + 4 + 8 + 8 + DBBT_SESSION_BYTES + 2 + 2 + 4 + 2;
constexpr size_t DBBT_MAX_WIRE_BYTES = 8192;

void appendU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void appendU64(std::vector<uint8_t>& out, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

uint16_t readU16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t readU32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0])
        | (static_cast<uint32_t>(data[1]) << 8)
        | (static_cast<uint32_t>(data[2]) << 16)
        | (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t readU64(const uint8_t* data) {
    uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | data[i];
    }
    return value;
}

void appendBytes(std::vector<uint8_t>& out, const uint8_t* bytes, size_t len) {
    if (bytes == nullptr || len == 0) {
        return;
    }
    out.insert(out.end(), bytes, bytes + len);
}

std::string trimAscii(std::string value) {
    auto is_space = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

char hexDigit(uint8_t value) {
    constexpr char kHex[] = "0123456789abcdef";
    return kHex[value & 0x0F];
}

bool timingSafeEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool computeHmacSha256(const std::vector<uint8_t>& key,
                       const uint8_t* data,
                       size_t data_len,
                       std::vector<uint8_t>& mac_out,
                       core::ErrorContext* ctx) {
    if (key.empty() || key.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Invalid DBBT key length");
        return false;
    }
    unsigned int mac_len = 0;
    std::vector<uint8_t> mac(EVP_MAX_MD_SIZE);
    if (!HMAC(EVP_sha256(),
              key.data(),
              static_cast<int>(key.size()),
              data,
              data_len,
              mac.data(),
              &mac_len)) {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "DBBT HMAC computation failed");
        return false;
    }
    mac.resize(mac_len);
    mac_out.swap(mac);
    return true;
}

bool encodeDatabaseBindingTokenBody(const DatabaseBindingToken& token,
                                    std::vector<uint8_t>& out,
                                    core::ErrorContext* ctx) {
    if (token.client_nonce.size() > DBBT_MAX_NONCE_BYTES ||
        token.server_nonce.size() > DBBT_MAX_NONCE_BYTES) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "DBBT nonce exceeds maximum size");
        return false;
    }
    if (token.client_nonce.size() > UINT16_MAX || token.server_nonce.size() > UINT16_MAX) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "DBBT nonce too large for wire format");
        return false;
    }

    out.clear();
    out.reserve(DBBT_MIN_WIRE_BYTES + token.client_nonce.size() + token.server_nonce.size());
    out.push_back(token.version);
    appendBytes(out, token.db_uuid.data(), token.db_uuid.size());
    appendU32(out, token.listener_id);
    appendU64(out, token.issued_at_ms);
    appendU64(out, token.expires_at_ms);
    appendBytes(out, token.manager_session_id.data(), token.manager_session_id.size());
    appendU16(out, static_cast<uint16_t>(token.client_nonce.size()));
    appendBytes(out, token.client_nonce.data(), token.client_nonce.size());
    appendU16(out, static_cast<uint16_t>(token.server_nonce.size()));
    appendBytes(out, token.server_nonce.data(), token.server_nonce.size());
    appendU32(out, token.flags);
    return true;
}

}  // namespace

uint64_t currentEpochMillis() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string bytesToHex(const std::vector<uint8_t>& bytes) {
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        hex.push_back(hexDigit(static_cast<uint8_t>(b >> 4)));
        hex.push_back(hexDigit(static_cast<uint8_t>(b & 0x0F)));
    }
    return hex;
}

bool hexToBytes(std::string_view hex, std::vector<uint8_t>& out) {
    std::string filtered;
    filtered.reserve(hex.size());
    for (char c : hex) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0 || c == ':') {
            continue;
        }
        filtered.push_back(c);
    }
    if (filtered.empty() || (filtered.size() % 2) != 0) {
        return false;
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(filtered.size() / 2);
    for (size_t i = 0; i < filtered.size(); i += 2) {
        int hi = hexNibble(filtered[i]);
        int lo = hexNibble(filtered[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    out.swap(bytes);
    return true;
}

bool encodeListenerPrefaceV1(const ListenerPrefaceV1& preface,
                             std::vector<uint8_t>& out,
                             core::ErrorContext* ctx) {
    if (preface.listener_id == 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "LPREFACE listener_id required");
        return false;
    }
    if (preface.dbbt.empty() || preface.dbbt.size() > LPREFACE_MAX_DBBT_BYTES) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "LPREFACE invalid DBBT payload");
        return false;
    }
    if (preface.db_selector.size() > LPREFACE_MAX_TEXT_BYTES ||
        preface.requested_profile.size() > LPREFACE_MAX_TEXT_BYTES) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "LPREFACE text field exceeds size");
        return false;
    }
    if (preface.db_selector.size() > UINT16_MAX ||
        preface.requested_profile.size() > UINT16_MAX ||
        preface.dbbt.size() > UINT32_MAX) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "LPREFACE field too large for wire format");
        return false;
    }

    out.clear();
    out.reserve(16 + preface.dbbt.size() + preface.db_selector.size() +
                preface.requested_profile.size());

    appendU32(out, LPREFACE_MAGIC);
    appendU16(out, LPREFACE_VERSION);
    appendU16(out, preface.reserved);
    appendU32(out, preface.listener_id);
    appendU32(out, static_cast<uint32_t>(preface.dbbt.size()));
    appendBytes(out, preface.dbbt.data(), preface.dbbt.size());
    appendU16(out, static_cast<uint16_t>(preface.db_selector.size()));
    appendBytes(out, reinterpret_cast<const uint8_t*>(preface.db_selector.data()),
                preface.db_selector.size());
    appendU16(out, static_cast<uint16_t>(preface.requested_profile.size()));
    appendBytes(out, reinterpret_cast<const uint8_t*>(preface.requested_profile.data()),
                preface.requested_profile.size());
    appendU32(out, preface.flags);
    return true;
}

bool decodeListenerPrefaceV1(const uint8_t* data,
                             size_t len,
                             ListenerPrefaceV1& out,
                             core::ErrorContext* ctx) {
    out = ListenerPrefaceV1{};
    if (data == nullptr || len < 4 + 2 + 2 + 4 + 4 + 2 + 2 + 4) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "LPREFACE too short");
        return false;
    }

    size_t offset = 0;
    out.magic = readU32(data + offset);
    offset += 4;
    out.version = readU16(data + offset);
    offset += 2;
    out.reserved = readU16(data + offset);
    offset += 2;
    out.listener_id = readU32(data + offset);
    offset += 4;

    if (out.magic != LPREFACE_MAGIC || out.version != LPREFACE_VERSION) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "LPREFACE version mismatch");
        return false;
    }

    const uint32_t dbbt_len = readU32(data + offset);
    offset += 4;
    if (dbbt_len == 0 || dbbt_len > LPREFACE_MAX_DBBT_BYTES || offset + dbbt_len > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "LPREFACE invalid DBBT length");
        return false;
    }
    out.dbbt.assign(data + offset, data + offset + dbbt_len);
    offset += dbbt_len;

    if (offset + 2 > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "LPREFACE truncated db_selector length");
        return false;
    }
    const uint16_t selector_len = readU16(data + offset);
    offset += 2;
    if (selector_len > LPREFACE_MAX_TEXT_BYTES || offset + selector_len > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "LPREFACE invalid db_selector");
        return false;
    }
    out.db_selector.assign(reinterpret_cast<const char*>(data + offset), selector_len);
    offset += selector_len;

    if (offset + 2 > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "LPREFACE truncated profile length");
        return false;
    }
    const uint16_t profile_len = readU16(data + offset);
    offset += 2;
    if (profile_len > LPREFACE_MAX_TEXT_BYTES || offset + profile_len > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "LPREFACE invalid requested_profile");
        return false;
    }
    out.requested_profile.assign(reinterpret_cast<const char*>(data + offset), profile_len);
    offset += profile_len;

    if (offset + 4 != len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "LPREFACE truncated flags");
        return false;
    }
    out.flags = readU32(data + offset);
    return true;
}

bool encodeListenerPrefaceAck(const ListenerPrefaceAck& ack,
                              std::vector<uint8_t>& out,
                              core::ErrorContext* ctx) {
    if (ack.message.size() > LPREFACE_MAX_TEXT_BYTES || ack.message.size() > UINT16_MAX) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "LPREFACE ack message too large");
        return false;
    }
    out.clear();
    out.reserve(1 + 2 + 2 + ack.message.size());
    out.push_back(ack.accepted ? 1 : 0);
    appendU16(out, static_cast<uint16_t>(ack.nack_code));
    appendU16(out, static_cast<uint16_t>(ack.message.size()));
    appendBytes(out, reinterpret_cast<const uint8_t*>(ack.message.data()), ack.message.size());
    return true;
}

bool decodeListenerPrefaceAck(const uint8_t* data,
                              size_t len,
                              ListenerPrefaceAck& out,
                              core::ErrorContext* ctx) {
    out = ListenerPrefaceAck{};
    if (data == nullptr || len < 1 + 2 + 2) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "LPREFACE ack too short");
        return false;
    }

    size_t offset = 0;
    out.accepted = data[offset++] != 0;
    out.nack_code = static_cast<ListenerPrefaceNackCode>(readU16(data + offset));
    offset += 2;
    const uint16_t message_len = readU16(data + offset);
    offset += 2;
    if (offset + message_len != len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "LPREFACE ack invalid message length");
        return false;
    }
    out.message.assign(reinterpret_cast<const char*>(data + offset), message_len);
    return true;
}

core::Status validateListenerPrefaceV1(const std::vector<uint8_t>& encoded_preface,
                                       const DatabaseBindingKeyRing& key_ring,
                                       const DatabaseBindingValidationOptions& options,
                                       DatabaseBindingReplayCache* replay_cache,
                                       ListenerPrefaceV1* preface_out,
                                       DatabaseBindingToken* token_out,
                                       core::ErrorContext* ctx) {
    ListenerPrefaceV1 preface;
    if (!decodeListenerPrefaceV1(encoded_preface.data(), encoded_preface.size(), preface, ctx)) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (preface.listener_id == 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "LPREFACE missing listener_id");
        return core::Status::PROTOCOL_VIOLATION;
    }

    DatabaseBindingValidationOptions dbbt_options = options;
    if (dbbt_options.expected_listener_id == 0) {
        dbbt_options.expected_listener_id = preface.listener_id;
    } else if (preface.listener_id != dbbt_options.expected_listener_id) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_AUTHORIZATION, "LPREFACE listener mismatch");
        return core::Status::INVALID_AUTHORIZATION;
    }

    DatabaseBindingToken token;
    core::Status status = validateDatabaseBindingToken(
        preface.dbbt, key_ring, dbbt_options, replay_cache, &token, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    if (preface_out) {
        *preface_out = std::move(preface);
    }
    if (token_out) {
        *token_out = std::move(token);
    }
    return core::Status::OK;
}

bool encodeDatabaseBindingToken(const DatabaseBindingToken& token,
                                std::vector<uint8_t>& out,
                                core::ErrorContext* ctx) {
    if (token.mac.size() > UINT16_MAX) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "DBBT MAC too large for wire format");
        return false;
    }

    std::vector<uint8_t> body;
    if (!encodeDatabaseBindingTokenBody(token, body, ctx)) {
        return false;
    }

    out = std::move(body);
    appendU16(out, static_cast<uint16_t>(token.mac.size()));
    appendBytes(out, token.mac.data(), token.mac.size());
    return true;
}

bool decodeDatabaseBindingToken(const uint8_t* data,
                                size_t len,
                                DatabaseBindingToken& token,
                                core::ErrorContext* ctx) {
    token = DatabaseBindingToken{};
    if (data == nullptr || len < DBBT_MIN_WIRE_BYTES || len > DBBT_MAX_WIRE_BYTES) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Invalid DBBT size");
        return false;
    }

    size_t offset = 0;
    token.version = data[offset++];

    if (offset + DBBT_UUID_BYTES > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Truncated DBBT uuid");
        return false;
    }
    std::memcpy(token.db_uuid.data(), data + offset, DBBT_UUID_BYTES);
    offset += DBBT_UUID_BYTES;

    if (offset + 4 + 8 + 8 + DBBT_SESSION_BYTES > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Truncated DBBT header");
        return false;
    }
    token.listener_id = readU32(data + offset);
    offset += 4;
    token.issued_at_ms = readU64(data + offset);
    offset += 8;
    token.expires_at_ms = readU64(data + offset);
    offset += 8;
    std::memcpy(token.manager_session_id.data(), data + offset, DBBT_SESSION_BYTES);
    offset += DBBT_SESSION_BYTES;

    if (offset + 2 > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated DBBT client nonce length");
        return false;
    }
    const uint16_t client_len = readU16(data + offset);
    offset += 2;
    if (client_len > DBBT_MAX_NONCE_BYTES || offset + client_len > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Invalid DBBT client nonce");
        return false;
    }
    token.client_nonce.assign(data + offset, data + offset + client_len);
    offset += client_len;

    if (offset + 2 > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated DBBT server nonce length");
        return false;
    }
    const uint16_t server_len = readU16(data + offset);
    offset += 2;
    if (server_len > DBBT_MAX_NONCE_BYTES || offset + server_len > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Invalid DBBT server nonce");
        return false;
    }
    token.server_nonce.assign(data + offset, data + offset + server_len);
    offset += server_len;

    if (offset + 4 + 2 > len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Truncated DBBT flags/mac");
        return false;
    }
    token.flags = readU32(data + offset);
    offset += 4;

    const uint16_t mac_len = readU16(data + offset);
    offset += 2;
    if (mac_len == 0 || offset + mac_len != len) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Invalid DBBT mac length");
        return false;
    }
    token.mac.assign(data + offset, data + offset + mac_len);
    return true;
}

std::vector<uint8_t> databaseBindingTokenId(const DatabaseBindingToken& token) {
    std::vector<uint8_t> id;
    if (token.mac.size() >= DBBT_UUID_BYTES) {
        id.assign(token.mac.begin(), token.mac.begin() + DBBT_UUID_BYTES);
    } else if (!token.mac.empty()) {
        id = token.mac;
    }
    return id;
}

bool DatabaseBindingKeyRing::addKey(const std::string& key_id,
                                    const std::vector<uint8_t>& key,
                                    bool make_active,
                                    core::ErrorContext* ctx) {
    if (key_id.empty()) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "DBBT key_id required");
        return false;
    }
    if (key.size() < 16 || key.size() > 128) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "DBBT key length must be 16..128");
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    keys_[key_id] = key;
    if (make_active || active_key_.empty()) {
        active_key_ = key_id;
    }
    return true;
}

bool DatabaseBindingKeyRing::setActiveKey(const std::string& key_id, core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = keys_.find(key_id);
    if (it == keys_.end()) {
        SET_ERROR_CONTEXT(ctx, core::Status::NOT_FOUND, "DBBT key_id not found");
        return false;
    }
    active_key_ = key_id;
    return true;
}

bool DatabaseBindingKeyRing::hasActiveKey() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !active_key_.empty() && keys_.find(active_key_) != keys_.end();
}

bool DatabaseBindingKeyRing::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return keys_.empty();
}

size_t DatabaseBindingKeyRing::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return keys_.size();
}

std::string DatabaseBindingKeyRing::activeKeyId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_key_;
}

core::Status DatabaseBindingKeyRing::sign(DatabaseBindingToken& token, core::ErrorContext* ctx) const {
    std::vector<uint8_t> key;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = keys_.find(active_key_);
        if (it == keys_.end()) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "No active DBBT key");
            return core::Status::INVALID_ARGUMENT;
        }
        key = it->second;
    }

    std::vector<uint8_t> body;
    if (!encodeDatabaseBindingTokenBody(token, body, ctx)) {
        return core::Status::INVALID_ARGUMENT;
    }

    std::vector<uint8_t> mac;
    if (!computeHmacSha256(key, body.data(), body.size(), mac, ctx)) {
        return core::Status::INTERNAL_ERROR;
    }
    token.mac.swap(mac);
    return core::Status::OK;
}

core::Status DatabaseBindingKeyRing::verify(const DatabaseBindingToken& token,
                                            std::string* matched_key_id,
                                            core::ErrorContext* ctx) const {
    if (token.mac.empty()) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "DBBT missing mac");
        return core::Status::PROTOCOL_VIOLATION;
    }

    std::vector<uint8_t> body;
    if (!encodeDatabaseBindingTokenBody(token, body, ctx)) {
        return core::Status::PROTOCOL_VIOLATION;
    }

    std::map<std::string, std::vector<uint8_t>> keys_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        keys_copy = keys_;
    }
    if (keys_copy.empty()) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "DBBT keyring is empty");
        return core::Status::INVALID_ARGUMENT;
    }

    for (const auto& entry : keys_copy) {
        std::vector<uint8_t> expected_mac;
        if (!computeHmacSha256(entry.second, body.data(), body.size(), expected_mac, ctx)) {
            continue;
        }
        if (timingSafeEqual(expected_mac, token.mac)) {
            if (matched_key_id) {
                *matched_key_id = entry.first;
            }
            return core::Status::OK;
        }
    }

    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_AUTHORIZATION, "DBBT mac verification failed");
    return core::Status::INVALID_AUTHORIZATION;
}

core::Status DatabaseBindingKeyRing::loadFromTextFile(const std::string& path,
                                                      DatabaseBindingKeyRing& out,
                                                      core::ErrorContext* ctx) {
    std::ifstream input(path);
    if (!input.good()) {
        SET_ERROR_CONTEXT(ctx, core::Status::FILE_NOT_FOUND, "Unable to open DBBT keyring file");
        return core::Status::FILE_NOT_FOUND;
    }
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return loadFromText(text, out, ctx);
}

core::Status DatabaseBindingKeyRing::loadFromText(std::string_view text,
                                                  DatabaseBindingKeyRing& out,
                                                  core::ErrorContext* ctx) {
    std::map<std::string, std::vector<uint8_t>> parsed_keys;
    std::string parsed_active;
    std::string requested_active;

    size_t cursor = 0;
    while (cursor <= text.size()) {
        size_t end = text.find('\n', cursor);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string line(text.substr(cursor, end - cursor));
        cursor = end + 1;

        line = trimAscii(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        bool active_inline = false;
        if (!line.empty() && line.front() == '*') {
            active_inline = true;
            line.erase(line.begin());
            line = trimAscii(line);
        }

        const size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 >= line.size()) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Invalid DBBT keyring line");
            return core::Status::INVALID_ARGUMENT;
        }

        std::string key = trimAscii(line.substr(0, eq));
        std::string value = trimAscii(line.substr(eq + 1));

        if (key == "active") {
            requested_active = value;
            continue;
        }
        if (value.rfind("hex:", 0) == 0) {
            value = value.substr(4);
        }

        std::vector<uint8_t> key_bytes;
        if (!hexToBytes(value, key_bytes)) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Invalid DBBT key hex value");
            return core::Status::INVALID_ARGUMENT;
        }
        if (key_bytes.size() < 16 || key_bytes.size() > 128) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "DBBT key length must be 16..128");
            return core::Status::INVALID_ARGUMENT;
        }
        parsed_keys[key] = key_bytes;
        if (parsed_active.empty()) {
            parsed_active = key;
        }
        if (active_inline) {
            requested_active = key;
        }
    }

    if (parsed_keys.empty()) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "DBBT keyring contains no keys");
        return core::Status::INVALID_ARGUMENT;
    }
    if (!requested_active.empty()) {
        if (parsed_keys.find(requested_active) == parsed_keys.end()) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "DBBT active key_id not found");
            return core::Status::INVALID_ARGUMENT;
        }
        parsed_active = requested_active;
    }

    {
        std::lock_guard<std::mutex> lock(out.mutex_);
        out.keys_ = std::move(parsed_keys);
        out.active_key_ = std::move(parsed_active);
    }
    return core::Status::OK;
}

DatabaseBindingReplayCache::DatabaseBindingReplayCache(size_t max_entries)
    : max_entries_(std::max<size_t>(1, max_entries)) {}

bool DatabaseBindingReplayCache::checkAndInsert(const std::vector<uint8_t>& token_id,
                                                uint64_t expires_at_ms,
                                                uint64_t now_ms) {
    if (token_id.empty()) {
        return false;
    }
    if (expires_at_ms <= now_ms) {
        return false;
    }

    const std::string key(reinterpret_cast<const char*>(token_id.data()), token_id.size());
    std::lock_guard<std::mutex> lock(mutex_);
    while (!insertion_order_.empty()) {
        const std::string& oldest_key = insertion_order_.front();
        auto it = entries_.find(oldest_key);
        if (it == entries_.end()) {
            insertion_order_.pop_front();
            continue;
        }
        if (it->second.expires_at_ms > now_ms) {
            break;
        }
        entries_.erase(it);
        insertion_order_.pop_front();
    }

    auto existing = entries_.find(key);
    if (existing != entries_.end()) {
        return false;
    }

    while (!insertion_order_.empty() && entries_.size() >= max_entries_) {
        const std::string oldest = insertion_order_.front();
        insertion_order_.pop_front();
        entries_.erase(oldest);
    }

    entries_[key] = Entry{expires_at_ms};
    insertion_order_.push_back(key);
    return true;
}

bool DatabaseBindingReplayCache::checkAndInsert(const DatabaseBindingToken& token, uint64_t now_ms) {
    return checkAndInsert(databaseBindingTokenId(token), token.expires_at_ms, now_ms);
}

void DatabaseBindingReplayCache::pruneExpired(uint64_t now_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!insertion_order_.empty()) {
        const std::string& key = insertion_order_.front();
        auto it = entries_.find(key);
        if (it == entries_.end()) {
            insertion_order_.pop_front();
            continue;
        }
        if (it->second.expires_at_ms > now_ms) {
            break;
        }
        entries_.erase(it);
        insertion_order_.pop_front();
    }
}

size_t DatabaseBindingReplayCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

core::Status validateDatabaseBindingToken(const std::vector<uint8_t>& encoded_token,
                                          const DatabaseBindingKeyRing& key_ring,
                                          const DatabaseBindingValidationOptions& options,
                                          DatabaseBindingReplayCache* replay_cache,
                                          DatabaseBindingToken* token_out,
                                          core::ErrorContext* ctx) {
    DatabaseBindingToken token;
    if (!decodeDatabaseBindingToken(encoded_token.data(), encoded_token.size(), token, ctx)) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (token.version != DBBT_VERSION) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Unsupported DBBT version");
        return core::Status::PROTOCOL_VIOLATION;
    }

    core::ErrorContext verify_ctx;
    core::Status verify_status = key_ring.verify(token, nullptr, &verify_ctx);
    if (verify_status != core::Status::OK) {
        SET_ERROR_CONTEXT(ctx, verify_status,
                          verify_ctx.message.empty() ? "DBBT verification failed"
                                                     : verify_ctx.message.c_str());
        return verify_status;
    }

    if (options.expected_listener_id != 0 &&
        token.listener_id != options.expected_listener_id) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_AUTHORIZATION, "DBBT listener_id mismatch");
        return core::Status::INVALID_AUTHORIZATION;
    }

    const uint64_t now_ms = options.now_ms == 0 ? currentEpochMillis() : options.now_ms;
    const uint64_t skew_ms = options.clock_skew_ms;

    if (token.expires_at_ms + skew_ms < now_ms) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_AUTHORIZATION, "DBBT expired");
        return core::Status::INVALID_AUTHORIZATION;
    }
    if (token.issued_at_ms > now_ms + skew_ms) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_AUTHORIZATION, "DBBT not yet valid");
        return core::Status::INVALID_AUTHORIZATION;
    }

    if (options.enforce_replay && replay_cache) {
        if (!replay_cache->checkAndInsert(token, now_ms)) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_AUTHORIZATION, "DBBT replay detected");
            return core::Status::INVALID_AUTHORIZATION;
        }
    }

    if (token_out) {
        *token_out = std::move(token);
    }
    return core::Status::OK;
}

bool encodeControlPlaneHeader(const ControlPlaneHeader& header, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve(24);
    appendU32(out, header.magic);
    appendU16(out, header.version);
    appendU16(out, header.message_type);
    appendU16(out, header.flags);
    appendU16(out, header.reserved);
    appendU64(out, header.request_id);
    appendU64(out, header.payload_len);
    return true;
}

bool decodeControlPlaneHeader(const uint8_t* data, size_t len, ControlPlaneHeader& header) {
    if (len < 24 || data == nullptr) {
        return false;
    }
    header.magic = readU32(data);
    header.version = readU16(data + 4);
    header.message_type = readU16(data + 6);
    header.flags = readU16(data + 8);
    header.reserved = readU16(data + 10);
    header.request_id = readU64(data + 12);
    header.payload_len = readU64(data + 20);
    return true;
}

core::Status sendControlPlaneMessage(Socket& socket,
                                     const ControlPlaneMessage& message,
                                     socket_t send_fd,
                                     uint32_t target_pid,
                                     core::ErrorContext* ctx) {
    ControlPlaneHeader header = message.header;
    header.payload_len = message.payload.size();
    if (send_fd != INVALID_SOCKET_VALUE) {
        header.flags |= CONTROL_PLANE_FLAG_HAS_HANDLE;
    }

    std::vector<uint8_t> buffer;
    encodeControlPlaneHeader(header, buffer);
    buffer.insert(buffer.end(), message.payload.begin(), message.payload.end());

#ifdef _WIN32
    if (send_fd != INVALID_SOCKET_VALUE) {
        if (target_pid == 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Target PID required for WSADuplicateSocket");
            return core::Status::INVALID_ARGUMENT;
        }
        WSAPROTOCOL_INFO info{};
        if (WSADuplicateSocket(send_fd, target_pid, &info) != 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "WSADuplicateSocket failed");
            return core::Status::IO_ERROR;
        }
        auto info_bytes = reinterpret_cast<const uint8_t*>(&info);
        buffer.insert(buffer.end(), info_bytes, info_bytes + sizeof(info));
    }
    return socket.writeExact(buffer.data(), buffer.size(), ctx);
#else
    struct msghdr msg{};
    struct iovec iov{};
    iov.iov_base = buffer.data();
    iov.iov_len = buffer.size();
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control[CMSG_SPACE(sizeof(int))];
    if (send_fd != INVALID_SOCKET_VALUE) {
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);
        std::memset(control, 0, sizeof(control));
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &send_fd, sizeof(int));
    }

    ssize_t sent = ::sendmsg(socket.getFd(), &msg, 0);
    if (sent < 0 || static_cast<size_t>(sent) != buffer.size()) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "sendmsg failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
#endif
}

core::Status receiveControlPlaneMessage(Socket& socket,
                                        ControlPlaneMessage& message,
                                        socket_t* recv_fd,
                                        core::ErrorContext* ctx) {
    if (recv_fd) {
        *recv_fd = INVALID_SOCKET_VALUE;
    }

#ifdef _WIN32
    std::vector<uint8_t> header_buf(CONTROL_PLANE_HEADER_SIZE);
    core::Status status = socket.readExact(header_buf.data(), header_buf.size(), ctx);
    if (status != core::Status::OK) {
        return status;
    }
    if (!decodeControlPlaneHeader(header_buf.data(), header_buf.size(), message.header)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Invalid control header");
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (message.header.payload_len > CONTROL_PLANE_MAX_MESSAGE) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Control payload too large");
        return core::Status::PROTOCOL_VIOLATION;
    }
    message.payload.resize(static_cast<size_t>(message.header.payload_len));
    if (!message.payload.empty()) {
        status = socket.readExact(message.payload.data(), message.payload.size(), ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    if ((message.header.flags & CONTROL_PLANE_FLAG_HAS_HANDLE) != 0) {
        WSAPROTOCOL_INFO info{};
        status = socket.readExact(&info, sizeof(info), ctx);
        if (status != core::Status::OK) {
            return status;
        }
        SOCKET dup = WSASocket(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO,
                               &info, 0, 0);
        if (dup == INVALID_SOCKET_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "WSASocket failed");
            return core::Status::IO_ERROR;
        }
        if (recv_fd) {
            *recv_fd = dup;
        } else {
            closesocket(dup);
        }
    }
    return core::Status::OK;
#else
    std::vector<uint8_t> header_buf(CONTROL_PLANE_HEADER_SIZE);
    struct msghdr msg{};
    struct iovec iov{};
    iov.iov_base = header_buf.data();
    iov.iov_len = header_buf.size();
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control[CMSG_SPACE(sizeof(int))];
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    ssize_t received = ::recvmsg(socket.getFd(), &msg, MSG_WAITALL);
    if (received <= 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "recvmsg failed");
        return core::Status::CONNECTION_FAILURE;
    }
    if (static_cast<size_t>(received) < CONTROL_PLANE_HEADER_SIZE) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Short control header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
         cmsg != nullptr;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            if (recv_fd) {
                std::memcpy(recv_fd, CMSG_DATA(cmsg), sizeof(int));
            }
        }
    }

    if (!decodeControlPlaneHeader(header_buf.data(), header_buf.size(), message.header)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Invalid control header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (message.header.payload_len > CONTROL_PLANE_MAX_MESSAGE) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION, "Control payload too large");
        return core::Status::PROTOCOL_VIOLATION;
    }

    message.payload.resize(static_cast<size_t>(message.header.payload_len));
    if (!message.payload.empty()) {
        core::Status status = socket.readExact(message.payload.data(), message.payload.size(), ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    return core::Status::OK;
#endif
}

ControlPlaneServer::~ControlPlaneServer() {
    stop();
}

core::Status ControlPlaneServer::start(const std::string& path, core::ErrorContext* ctx) {
    if (path.empty()) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Control socket path required");
        return core::Status::INVALID_ARGUMENT;
    }

#ifdef _WIN32
    (void)path;
    SET_ERROR_CONTEXT(ctx, core::Status::NOT_IMPLEMENTED,
                      "Control-plane sockets not implemented on Windows");
    return core::Status::NOT_IMPLEMENTED;
#else
    path_ = path;

    // Remove any stale socket file
    ::unlink(path.c_str());

    auto socket = Socket::create(AddressFamily::UNIX, SocketType::STREAM, ctx);
    if (!socket) {
        return core::Status::IO_ERROR;
    }

    NetworkAddress address;
    address.family = AddressFamily::UNIX;
    address.path = path;

    core::Status status = socket->bind(address, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    status = socket->listen(DEFAULT_LISTEN_BACKLOG, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    listener_ = std::move(socket);
    return core::Status::OK;
#endif
}

void ControlPlaneServer::stop() {
    if (listener_) {
        listener_->close();
        listener_.reset();
    }
#ifndef _WIN32
    if (!path_.empty()) {
        ::unlink(path_.c_str());
    }
#endif
    path_.clear();
}

std::unique_ptr<Socket> ControlPlaneServer::accept(core::ErrorContext* ctx) {
    if (!listener_) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "Control-plane listener not running");
        return nullptr;
    }
    return listener_->accept(nullptr, ctx);
}

}  // namespace scratchbird::network
