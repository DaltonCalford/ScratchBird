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
 * Control Plane Protocol (Listener <-> Parser)
 *
 * Defines message framing and a minimal control-plane server scaffold.
 */
// Section 32 invariant: declarations in this header define a bounded internal
// management surface. Presence here does not promote these message families
// into a general public plugin or wire-protocol stability contract.

#pragma once

#include <cstdint>
#include <array>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <memory>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/network/socket_types.h"

#ifdef _WIN32
#ifdef ERROR
#pragma push_macro("ERROR")
#undef ERROR
#define SCRATCHBIRD_RESTORE_CONTROL_PLANE_ERROR_MACRO
#endif
#endif

namespace scratchbird::network {

constexpr uint32_t CONTROL_PLANE_MAGIC = 0x54434253; // "SBCT" little-endian
constexpr uint16_t CONTROL_PLANE_VERSION = 1;
constexpr uint16_t CONTROL_PLANE_FLAG_HAS_HANDLE = 0x0001;

constexpr uint8_t DBBT_VERSION = 1;
constexpr size_t DBBT_UUID_BYTES = 16;
constexpr size_t DBBT_SESSION_BYTES = 16;
constexpr size_t DBBT_MAC_BYTES = 32;  // HMAC-SHA-256
constexpr size_t DBBT_MAX_NONCE_BYTES = 1024;
constexpr uint64_t DBBT_DEFAULT_CLOCK_SKEW_MS = 2000;

constexpr uint32_t LPREFACE_MAGIC = 0x504C4253;  // "SBLP" little-endian
constexpr uint16_t LPREFACE_VERSION = 1;
constexpr size_t LPREFACE_MAX_DBBT_BYTES = 8192;
constexpr size_t LPREFACE_MAX_TEXT_BYTES = 1024;

enum class ControlPlaneMessageType : uint16_t {
    HELLO = 0x0001,
    HELLO_ACK = 0x0002,
    SPAWN_REQUEST = 0x0010,
    SPAWN_READY = 0x0011,
    HANDOFF_SOCKET = 0x0020,
    HANDOFF_ACK = 0x0021,
    LPREFACE = 0x0022,
    LPREFACE_ACK = 0x0023,
    LPREFACE_NACK = 0x0024,
    HEALTH_CHECK = 0x0030,
    HEALTH_REPORT = 0x0031,
    POOL_STATS = 0x0040,
    RECYCLE = 0x0050,
    SHUTDOWN = 0x0051,
    MANAGEMENT_COMMAND = 0x0060,
    MANAGEMENT_RESPONSE = 0x0061,
    ERROR_MESSAGE = 0x00FF
};

struct ControlPlaneHeader {
    uint32_t magic = CONTROL_PLANE_MAGIC;
    uint16_t version = CONTROL_PLANE_VERSION;
    uint16_t message_type = 0;
    uint16_t flags = 0;
    uint16_t reserved = 0;
    uint64_t request_id = 0;
    uint64_t payload_len = 0;
};

struct ControlPlaneMessage {
    ControlPlaneHeader header;
    std::vector<uint8_t> payload;
};

/**
 * Database Binding Token (DBBT)
 *
 * Binary layout (all little-endian):
 *  version:u8
 *  db_uuid[16]
 *  listener_id:u32
 *  issued_at_ms:u64
 *  expires_at_ms:u64
 *  manager_session_id[16]
 *  client_nonce_len:u16 + bytes
 *  server_nonce_len:u16 + bytes
 *  flags:u32
 *  mac_len:u16 + mac_bytes
 */
struct DatabaseBindingToken {
    uint8_t version = DBBT_VERSION;
    std::array<uint8_t, DBBT_UUID_BYTES> db_uuid{};
    uint32_t listener_id = 0;
    uint64_t issued_at_ms = 0;
    uint64_t expires_at_ms = 0;
    std::array<uint8_t, DBBT_SESSION_BYTES> manager_session_id{};
    std::vector<uint8_t> client_nonce;
    std::vector<uint8_t> server_nonce;
    uint32_t flags = 0;
    std::vector<uint8_t> mac;
};

class DatabaseBindingKeyRing {
public:
    bool addKey(const std::string& key_id,
                const std::vector<uint8_t>& key,
                bool make_active = false,
                core::ErrorContext* ctx = nullptr);
    bool setActiveKey(const std::string& key_id, core::ErrorContext* ctx = nullptr);
    bool hasActiveKey() const;
    bool empty() const;
    size_t size() const;
    std::string activeKeyId() const;

    core::Status sign(DatabaseBindingToken& token, core::ErrorContext* ctx = nullptr) const;
    core::Status verify(const DatabaseBindingToken& token,
                        std::string* matched_key_id = nullptr,
                        core::ErrorContext* ctx = nullptr) const;

    static core::Status loadFromTextFile(const std::string& path,
                                         DatabaseBindingKeyRing& out,
                                         core::ErrorContext* ctx = nullptr);
    static core::Status loadFromText(std::string_view text,
                                     DatabaseBindingKeyRing& out,
                                     core::ErrorContext* ctx = nullptr);

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::vector<uint8_t>> keys_;
    std::string active_key_;
};

class DatabaseBindingReplayCache {
public:
    explicit DatabaseBindingReplayCache(size_t max_entries = 4096);

    bool checkAndInsert(const std::vector<uint8_t>& token_id,
                        uint64_t expires_at_ms,
                        uint64_t now_ms);
    bool checkAndInsert(const DatabaseBindingToken& token, uint64_t now_ms);
    void pruneExpired(uint64_t now_ms);
    size_t size() const;

private:
    struct Entry {
        uint64_t expires_at_ms = 0;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
    std::deque<std::string> insertion_order_;
    size_t max_entries_;
};

struct DatabaseBindingValidationOptions {
    uint32_t expected_listener_id = 0;
    uint64_t now_ms = 0;
    uint64_t clock_skew_ms = DBBT_DEFAULT_CLOCK_SKEW_MS;
    bool enforce_replay = true;
};

enum class ListenerPrefaceNackCode : uint16_t {
    NONE = 0,
    INVALID_FORMAT = 1,
    INVALID_DBBT = 2,
    LISTENER_MISMATCH = 3,
    DB_NOT_READY = 4,
    POLICY_DENIED = 5,
    RESOURCE_EXHAUSTED = 6,
    INTERNAL_ERROR = 7
};

struct ListenerPrefaceV1 {
    uint32_t magic = LPREFACE_MAGIC;
    uint16_t version = LPREFACE_VERSION;
    uint16_t reserved = 0;
    uint32_t listener_id = 0;
    std::vector<uint8_t> dbbt;
    std::string db_selector;
    std::string requested_profile;
    uint32_t flags = 0;
};

struct ListenerPrefaceAck {
    bool accepted = false;
    ListenerPrefaceNackCode nack_code = ListenerPrefaceNackCode::NONE;
    std::string message;
};

bool encodeControlPlaneHeader(const ControlPlaneHeader& header, std::vector<uint8_t>& out);
bool decodeControlPlaneHeader(const uint8_t* data, size_t len, ControlPlaneHeader& header);

uint64_t currentEpochMillis();

bool encodeDatabaseBindingToken(const DatabaseBindingToken& token,
                                std::vector<uint8_t>& out,
                                core::ErrorContext* ctx = nullptr);
bool decodeDatabaseBindingToken(const uint8_t* data,
                                size_t len,
                                DatabaseBindingToken& token,
                                core::ErrorContext* ctx = nullptr);

std::vector<uint8_t> databaseBindingTokenId(const DatabaseBindingToken& token);

core::Status validateDatabaseBindingToken(const std::vector<uint8_t>& encoded_token,
                                          const DatabaseBindingKeyRing& key_ring,
                                          const DatabaseBindingValidationOptions& options,
                                          DatabaseBindingReplayCache* replay_cache,
                                          DatabaseBindingToken* token_out = nullptr,
                                          core::ErrorContext* ctx = nullptr);

std::string bytesToHex(const std::vector<uint8_t>& bytes);
bool hexToBytes(std::string_view hex, std::vector<uint8_t>& out);

bool encodeListenerPrefaceV1(const ListenerPrefaceV1& preface,
                             std::vector<uint8_t>& out,
                             core::ErrorContext* ctx = nullptr);
bool decodeListenerPrefaceV1(const uint8_t* data,
                             size_t len,
                             ListenerPrefaceV1& out,
                             core::ErrorContext* ctx = nullptr);

bool encodeListenerPrefaceAck(const ListenerPrefaceAck& ack,
                              std::vector<uint8_t>& out,
                              core::ErrorContext* ctx = nullptr);
bool decodeListenerPrefaceAck(const uint8_t* data,
                              size_t len,
                              ListenerPrefaceAck& out,
                              core::ErrorContext* ctx = nullptr);

core::Status validateListenerPrefaceV1(const std::vector<uint8_t>& encoded_preface,
                                       const DatabaseBindingKeyRing& key_ring,
                                       const DatabaseBindingValidationOptions& options,
                                       DatabaseBindingReplayCache* replay_cache,
                                       ListenerPrefaceV1* preface_out = nullptr,
                                       DatabaseBindingToken* token_out = nullptr,
                                       core::ErrorContext* ctx = nullptr);

core::Status sendControlPlaneMessage(Socket& socket,
                                     const ControlPlaneMessage& message,
                                     socket_t send_fd = INVALID_SOCKET_VALUE,
                                     uint32_t target_pid = 0,
                                     core::ErrorContext* ctx = nullptr);
core::Status receiveControlPlaneMessage(Socket& socket,
                                        ControlPlaneMessage& message,
                                        socket_t* recv_fd = nullptr,
                                        core::ErrorContext* ctx = nullptr);

class ControlPlaneServer {
public:
    ControlPlaneServer() = default;
    ~ControlPlaneServer();

    core::Status start(const std::string& path, core::ErrorContext* ctx = nullptr);
    void stop();

    std::unique_ptr<Socket> accept(core::ErrorContext* ctx = nullptr);

    bool isRunning() const { return listener_ != nullptr; }
    const std::string& path() const { return path_; }

private:
    std::unique_ptr<Socket> listener_;
    std::string path_;
};

}  // namespace scratchbird::network

#ifdef SCRATCHBIRD_RESTORE_CONTROL_PLANE_ERROR_MACRO
#pragma pop_macro("ERROR")
#undef SCRATCHBIRD_RESTORE_CONTROL_PLANE_ERROR_MACRO
#endif
