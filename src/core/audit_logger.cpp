/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/secure_diagnostics.h"
#include "scratchbird/core/structured_logger.h"
#include "scratchbird/core/logger.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>

#include <openssl/sha.h>
#include <nlohmann/json.hpp>

namespace scratchbird {
namespace core {

namespace {

using OrderedJson = nlohmann::ordered_json;

struct AuditGovernancePolicy
{
    uint64_t policy_version = 1;
    uint64_t hot_retention_days = 30;
    uint64_t archive_retention_days = 365;
    bool legal_hold_active = false;
    std::string legal_hold_reason;
    std::string legal_hold_actor;
    uint64_t legal_hold_set_time = 0;
    uint64_t legal_hold_release_time = 0;
    OrderedJson root = OrderedJson::object();
};

bool isZeroId(const ID& id)
{
    return std::all_of(id.bytes.begin(), id.bytes.end(), [](uint8_t byte) { return byte == 0; });
}

uint64_t currentSystemTicks()
{
    return static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
}

uint64_t retentionDaysToSystemTicks(uint64_t days)
{
    using ClockDuration = std::chrono::system_clock::duration;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<ClockDuration>(std::chrono::hours(24 * days)).count());
}

std::string joinUuidList(const std::vector<ID>& ids)
{
    std::ostringstream out;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        if (i > 0)
        {
            out << ",";
        }
        out << ids[i].toString();
    }
    return out.str();
}

bool loadAuditGovernancePolicy(const std::string& config_json,
                               AuditGovernancePolicy& policy_out,
                               ErrorContext* ctx)
{
    policy_out = AuditGovernancePolicy{};
    OrderedJson root = OrderedJson::object();
    if (!config_json.empty())
    {
        try
        {
            root = OrderedJson::parse(config_json);
        }
        catch (const std::exception&)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                              "audit sink profile config_json is not valid JSON");
            return false;
        }
        if (!root.is_object())
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                              "audit sink profile config_json must be a JSON object");
            return false;
        }
    }

    const OrderedJson retention =
        root.contains("retention_policy") && root["retention_policy"].is_object()
            ? root["retention_policy"]
            : OrderedJson::object();
    const OrderedJson legal_hold =
        root.contains("legal_hold") && root["legal_hold"].is_object()
            ? root["legal_hold"]
            : OrderedJson::object();

    policy_out.policy_version = root.value("policy_version", 1ull);
    policy_out.hot_retention_days = retention.value("hot_retention_days", 30ull);
    policy_out.archive_retention_days = retention.value("archive_retention_days", 365ull);
    policy_out.legal_hold_active = legal_hold.value("active", false);
    policy_out.legal_hold_reason = legal_hold.value("reason", "");
    policy_out.legal_hold_actor = legal_hold.value("actor", "");
    policy_out.legal_hold_set_time = legal_hold.value("set_time", 0ull);
    policy_out.legal_hold_release_time = legal_hold.value("release_time", 0ull);
    policy_out.root = std::move(root);

    if (policy_out.hot_retention_days == 0 || policy_out.archive_retention_days == 0 ||
        policy_out.archive_retention_days < policy_out.hot_retention_days)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "audit retention policy window is invalid");
        return false;
    }
    return true;
}

void storeAuditGovernancePolicy(const AuditGovernancePolicy& policy,
                                std::string& config_json_out)
{
    OrderedJson root = policy.root.is_object() ? policy.root : OrderedJson::object();
    OrderedJson retention =
        root.contains("retention_policy") && root["retention_policy"].is_object()
            ? root["retention_policy"]
            : OrderedJson::object();
    OrderedJson legal_hold =
        root.contains("legal_hold") && root["legal_hold"].is_object()
            ? root["legal_hold"]
            : OrderedJson::object();

    root["policy_version"] = policy.policy_version;
    retention["hot_retention_days"] = policy.hot_retention_days;
    retention["archive_retention_days"] = policy.archive_retention_days;
    root["retention_policy"] = retention;

    legal_hold["active"] = policy.legal_hold_active;
    legal_hold["reason"] = redactSensitiveDiagnosticText(policy.legal_hold_reason);
    legal_hold["actor"] = redactSensitiveDiagnosticText(policy.legal_hold_actor);
    legal_hold["set_time"] = policy.legal_hold_set_time;
    legal_hold["release_time"] = policy.legal_hold_release_time;
    root["legal_hold"] = legal_hold;

    config_json_out = root.dump();
}

void sanitizeAuditEvent(AuditEvent& event)
{
    event.details = redactSensitiveDiagnosticText(event.details);
    event.ip_address = redactSensitiveDiagnosticField("ip_address", event.ip_address);
    event.application_name = redactSensitiveDiagnosticField(
        "application_name", event.application_name);
}

void appendBytes(std::vector<uint8_t>& out, const void* data, size_t len)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + len);
}

void appendUint64(std::vector<uint8_t>& out, uint64_t value)
{
    for (int i = 7; i >= 0; --i)
    {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

void appendUint32(std::vector<uint8_t>& out, uint32_t value)
{
    for (int i = 3; i >= 0; --i)
    {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

void appendUint16(std::vector<uint8_t>& out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void appendUint8(std::vector<uint8_t>& out, uint8_t value)
{
    out.push_back(value);
}

void appendString(std::vector<uint8_t>& out, const std::string& value)
{
    appendUint32(out, static_cast<uint32_t>(value.size()));
    if (!value.empty())
    {
        appendBytes(out, value.data(), value.size());
    }
}

void appendId(std::vector<uint8_t>& out, const ID& id)
{
    appendBytes(out, id.bytes.data(), id.bytes.size());
}

std::string hashToHex(const std::array<uint8_t, 32>& hash)
{
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : hash)
    {
        ss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return ss.str();
}

bool parseHexHash(const std::string& text, std::array<uint8_t, 32>& hash_out)
{
    if (text.size() != 64)
    {
        return false;
    }

    auto hex_value = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    };

    for (size_t i = 0; i < hash_out.size(); ++i)
    {
        int high = hex_value(text[i * 2]);
        int low = hex_value(text[i * 2 + 1]);
        if (high < 0 || low < 0)
        {
            return false;
        }
        hash_out[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

bool parseUuidText(const std::string& text, ID& id_out)
{
    std::string hex;
    hex.reserve(32);
    for (char ch : text)
    {
        if (ch == '-')
        {
            continue;
        }
        hex.push_back(ch);
    }
    if (hex.size() != 32)
    {
        return false;
    }

    auto hex_value = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    };

    id_out = ID{};
    for (size_t i = 0; i < id_out.bytes.size(); ++i)
    {
        int high = hex_value(hex[i * 2]);
        int low = hex_value(hex[i * 2 + 1]);
        if (high < 0 || low < 0)
        {
            return false;
        }
        id_out.bytes[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

bool parseUint64Text(const std::string& text, uint64_t& value_out)
{
    try
    {
        size_t consumed = 0;
        value_out = std::stoull(text, &consumed, 10);
        return consumed == text.size();
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::string sha256Hex(const std::string& data)
{
    std::array<uint8_t, 32> hash{};
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash.data());
    return hashToHex(hash);
}

std::string serializeAuditEventJsonLine(const AuditEvent& event,
                                        const std::array<uint8_t, 32>& hash_prev,
                                        const std::array<uint8_t, 32>& hash_curr)
{
    std::ostringstream out;
    out << "{"
        << "\"event_id\":" << event.event_id << ","
        << "\"timestamp\":" << event.timestamp << ","
        << "\"event_type\":\"" << StructuredLogEntry::escapeJson(AuditLogger::getEventTypeName(event.event_type)) << "\","
        << "\"success\":" << (event.success ? "true" : "false") << ","
        << "\"user_id\":\"" << event.user_id.toString() << "\","
        << "\"role_id\":\"" << event.role_id.toString() << "\","
        << "\"session_id\":\"" << event.session_id.toString() << "\","
        << "\"authkey_id\":\"" << event.authkey_id.toString() << "\","
        << "\"username\":\"" << StructuredLogEntry::escapeJson(event.username) << "\","
        << "\"target_username\":\"" << StructuredLogEntry::escapeJson(event.target_username) << "\","
        << "\"object_type\":\"" << StructuredLogEntry::escapeJson(event.object_type) << "\","
        << "\"object_name\":\"" << StructuredLogEntry::escapeJson(event.object_name) << "\","
        << "\"object_id\":\"" << event.object_id.toString() << "\","
        << "\"details\":\"" << StructuredLogEntry::escapeJson(event.details) << "\","
        << "\"ip_address\":\"" << StructuredLogEntry::escapeJson(event.ip_address) << "\","
        << "\"application_name\":\"" << StructuredLogEntry::escapeJson(event.application_name) << "\","
        << "\"hash_prev\":\"" << hashToHex(hash_prev) << "\","
        << "\"hash_curr\":\"" << hashToHex(hash_curr) << "\""
        << "}\n";
    return out.str();
}

constexpr char kAuditExportPackageMagic[] = "SB_AUDIT_EXPORT_PACKAGE_V1";
constexpr char kAuditExportManifestEnd[] = "END_MANIFEST\n";

struct AuditExportManifestData
{
    ID segment_id;
    ID database_id;
    ID sink_profile_id;
    std::string profile_name;
    std::string sink_type;
    std::string failure_policy;
    std::string evidence_class;
    uint64_t segment_seq = 0;
    uint64_t range_start_time = 0;
    uint64_t range_end_time = 0;
    uint64_t event_count = 0;
    uint64_t first_event_id = 0;
    uint64_t last_event_id = 0;
    std::string first_event_hash_prev;
    std::string last_event_hash_curr;
    std::string payload_sha256;
    uint64_t payload_bytes = 0;
};

std::string buildAuditExportManifest(const AuditExportManifestData& manifest)
{
    std::ostringstream out;
    out << kAuditExportPackageMagic << "\n";
    out << "manifest_version=1\n";
    out << "segment_uuid=" << manifest.segment_id.toString() << "\n";
    out << "database_uuid=" << manifest.database_id.toString() << "\n";
    out << "sink_profile_uuid=" << manifest.sink_profile_id.toString() << "\n";
    out << "profile_name=" << manifest.profile_name << "\n";
    out << "sink_type=" << manifest.sink_type << "\n";
    out << "failure_policy=" << manifest.failure_policy << "\n";
    out << "evidence_class=" << manifest.evidence_class << "\n";
    out << "segment_seq=" << manifest.segment_seq << "\n";
    out << "range_start_time=" << manifest.range_start_time << "\n";
    out << "range_end_time=" << manifest.range_end_time << "\n";
    out << "event_count=" << manifest.event_count << "\n";
    out << "first_event_id=" << manifest.first_event_id << "\n";
    out << "last_event_id=" << manifest.last_event_id << "\n";
    out << "first_event_hash_prev=" << manifest.first_event_hash_prev << "\n";
    out << "last_event_hash_curr=" << manifest.last_event_hash_curr << "\n";
    out << "payload_sha256=" << manifest.payload_sha256 << "\n";
    out << "payload_bytes=" << manifest.payload_bytes << "\n";
    out << kAuditExportManifestEnd;
    return out.str();
}

std::string appendManifestFields(std::string manifest_text,
                                 const std::unordered_map<std::string, std::string>& extra_fields)
{
    const std::string marker = kAuditExportManifestEnd;
    const size_t pos = manifest_text.rfind(marker);
    if (pos == std::string::npos)
    {
        return manifest_text;
    }

    std::ostringstream extra;
    for (const auto& [key, value] : extra_fields)
    {
        extra << key << "=" << redactSensitiveDiagnosticField(key, value) << "\n";
    }
    manifest_text.insert(pos, extra.str());
    return manifest_text;
}

bool parseAuditExportManifest(const std::string& manifest_text,
                              AuditExportManifestData& manifest_out,
                              ErrorContext* ctx)
{
    std::istringstream in(manifest_text);
    std::string line;
    if (!std::getline(in, line) || line != kAuditExportPackageMagic)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "audit export manifest magic is invalid");
        return false;
    }

    std::unordered_map<std::string, std::string> fields;
    while (std::getline(in, line))
    {
        if (line == "END_MANIFEST")
        {
            break;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "audit export manifest line is malformed");
            return false;
        }
        fields.emplace(line.substr(0, eq), line.substr(eq + 1));
    }

    auto require = [&fields, ctx](const char* key, std::string& value_out) -> bool {
        auto it = fields.find(key);
        if (it == fields.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, key);
            return false;
        }
        value_out = it->second;
        return true;
    };

    std::string value;
    if (!require("segment_uuid", value) || !parseUuidText(value, manifest_out.segment_id)) return false;
    if (!require("database_uuid", value) || !parseUuidText(value, manifest_out.database_id)) return false;
    if (!require("sink_profile_uuid", value) || !parseUuidText(value, manifest_out.sink_profile_id)) return false;
    if (!require("profile_name", manifest_out.profile_name)) return false;
    if (!require("sink_type", manifest_out.sink_type)) return false;
    if (!require("failure_policy", manifest_out.failure_policy)) return false;
    if (!require("evidence_class", manifest_out.evidence_class)) return false;
    if (!require("segment_seq", value) || !parseUint64Text(value, manifest_out.segment_seq)) return false;
    if (!require("range_start_time", value) || !parseUint64Text(value, manifest_out.range_start_time)) return false;
    if (!require("range_end_time", value) || !parseUint64Text(value, manifest_out.range_end_time)) return false;
    if (!require("event_count", value) || !parseUint64Text(value, manifest_out.event_count)) return false;
    if (!require("first_event_id", value) || !parseUint64Text(value, manifest_out.first_event_id)) return false;
    if (!require("last_event_id", value) || !parseUint64Text(value, manifest_out.last_event_id)) return false;
    if (!require("first_event_hash_prev", manifest_out.first_event_hash_prev)) return false;
    if (!require("last_event_hash_curr", manifest_out.last_event_hash_curr)) return false;
    if (!require("payload_sha256", manifest_out.payload_sha256)) return false;
    if (!require("payload_bytes", value) || !parseUint64Text(value, manifest_out.payload_bytes)) return false;
    return true;
}

bool splitAuditExportPackage(const std::string& package_contents,
                             std::string& manifest_out,
                             std::string& payload_out,
                             ErrorContext* ctx)
{
    const std::string marker = kAuditExportManifestEnd;
    const size_t pos = package_contents.find(marker);
    if (pos == std::string::npos)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "audit export package is missing manifest terminator");
        return false;
    }
    manifest_out = package_contents.substr(0, pos + marker.size());
    payload_out = package_contents.substr(pos + marker.size());
    return true;
}

bool matchesQuery(const AuditEvent& event, const AuditQuery& query)
{
    if (query.start_time > 0 && event.timestamp < query.start_time)
    {
        return false;
    }
    if (query.end_time > 0 && event.timestamp > query.end_time)
    {
        return false;
    }
    if (query.user_id.has_value())
    {
        if (std::memcmp(&event.user_id, &query.user_id.value(), sizeof(ID)) != 0)
        {
            return false;
        }
    }
    if (query.session_id.has_value())
    {
        if (std::memcmp(&event.session_id, &query.session_id.value(), sizeof(ID)) != 0)
        {
            return false;
        }
    }
    if (query.authkey_id.has_value())
    {
        if (std::memcmp(&event.authkey_id, &query.authkey_id.value(), sizeof(ID)) != 0)
        {
            return false;
        }
    }
    if (query.username.has_value() && event.username != query.username.value())
    {
        return false;
    }
    if (query.event_type.has_value() && event.event_type != query.event_type.value())
    {
        return false;
    }
    if (!query.object_name.empty() && event.object_name != query.object_name)
    {
        return false;
    }
    if (query.success.has_value() && event.success != query.success.value())
    {
        return false;
    }

    return true;
}

} // namespace

AuditLogger::AuditLogger(CatalogManager* catalog)
    : catalog_(catalog),
      next_event_id_(1)
{
    buffer_.reserve(MAX_BUFFER_SIZE);
    last_hash_.fill(0);

    if (catalog_)
    {
        ErrorContext ctx;
        uint64_t last_event_id = 0;
        std::array<uint8_t, 32> last_hash;
        Status status = catalog_->getAuditLogTail(last_event_id, last_hash, &ctx);
        if (status == Status::OK)
        {
            if (last_event_id >= next_event_id_)
            {
                next_event_id_ = last_event_id + 1;
            }
            last_hash_ = last_hash;
            tail_loaded_ = true;
        }
        else
        {
            LOG_WARNING(GENERAL, "Audit logger failed to load audit log tail: %s",
                        ctx.message.c_str());
        }
    }
}

AuditLogger::~AuditLogger()
{
    // Flush any remaining events
    ErrorContext ctx;
    flush(&ctx);
}

uint64_t AuditLogger::getCurrentTimeMs() const
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void AuditLogger::configureSinks(const AuditSinkConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    bool was_catalog_enabled = sink_config_.enable_catalog;
    sink_config_ = config;
    if (!was_catalog_enabled && sink_config_.enable_catalog)
    {
        tail_loaded_ = false;
    }
}

void AuditLogger::addBroadcastSink(const std::function<void(const AuditEvent&)>& sink)
{
    std::lock_guard<std::mutex> lock(mutex_);
    broadcast_sinks_.push_back(sink);
}

Status AuditLogger::logEvent(AuditEvent& event, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!tail_loaded_ && catalog_ && sink_config_.enable_catalog)
    {
        uint64_t last_event_id = 0;
        std::array<uint8_t, 32> last_hash;
        Status status = catalog_->getAuditLogTail(last_event_id, last_hash, ctx);
        if (status == Status::OK)
        {
            if (last_event_id >= next_event_id_)
            {
                next_event_id_ = last_event_id + 1;
            }
            last_hash_ = last_hash;
            tail_loaded_ = true;
        }
    }

    // Fill in automatic fields
    event.event_id = next_event_id_++;
    event.timestamp = getCurrentTimeMs();
    sanitizeAuditEvent(event);

    AuditBufferEntry entry;
    entry.event = event;
    entry.hash_prev = last_hash_;
    entry.hash_curr = computeChainHash(event, entry.hash_prev);
    last_hash_ = entry.hash_curr;

    // Add to buffer
    buffer_.push_back(entry);

    // Flush if buffer is full (use unlocked version since we hold mutex)
    if ((buffer_.size() - flush_cursor_) >= MAX_BUFFER_SIZE)
    {
        return flushUnlocked(ctx);
    }

    return Status::OK;
}

Status AuditLogger::writeEventToCatalog(const AuditBufferEntry& entry, ErrorContext* ctx)
{
    if (!catalog_ || !sink_config_.enable_catalog)
    {
        // No catalog available - events only in memory
        return Status::OK;
    }

    return catalog_->appendAuditLog(entry.event, entry.hash_prev, entry.hash_curr, ctx);
}

Status AuditLogger::writeEventToFile(const AuditBufferEntry& entry, ErrorContext* ctx)
{
    if (!sink_config_.enable_file)
    {
        return Status::OK;
    }

    if (sink_config_.file_path.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Audit file sink enabled but file path is empty");
        return Status::INVALID_ARGUMENT;
    }

    std::ofstream out(sink_config_.file_path, std::ios::app);
    if (!out.is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open audit log file sink");
        return Status::IO_ERROR;
    }

    out << serializeAuditEventJsonLine(entry.event, entry.hash_prev, entry.hash_curr);

    if (!out.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write audit log file sink");
        return Status::IO_ERROR;
    }

    return Status::OK;
}

void AuditLogger::broadcastEvent(const AuditEvent& event)
{
    if (!sink_config_.enable_broadcast)
    {
        return;
    }

    for (const auto& sink : broadcast_sinks_)
    {
        if (sink)
        {
            try
            {
                sink(event);
            }
            catch (const std::exception& ex)
            {
                LOG_WARNING(GENERAL, "Audit broadcast sink failed: %s", ex.what());
            }
        }
    }
}

std::array<uint8_t, 32> AuditLogger::computeChainHash(const AuditEvent& event,
                                                      const std::array<uint8_t, 32>& prev_hash)
{
    std::vector<uint8_t> data;
    data.reserve(256 + event.username.size() + event.target_username.size() +
                 event.object_type.size() + event.object_name.size() +
                 event.details.size());

    appendBytes(data, prev_hash.data(), prev_hash.size());
    appendUint64(data, event.event_id);
    appendUint64(data, event.timestamp);
    appendUint16(data, static_cast<uint16_t>(event.event_type));
    appendUint8(data, event.success ? 1 : 0);
    appendId(data, event.user_id);
    appendId(data, event.role_id);
    appendId(data, event.target_user_id);
    appendId(data, event.object_id);
    appendId(data, event.session_id);
    appendId(data, event.authkey_id);
    appendString(data, event.username);
    appendString(data, event.target_username);
    appendString(data, event.object_type);
    appendString(data, event.object_name);
    appendString(data, event.details);

    std::array<uint8_t, 32> hash{};
    SHA256(data.data(), data.size(), hash.data());
    return hash;
}

std::array<uint8_t, 32> AuditLogger::computeExportSegmentHash(
    const std::string& manifest_payload,
    const std::array<uint8_t, 32>& prev_hash)
{
    std::vector<uint8_t> data;
    data.reserve(prev_hash.size() + manifest_payload.size());
    appendBytes(data, prev_hash.data(), prev_hash.size());
    if (!manifest_payload.empty())
    {
        appendBytes(data, manifest_payload.data(), manifest_payload.size());
    }

    std::array<uint8_t, 32> hash{};
    SHA256(data.data(), data.size(), hash.data());
    return hash;
}

Status AuditLogger::verifyIntegrity(AuditIntegrityResult& result_out, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    result_out = AuditIntegrityResult{};

    if (catalog_ && sink_config_.enable_catalog)
    {
        return catalog_->verifyAuditLogChain(result_out, ctx);
    }

    std::array<uint8_t, 32> expected_prev{};
    uint64_t expected_event_id = 1;

    for (const auto& entry : buffer_)
    {
        if (entry.event.event_id != expected_event_id)
        {
            result_out.first_bad_event_id = entry.event.event_id;
            result_out.failure_reason = "audit event_id sequence is not append-only";
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, result_out.failure_reason.c_str());
            return Status::DATA_CORRUPTED;
        }
        if (entry.hash_prev != expected_prev)
        {
            result_out.first_bad_event_id = entry.event.event_id;
            result_out.failure_reason = "audit hash_prev link does not match prior event";
            SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH, result_out.failure_reason.c_str());
            return Status::CHECKSUM_MISMATCH;
        }

        const auto expected_curr = computeChainHash(entry.event, expected_prev);
        if (entry.hash_curr != expected_curr)
        {
            result_out.first_bad_event_id = entry.event.event_id;
            result_out.failure_reason = "audit hash_curr does not match persisted event payload";
            SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH, result_out.failure_reason.c_str());
            return Status::CHECKSUM_MISMATCH;
        }

        result_out.verified_event_count += 1;
        result_out.last_verified_event_id = entry.event.event_id;
        expected_prev = entry.hash_curr;
        expected_event_id += 1;
    }

    result_out.chain_intact = true;
    return Status::OK;
}

Status AuditLogger::exportAuditPackage(const AuditExportPackageRequest& request,
                                       AuditExportPackageResult& result_out,
                                       ErrorContext* ctx)
{
    result_out = AuditExportPackageResult{};

    if (!catalog_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Audit export requires catalog-backed persistence");
        return Status::INVALID_ARGUMENT;
    }
    if (!sink_config_.enable_catalog)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Audit export requires the catalog sink to be enabled");
        return Status::INVALID_ARGUMENT;
    }
    if (request.output_path.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Audit export output_path is required");
        return Status::INVALID_ARGUMENT;
    }

    CatalogManager::AuditSinkProfileCatalogInfo profile;
    Status status = catalog_->getAuditSinkProfileCatalogEntry(request.sink_profile_id, profile, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    if (!profile.is_enabled)
    {
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, "Audit sink profile is disabled");
        return Status::CONSTRAINT_VIOLATION;
    }
    if (profile.sink_type != "LOCAL_APPEND_ONLY")
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED,
                          "NCW-034 export currently supports LOCAL_APPEND_ONLY sink profiles only");
        return Status::NOT_SUPPORTED;
    }

    std::filesystem::path output_path(request.output_path);
    std::error_code ec;
    if (std::filesystem::exists(output_path, ec))
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Audit export output path already exists");
        return Status::FILE_EXISTS;
    }
    if (output_path.has_parent_path())
    {
        std::filesystem::create_directories(output_path.parent_path(), ec);
        if (ec)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to create audit export directory");
            return Status::IO_ERROR;
        }
    }

    status = flush(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::vector<CatalogManager::AuditExportSegmentCatalogInfo> prior_segments;
    status = catalog_->listAuditExportSegmentCatalogEntries(request.sink_profile_id, prior_segments, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint64_t last_exported_event_id = 0;
    uint64_t next_segment_seq = 1;
    std::array<uint8_t, 32> prior_event_hash{};
    std::array<uint8_t, 32> prior_segment_hash{};
    if (!prior_segments.empty())
    {
        const auto& latest = prior_segments.back();
        next_segment_seq = latest.segment_seq + 1;
        prior_segment_hash = latest.hash_curr;

        AuditExportManifestData prior_manifest;
        ErrorContext manifest_ctx;
        if (!parseAuditExportManifest(latest.payload_manifest, prior_manifest, &manifest_ctx))
        {
            const Status manifest_status =
                (manifest_ctx.code == Status::OK) ? Status::DATA_CORRUPTED : manifest_ctx.code;
            SET_ERROR_CONTEXT(ctx, manifest_status, manifest_ctx.message.c_str());
            return manifest_status;
        }
        last_exported_event_id = prior_manifest.last_event_id;
        if (!parseHexHash(prior_manifest.last_event_hash_curr, prior_event_hash))
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                              "Persisted audit export manifest carries an invalid last_event_hash_curr");
            return Status::DATA_CORRUPTED;
        }
    }

    AuditQuery query;
    query.limit = std::numeric_limits<uint32_t>::max();
    query.offset = 0;
    query.descending = false;

    std::vector<AuditEvent> events;
    status = queryAuditLog(query, events, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::vector<AuditEvent> export_events;
    export_events.reserve(events.size());
    for (const auto& event : events)
    {
        if (event.event_id > last_exported_event_id)
        {
            export_events.push_back(event);
        }
    }
    if (export_events.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No new audit events are available for export");
        return Status::NOT_FOUND;
    }

    std::array<uint8_t, 32> first_event_hash_prev = prior_event_hash;
    std::array<uint8_t, 32> running_hash = prior_event_hash;
    std::string payload_block;
    for (const auto& event : export_events)
    {
        const auto hash_curr = computeChainHash(event, running_hash);
        payload_block += serializeAuditEventJsonLine(event, running_hash, hash_curr);
        running_hash = hash_curr;
    }

    AuditExportManifestData manifest;
    manifest.segment_id = generateUuidV7();
    manifest.database_id = catalog_->database()->uuid();
    manifest.sink_profile_id = request.sink_profile_id;
    manifest.profile_name = profile.profile_name;
    manifest.sink_type = profile.sink_type;
    manifest.failure_policy = profile.failure_policy;
    manifest.evidence_class = request.evidence_class;
    manifest.segment_seq = next_segment_seq;
    manifest.range_start_time = export_events.front().timestamp;
    manifest.range_end_time = export_events.back().timestamp;
    manifest.event_count = static_cast<uint64_t>(export_events.size());
    manifest.first_event_id = export_events.front().event_id;
    manifest.last_event_id = export_events.back().event_id;
    manifest.first_event_hash_prev = hashToHex(first_event_hash_prev);
    manifest.last_event_hash_curr = hashToHex(running_hash);
    manifest.payload_sha256 = sha256Hex(payload_block);
    manifest.payload_bytes = static_cast<uint64_t>(payload_block.size());

    std::string manifest_block = buildAuditExportManifest(manifest);
    const auto segment_hash_curr = computeExportSegmentHash(manifest_block, prior_segment_hash);
    const std::string package_contents = manifest_block + payload_block;

    std::filesystem::path temp_path = output_path;
    temp_path += ".part";
    std::filesystem::remove(temp_path, ec);

    {
        std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to create audit export package");
            return Status::IO_ERROR;
        }
        out.write(package_contents.data(), static_cast<std::streamsize>(package_contents.size()));
        if (!out.good())
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write audit export package");
            return Status::IO_ERROR;
        }
    }

    CatalogManager::AuditExportSegmentCatalogInfo segment_info;
    segment_info.audit_export_segment_id = manifest.segment_id;
    segment_info.audit_sink_profile_id = request.sink_profile_id;
    segment_info.evidence_class = request.evidence_class;
    segment_info.segment_seq = next_segment_seq;
    segment_info.range_start_time = manifest.range_start_time;
    segment_info.range_end_time = manifest.range_end_time;
    segment_info.payload_manifest = manifest_block;
    segment_info.hash_prev = prior_segment_hash;
    segment_info.hash_curr = segment_hash_curr;
    segment_info.delivery_state = "LOCAL_COMMITTED";
    segment_info.is_valid = true;
    status = catalog_->appendAuditExportSegmentCatalogEntry(segment_info, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::filesystem::rename(temp_path, output_path, ec);
    if (ec)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to finalize audit export package");
        return Status::IO_ERROR;
    }

    result_out.segment_id = manifest.segment_id;
    result_out.output_path = output_path.string();
    result_out.segment_seq = next_segment_seq;
    result_out.event_count = manifest.event_count;
    result_out.first_event_id = manifest.first_event_id;
    result_out.last_event_id = manifest.last_event_id;
    result_out.range_start_time = manifest.range_start_time;
    result_out.range_end_time = manifest.range_end_time;
    result_out.payload_sha256 = manifest.payload_sha256;
    return Status::OK;
}

Status AuditLogger::validateAuditPackage(const std::string& package_path,
                                         AuditExportValidationResult& result_out,
                                         ErrorContext* ctx)
{
    result_out = AuditExportValidationResult{};

    if (!catalog_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Audit package validation requires catalog-backed persistence");
        return Status::INVALID_ARGUMENT;
    }
    if (package_path.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Audit package path is required");
        return Status::INVALID_ARGUMENT;
    }

    std::ifstream in(package_path, std::ios::binary);
    if (!in.is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Audit export package file not found");
        return Status::FILE_NOT_FOUND;
    }
    std::string package_contents((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());

    std::string manifest_block;
    std::string payload_block;
    if (!splitAuditExportPackage(package_contents, manifest_block, payload_block, ctx))
    {
        return ctx && ctx->code != Status::OK ? ctx->code : Status::DATA_CORRUPTED;
    }

    AuditExportManifestData manifest;
    if (!parseAuditExportManifest(manifest_block, manifest, ctx))
    {
        return ctx && ctx->code != Status::OK ? ctx->code : Status::DATA_CORRUPTED;
    }

    CatalogManager::AuditExportSegmentCatalogInfo segment;
    Status status = catalog_->getAuditExportSegmentCatalogEntry(manifest.segment_id, segment, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    result_out.segment_id = manifest.segment_id;
    result_out.event_count = manifest.event_count;
    result_out.manifest_matches_catalog = (segment.payload_manifest == manifest_block);
    result_out.payload_checksum_valid =
        (manifest.payload_sha256 == sha256Hex(payload_block) &&
         manifest.payload_bytes == payload_block.size());
    result_out.package_valid = result_out.manifest_matches_catalog && result_out.payload_checksum_valid;

    if (!result_out.manifest_matches_catalog)
    {
        result_out.failure_reason = "audit export manifest does not match persisted catalog truth";
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, result_out.failure_reason.c_str());
        return Status::DATA_CORRUPTED;
    }
    if (!result_out.payload_checksum_valid)
    {
        result_out.failure_reason = "audit export payload checksum does not match manifest";
        SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH, result_out.failure_reason.c_str());
        return Status::CHECKSUM_MISMATCH;
    }

    return Status::OK;
}

Status AuditLogger::setAuditLegalHold(const AuditLegalHoldCommand& command,
                                      AuditLegalHoldResult& result_out,
                                      ErrorContext* ctx)
{
    result_out = AuditLegalHoldResult{};
    if (!catalog_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Audit legal hold requires catalog-backed persistence");
        return Status::INVALID_ARGUMENT;
    }
    if (isZeroId(command.sink_profile_id))
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "audit legal hold requires sink_profile_id");
        return Status::INVALID_ARGUMENT;
    }
    if (command.enable_hold && command.reason.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "audit legal hold enable requires reason");
        return Status::INVALID_ARGUMENT;
    }

    CatalogManager::AuditSinkProfileCatalogInfo profile;
    Status status = catalog_->getAuditSinkProfileCatalogEntry(command.sink_profile_id, profile, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    AuditGovernancePolicy policy;
    if (!loadAuditGovernancePolicy(profile.config_json, policy, ctx))
    {
        result_out.reject_code = "AUDIT_POLICY_INVALID";
        return ctx && ctx->code != Status::OK ? ctx->code : Status::DATA_CORRUPTED;
    }

    const uint64_t event_time = command.event_time == 0 ? currentSystemTicks() : command.event_time;
    policy.policy_version += 1;
    policy.legal_hold_active = command.enable_hold;
    if (command.enable_hold)
    {
        policy.legal_hold_reason = command.reason;
        policy.legal_hold_actor = command.actor;
        policy.legal_hold_set_time = event_time;
        policy.legal_hold_release_time = 0;
    }
    else
    {
        policy.legal_hold_release_time = event_time;
    }
    storeAuditGovernancePolicy(policy, profile.config_json);

    status = catalog_->upsertAuditSinkProfileCatalogEntry(profile, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::unordered_map<std::string, std::string> manifest_fields{
        {"policy_version", std::to_string(policy.policy_version)},
        {"legal_hold_active", command.enable_hold ? "true" : "false"},
        {"legal_hold_actor", command.actor},
        {"legal_hold_reason", command.reason}
    };
    status = appendGovernanceSegment(
        command.sink_profile_id,
        command.enable_hold ? "LEGAL_HOLD_ENABLED" : "LEGAL_HOLD_RELEASED",
        event_time,
        manifest_fields,
        result_out.evidence_segment_id,
        ctx);
    if (status != Status::OK)
    {
        return status;
    }

    result_out.legal_hold_active = policy.legal_hold_active;
    result_out.policy_version = policy.policy_version;
    result_out.event_time = event_time;
    return Status::OK;
}

Status AuditLogger::evaluateRetentionPolicy(const AuditRetentionEvaluationRequest& request,
                                            AuditRetentionEvaluationResult& result_out,
                                            ErrorContext* ctx)
{
    result_out = AuditRetentionEvaluationResult{};
    if (!catalog_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Audit retention evaluation requires catalog-backed persistence");
        return Status::INVALID_ARGUMENT;
    }
    if (isZeroId(request.sink_profile_id))
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "audit retention evaluation requires sink_profile_id");
        return Status::INVALID_ARGUMENT;
    }

    CatalogManager::AuditSinkProfileCatalogInfo profile;
    Status status = catalog_->getAuditSinkProfileCatalogEntry(request.sink_profile_id, profile, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    AuditGovernancePolicy policy;
    if (!loadAuditGovernancePolicy(profile.config_json, policy, ctx))
    {
        result_out.reject_code = "AUDIT_POLICY_INVALID";
        return ctx && ctx->code != Status::OK ? ctx->code : Status::DATA_CORRUPTED;
    }

    std::vector<CatalogManager::AuditExportSegmentCatalogInfo> segments;
    status = catalog_->listAuditExportSegmentCatalogEntries(request.sink_profile_id, segments, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    const uint64_t now_time = request.now_time == 0 ? currentSystemTicks() : request.now_time;
    const uint64_t archive_retention_ticks = retentionDaysToSystemTicks(policy.archive_retention_days);

    result_out.legal_hold_active = policy.legal_hold_active;
    result_out.policy_version = policy.policy_version;
    result_out.hot_retention_days = policy.hot_retention_days;
    result_out.archive_retention_days = policy.archive_retention_days;
    result_out.segments_examined = static_cast<uint64_t>(segments.size());

    for (const auto& segment : segments)
    {
        const bool age_satisfied =
            segment.created_time > 0 &&
            now_time >= segment.created_time &&
            (now_time - segment.created_time) >= archive_retention_ticks;
        if (!policy.legal_hold_active && age_satisfied)
        {
            result_out.eligible_segment_ids.push_back(segment.audit_export_segment_id);
        }
        else
        {
            result_out.blocked_segment_ids.push_back(segment.audit_export_segment_id);
        }
    }

    result_out.segments_eligible = static_cast<uint64_t>(result_out.eligible_segment_ids.size());
    result_out.segments_blocked = static_cast<uint64_t>(result_out.blocked_segment_ids.size());

    if (request.append_evidence)
    {
        std::unordered_map<std::string, std::string> manifest_fields{
            {"policy_version", std::to_string(policy.policy_version)},
            {"hot_retention_days", std::to_string(policy.hot_retention_days)},
            {"archive_retention_days", std::to_string(policy.archive_retention_days)},
            {"legal_hold_active", policy.legal_hold_active ? "true" : "false"},
            {"requested_by", request.requested_by},
            {"segments_examined", std::to_string(result_out.segments_examined)},
            {"segments_eligible", std::to_string(result_out.segments_eligible)},
            {"segments_blocked", std::to_string(result_out.segments_blocked)},
            {"eligible_segment_ids", joinUuidList(result_out.eligible_segment_ids)},
            {"blocked_segment_ids", joinUuidList(result_out.blocked_segment_ids)}
        };
        status = appendGovernanceSegment(request.sink_profile_id,
                                         "RETENTION_POLICY_DECISION",
                                         now_time,
                                         manifest_fields,
                                         result_out.evidence_segment_id,
                                         ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    return Status::OK;
}

Status AuditLogger::appendGovernanceSegment(const ID& sink_profile_id,
                                            const std::string& evidence_class,
                                            uint64_t range_time,
                                            const std::unordered_map<std::string, std::string>& extra_fields,
                                            ID& segment_id_out,
                                            ErrorContext* ctx)
{
    segment_id_out = ID{};

    CatalogManager::AuditSinkProfileCatalogInfo profile;
    Status status = catalog_->getAuditSinkProfileCatalogEntry(sink_profile_id, profile, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::vector<CatalogManager::AuditExportSegmentCatalogInfo> prior_segments;
    status = catalog_->listAuditExportSegmentCatalogEntries(sink_profile_id, prior_segments, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint64_t next_segment_seq = 1;
    std::array<uint8_t, 32> prior_segment_hash{};
    if (!prior_segments.empty())
    {
        const auto& latest = prior_segments.back();
        next_segment_seq = latest.segment_seq + 1;
        prior_segment_hash = latest.hash_curr;
    }

    AuditExportManifestData manifest;
    manifest.segment_id = generateUuidV7();
    manifest.database_id = catalog_->database()->uuid();
    manifest.sink_profile_id = sink_profile_id;
    manifest.profile_name = profile.profile_name;
    manifest.sink_type = profile.sink_type;
    manifest.failure_policy = profile.failure_policy;
    manifest.evidence_class = evidence_class;
    manifest.segment_seq = next_segment_seq;
    manifest.range_start_time = range_time;
    manifest.range_end_time = range_time;
    manifest.event_count = 0;
    manifest.first_event_id = 0;
    manifest.last_event_id = 0;
    manifest.first_event_hash_prev.assign(64, '0');
    manifest.last_event_hash_curr.assign(64, '0');
    manifest.payload_sha256 = sha256Hex("");
    manifest.payload_bytes = 0;

    std::string manifest_block = buildAuditExportManifest(manifest);
    manifest_block = appendManifestFields(manifest_block, extra_fields);
    const auto segment_hash_curr = computeExportSegmentHash(manifest_block, prior_segment_hash);

    CatalogManager::AuditExportSegmentCatalogInfo segment_info;
    segment_info.audit_export_segment_id = manifest.segment_id;
    segment_info.audit_sink_profile_id = sink_profile_id;
    segment_info.evidence_class = evidence_class;
    segment_info.segment_seq = next_segment_seq;
    segment_info.range_start_time = range_time;
    segment_info.range_end_time = range_time;
    segment_info.payload_manifest = manifest_block;
    segment_info.hash_prev = prior_segment_hash;
    segment_info.hash_curr = segment_hash_curr;
    segment_info.delivery_state = "LOCAL_COMMITTED";
    segment_info.is_valid = true;
    status = catalog_->appendAuditExportSegmentCatalogEntry(segment_info, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    segment_id_out = manifest.segment_id;
    return Status::OK;
}

Status AuditLogger::flushUnlocked(ErrorContext* ctx)
{
    // Internal flush method - caller must hold mutex_
    size_t start_index = flush_cursor_;

    for (size_t i = start_index; i < buffer_.size(); ++i)
    {
        const auto& entry = buffer_[i];
        Status status = writeEventToCatalog(entry, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = writeEventToFile(entry, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        broadcastEvent(entry.event);
    }

    if (sink_config_.keep_in_memory)
    {
        flush_cursor_ = buffer_.size();
    }
    else
    {
        buffer_.clear();
        flush_cursor_ = 0;
    }

    return Status::OK;
}

Status AuditLogger::flush(ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return flushUnlocked(ctx);
}

Status AuditLogger::queryAuditLog(
    const AuditQuery& query,
    std::vector<AuditEvent>& events_out,
    ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    events_out.clear();

    std::vector<AuditEvent> matching_events;
    bool use_catalog = sink_config_.enable_catalog && catalog_;

    if (use_catalog)
    {
        AuditQuery catalog_query = query;
        catalog_query.offset = 0;
        catalog_query.limit = std::numeric_limits<uint32_t>::max();

        std::vector<AuditEvent> catalog_events;
        Status status = catalog_->queryAuditLog(catalog_query, catalog_events, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        matching_events.insert(matching_events.end(), catalog_events.begin(),
                               catalog_events.end());
    }

    size_t start_index = use_catalog ? flush_cursor_ : 0;
    for (size_t i = start_index; i < buffer_.size(); ++i)
    {
        const auto& event = buffer_[i].event;
        if (matchesQuery(event, query))
        {
            matching_events.push_back(event);
        }
    }

    if (query.descending)
    {
        std::sort(matching_events.begin(), matching_events.end(),
                  [](const AuditEvent& a, const AuditEvent& b) {
                      if (a.timestamp != b.timestamp)
                      {
                          return a.timestamp > b.timestamp;
                      }
                      return a.event_id > b.event_id;
                  });
    }
    else
    {
        std::sort(matching_events.begin(), matching_events.end(),
                  [](const AuditEvent& a, const AuditEvent& b) {
                      if (a.timestamp != b.timestamp)
                      {
                          return a.timestamp < b.timestamp;
                      }
                      return a.event_id < b.event_id;
                  });
    }

    size_t start_idx = std::min(static_cast<size_t>(query.offset), matching_events.size());
    size_t end_idx = std::min(start_idx + query.limit, matching_events.size());
    events_out.reserve(end_idx - start_idx);
    for (size_t i = start_idx; i < end_idx; ++i)
    {
        events_out.push_back(matching_events[i]);
    }

    return Status::OK;
}

uint64_t AuditLogger::getTotalEventCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return next_event_id_ - 1;
}

std::string AuditLogger::getEventTypeName(AuditEventType type)
{
    switch (type) {
        // Authentication
        case AuditEventType::LOGIN_SUCCESS:        return "LOGIN_SUCCESS";
        case AuditEventType::LOGIN_FAILURE:        return "LOGIN_FAILURE";
        case AuditEventType::LOGOUT:               return "LOGOUT";
        case AuditEventType::PASSWORD_CHANGE:      return "PASSWORD_CHANGE";
        case AuditEventType::PASSWORD_RESET:       return "PASSWORD_RESET";
        case AuditEventType::ACCOUNT_LOCKED:       return "ACCOUNT_LOCKED";
        case AuditEventType::ACCOUNT_UNLOCKED:     return "ACCOUNT_UNLOCKED";
        case AuditEventType::BOOTSTRAP_ATTEMPT:    return "BOOTSTRAP_ATTEMPT";
        case AuditEventType::BOOTSTRAP_SUCCESS:    return "BOOTSTRAP_SUCCESS";
        case AuditEventType::BOOTSTRAP_FAILURE:    return "BOOTSTRAP_FAILURE";
        case AuditEventType::BOOTSTRAP_REVOKED:    return "BOOTSTRAP_REVOKED";
        case AuditEventType::REATTACH_TOKEN_ISSUED:return "REATTACH_TOKEN_ISSUED";
        case AuditEventType::REATTACH_SUCCESS:     return "REATTACH_SUCCESS";
        case AuditEventType::REATTACH_FAILURE:     return "REATTACH_FAILURE";
        case AuditEventType::REATTACH_TOKEN_REVOKED:return "REATTACH_TOKEN_REVOKED";
        case AuditEventType::AUTH_POLICY_DECISION: return "AUTH_POLICY_DECISION";
        case AuditEventType::TOKEN_AUTH_USED:      return "TOKEN_AUTH_USED";
        case AuditEventType::TOKEN_AUTH_REVOKED:   return "TOKEN_AUTH_REVOKED";
        case AuditEventType::MANAGED_PREFACE_DECISION:
            return "MANAGED_PREFACE_DECISION";
        case AuditEventType::MANAGED_DBBT_ISSUED:  return "MANAGED_DBBT_ISSUED";

        // Authorization
        case AuditEventType::PERMISSION_GRANTED:   return "PERMISSION_GRANTED";
        case AuditEventType::PERMISSION_REVOKED:   return "PERMISSION_REVOKED";
        case AuditEventType::PERMISSION_DENIED:    return "PERMISSION_DENIED";
        case AuditEventType::ROLE_GRANTED:         return "ROLE_GRANTED";
        case AuditEventType::ROLE_REVOKED:         return "ROLE_REVOKED";

        // User Management
        case AuditEventType::USER_CREATED:         return "USER_CREATED";
        case AuditEventType::USER_DELETED:         return "USER_DELETED";
        case AuditEventType::USER_MODIFIED:        return "USER_MODIFIED";
        case AuditEventType::USER_ENABLED:         return "USER_ENABLED";
        case AuditEventType::USER_DISABLED:        return "USER_DISABLED";
        case AuditEventType::ROLE_CREATED:         return "ROLE_CREATED";
        case AuditEventType::ROLE_DELETED:         return "ROLE_DELETED";
        case AuditEventType::ROLE_MODIFIED:        return "ROLE_MODIFIED";

        // Data Access
        case AuditEventType::RLS_VIOLATION:        return "RLS_VIOLATION";
        case AuditEventType::COLUMN_ACCESS_DENIED: return "COLUMN_ACCESS_DENIED";
        case AuditEventType::TABLE_ACCESS_DENIED:  return "TABLE_ACCESS_DENIED";
        case AuditEventType::SCHEMA_ACCESS_DENIED: return "SCHEMA_ACCESS_DENIED";
        case AuditEventType::DOMAIN_ACCESS:        return "DOMAIN_ACCESS";

        // Privilege Escalation
        case AuditEventType::SUPERUSER_ACCESS:     return "SUPERUSER_ACCESS";
        case AuditEventType::SET_ROLE:             return "SET_ROLE";
        case AuditEventType::IMPERSONATION:        return "IMPERSONATION";

        // DDL Operations
        case AuditEventType::DDL_CREATE:           return "DDL_CREATE";
        case AuditEventType::DDL_ALTER:            return "DDL_ALTER";
        case AuditEventType::DDL_DROP:             return "DDL_DROP";
        case AuditEventType::DDL_TRUNCATE:         return "DDL_TRUNCATE";

        // System Events
        case AuditEventType::DATABASE_STARTUP:     return "DATABASE_STARTUP";
        case AuditEventType::DATABASE_SHUTDOWN:    return "DATABASE_SHUTDOWN";
        case AuditEventType::BACKUP_STARTED:       return "BACKUP_STARTED";
        case AuditEventType::BACKUP_COMPLETED:     return "BACKUP_COMPLETED";
        case AuditEventType::BACKUP_FAILED:        return "BACKUP_FAILED";
        case AuditEventType::RESTORE_STARTED:      return "RESTORE_STARTED";
        case AuditEventType::RESTORE_COMPLETED:    return "RESTORE_COMPLETED";
        case AuditEventType::RESTORE_FAILED:       return "RESTORE_FAILED";

        // Security Configuration
        case AuditEventType::SECURITY_POLICY_CHANGED: return "SECURITY_POLICY_CHANGED";
        case AuditEventType::AUDIT_LOG_ACCESSED:      return "AUDIT_LOG_ACCESSED";
        case AuditEventType::AUDIT_LOG_MODIFIED:      return "AUDIT_LOG_MODIFIED";
        case AuditEventType::ENCRYPTION_KEY_CHANGED:  return "ENCRYPTION_KEY_CHANGED";

        // Job Scheduler
        case AuditEventType::JOB_CREATED:         return "JOB_CREATED";
        case AuditEventType::JOB_MODIFIED:        return "JOB_MODIFIED";
        case AuditEventType::JOB_DELETED:         return "JOB_DELETED";
        case AuditEventType::JOB_EXECUTED:        return "JOB_EXECUTED";
        case AuditEventType::JOB_FAILED:          return "JOB_FAILED";
        case AuditEventType::JOB_CANCELLED:       return "JOB_CANCELLED";

        default:                                   return "UNKNOWN";
    }
}

// ===== Helper Functions =====

AuditEvent AuditLogger::createLoginSuccessEvent(
    const ID& user_id,
    const std::string& username)
{
    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;
    event.user_id = user_id;
    event.username = username;
    event.success = true;
    event.details = "{}";
    return event;
}

AuditEvent AuditLogger::createLoginFailureEvent(
    const std::string& username,
    const std::string& reason)
{
    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_FAILURE;
    event.username = username;
    event.success = false;
    event.details = "{\"reason\":\"" + reason + "\"}";
    return event;
}

AuditEvent AuditLogger::createPermissionDeniedEvent(
    const ID& user_id,
    const std::string& username,
    const std::string& object_type,
    const std::string& object_name,
    const std::string& permission)
{
    AuditEvent event;
    event.event_type = AuditEventType::PERMISSION_DENIED;
    event.user_id = user_id;
    event.username = username;
    event.object_type = object_type;
    event.object_name = object_name;
    event.success = false;
    event.details = "{\"permission\":\"" + permission + "\"}";
    return event;
}

AuditEvent AuditLogger::createUserCreatedEvent(
    const ID& creator_user_id,
    const std::string& creator_username,
    const ID& new_user_id,
    const std::string& new_username)
{
    AuditEvent event;
    event.event_type = AuditEventType::USER_CREATED;
    event.user_id = creator_user_id;
    event.username = creator_username;
    event.target_user_id = new_user_id;
    event.target_username = new_username;
    event.object_type = "USER";
    event.object_name = new_username;
    event.success = true;
    event.details = "{}";
    return event;
}

}  // namespace core
}  // namespace scratchbird
