/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/gc_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/heap_toast_lob_diagnostics.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "scratchbird/core/config.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <algorithm>
#include <cstring>

namespace scratchbird::core
{
    namespace
    {
        constexpr const char* kSweepEvidenceProfileName = "__sweep_local_evidence__";
        constexpr const char* kSweepManifestMagic = "SB_SWEEP_EVIDENCE_MANIFEST_v1";
        constexpr const char* kWalAfterLogProfileName = "__sweep_wal_after_log__";
        constexpr const char* kWalAfterLogManifestMagic = "SB_WAL_AFTER_LOG_SEGMENT_v1";
        constexpr const char* kShadowCaptureProfileName = "__sweep_shadow_capture__";
        constexpr const char* kShadowCaptureManifestMagic = "SB_SHADOW_CAPTURE_MANIFEST_v1";
        // Startup reconciliation owns reserved slots 0..14 on the system-state
        // bootstrap page. Sweep progress must live in its own non-overlapping
        // slot range so a fresh database never mistakes startup metadata for an
        // in-flight sweep checkpoint.
        constexpr size_t kSweepProgressSlotGeneration = 16;
        constexpr size_t kSweepProgressSlotActive = 17;
        constexpr size_t kSweepProgressSlotStartHorizon = 18;
        constexpr size_t kSweepProgressSlotRelationHi = 19;
        constexpr size_t kSweepProgressSlotRelationLo = 20;
        constexpr size_t kSweepProgressSlotPageCursor = 21;
        constexpr size_t kSweepProgressSlotReclaimedVersions = 22;
        constexpr size_t kSweepProgressSlotReclaimedBytes = 23;
        constexpr size_t kSweepProgressSlotIndexBacklog = 24;
        constexpr size_t kSweepProgressSlotResumeMeta = 25;
        constexpr uint64_t kSweepProgressStageShift = 60;
        constexpr uint64_t kSweepProgressLaneShift = 52;
        constexpr uint64_t kSweepProgressStrictShift = 51;
        constexpr uint64_t kSweepProgressLaneMask = 0xFFULL;
        constexpr uint64_t kSweepProgressResumeOitMask =
            (uint64_t{1} << kSweepProgressStrictShift) - 1;

        bool isZeroIdLocal(const ID& id)
        {
            for (uint8_t byte : id.bytes)
            {
                if (byte != 0)
                {
                    return false;
                }
            }
            return true;
        }

        uint64_t currentSystemMicros()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                .count();
        }

        auto decodeSweepProgressStage(uint64_t raw_stage) -> SweepProgressStage
        {
            switch (raw_stage)
            {
            case static_cast<uint64_t>(SweepProgressStage::NONE):
                return SweepProgressStage::NONE;
            case static_cast<uint64_t>(SweepProgressStage::LOCAL_EVIDENCE_PENDING):
                return SweepProgressStage::LOCAL_EVIDENCE_PENDING;
            case static_cast<uint64_t>(SweepProgressStage::PAGE_AUDIT_PENDING):
                return SweepProgressStage::PAGE_AUDIT_PENDING;
            case static_cast<uint64_t>(SweepProgressStage::SHADOW_CAPTURE_PENDING):
                return SweepProgressStage::SHADOW_CAPTURE_PENDING;
            case static_cast<uint64_t>(SweepProgressStage::WAL_AFTER_PENDING):
                return SweepProgressStage::WAL_AFTER_PENDING;
            case static_cast<uint64_t>(SweepProgressStage::RECLAIM_PENDING):
                return SweepProgressStage::RECLAIM_PENDING;
            default:
                return SweepProgressStage::NONE;
            }
        }

        auto encodeSweepPolicyLaneMask(const std::vector<SweepPolicyLane>& lanes) -> uint16_t
        {
            uint16_t mask = 0;
            for (SweepPolicyLane lane : lanes)
            {
                const uint8_t bit = static_cast<uint8_t>(lane);
                if (bit < 16)
                {
                    mask |= static_cast<uint16_t>(uint16_t{1} << bit);
                }
            }
            return mask;
        }

        auto hasSweepPolicyLaneInMask(uint16_t mask, SweepPolicyLane lane) -> bool
        {
            const uint8_t bit = static_cast<uint8_t>(lane);
            return bit < 16 && (mask & static_cast<uint16_t>(uint16_t{1} << bit)) != 0;
        }

        auto decodeSweepPolicyLaneMask(uint16_t mask) -> std::vector<SweepPolicyLane>
        {
            std::vector<SweepPolicyLane> lanes;
            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                if ((mask & static_cast<uint16_t>(uint16_t{1} << bit)) != 0)
                {
                    lanes.push_back(static_cast<SweepPolicyLane>(bit));
                }
            }
            if (lanes.empty())
            {
                lanes.push_back(SweepPolicyLane::NORMAL);
            }
            return lanes;
        }

        auto packSweepProgressResumeMeta(SweepProgressStage stage,
                                         uint16_t lane_mask,
                                         bool strict_audit,
                                         uint64_t resume_oit_before)
            -> uint64_t
        {
            return (static_cast<uint64_t>(stage) << kSweepProgressStageShift) |
                   ((static_cast<uint64_t>(lane_mask) & kSweepProgressLaneMask)
                    << kSweepProgressLaneShift) |
                   ((strict_audit ? 1ULL : 0ULL) << kSweepProgressStrictShift) |
                   (resume_oit_before & kSweepProgressResumeOitMask);
        }

        void unpackSweepProgressResumeMeta(uint64_t packed,
                                           SweepProgressStage* stage_out,
                                           uint16_t* lane_mask_out,
                                           bool* strict_audit_out,
                                           uint64_t* resume_oit_before_out)
        {
            if (stage_out != nullptr)
            {
                *stage_out =
                    decodeSweepProgressStage((packed >> kSweepProgressStageShift) & 0x0FULL);
            }
            if (lane_mask_out != nullptr)
            {
                *lane_mask_out = static_cast<uint16_t>((packed >> kSweepProgressLaneShift) &
                                                       kSweepProgressLaneMask);
            }
            if (strict_audit_out != nullptr)
            {
                *strict_audit_out = ((packed >> kSweepProgressStrictShift) & 0x1ULL) != 0;
            }
            if (resume_oit_before_out != nullptr)
            {
                *resume_oit_before_out = packed & kSweepProgressResumeOitMask;
            }
        }

        void encodeIdToSlots(const ID& id, uint64_t& hi_out, uint64_t& lo_out)
        {
            std::memcpy(&hi_out, id.bytes.data(), sizeof(uint64_t));
            std::memcpy(&lo_out, id.bytes.data() + sizeof(uint64_t), sizeof(uint64_t));
        }

        ID decodeIdFromSlots(uint64_t hi, uint64_t lo)
        {
            ID id{};
            std::memcpy(id.bytes.data(), &hi, sizeof(uint64_t));
            std::memcpy(id.bytes.data() + sizeof(uint64_t), &lo, sizeof(uint64_t));
            return id;
        }

        std::string hashBytesToHex(const std::array<uint8_t, 32>& hash)
        {
            static constexpr char kHex[] = "0123456789abcdef";
            std::string out;
            out.resize(hash.size() * 2);
            for (size_t i = 0; i < hash.size(); ++i)
            {
                out[i * 2] = kHex[(hash[i] >> 4) & 0x0F];
                out[(i * 2) + 1] = kHex[hash[i] & 0x0F];
            }
            return out;
        }

        bool parseUuidFromString(const std::string& text, ID& out)
        {
            std::string hex;
            hex.reserve(32);
            for (char c : text)
            {
                if (c == '-')
                {
                    continue;
                }
                if ((c >= '0' && c <= '9') ||
                    (c >= 'a' && c <= 'f') ||
                    (c >= 'A' && c <= 'F'))
                {
                    hex.push_back(c);
                }
                else
                {
                    return false;
                }
            }
            if (hex.size() != 32)
            {
                return false;
            }

            auto nibble = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
                if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
                return static_cast<uint8_t>(10 + (c - 'A'));
            };

            for (size_t i = 0; i < 16; ++i)
            {
                out.bytes[i] = static_cast<uint8_t>(
                    (nibble(hex[i * 2]) << 4) | nibble(hex[(i * 2) + 1]));
            }
            return true;
        }

        std::string sweepPolicyLaneToString(SweepPolicyLane lane)
        {
            switch (lane)
            {
                case SweepPolicyLane::NORMAL: return "NORMAL";
                case SweepPolicyLane::LINEAGE_RETENTION: return "LINEAGE_RETENTION";
                case SweepPolicyLane::OBJECT_TOUCH_AUDIT: return "OBJECT_TOUCH_AUDIT";
                case SweepPolicyLane::SCHEMA_CHANGE_AUDIT: return "SCHEMA_CHANGE_AUDIT";
                case SweepPolicyLane::WAL_AFTER_EXPORT: return "WAL_AFTER_EXPORT";
                case SweepPolicyLane::PAGE_SPOT_AUDIT: return "PAGE_SPOT_AUDIT";
                case SweepPolicyLane::SHADOW_CAPTURE: return "SHADOW_CAPTURE";
                case SweepPolicyLane::COMPOSITE: return "COMPOSITE";
            }
            return "NORMAL";
        }

        auto parseSweepPolicyLane(const std::string& text, SweepPolicyLane& lane_out) -> bool
        {
            if (text == "NORMAL")
            {
                lane_out = SweepPolicyLane::NORMAL;
                return true;
            }
            if (text == "LINEAGE_RETENTION")
            {
                lane_out = SweepPolicyLane::LINEAGE_RETENTION;
                return true;
            }
            if (text == "OBJECT_TOUCH_AUDIT")
            {
                lane_out = SweepPolicyLane::OBJECT_TOUCH_AUDIT;
                return true;
            }
            if (text == "SCHEMA_CHANGE_AUDIT")
            {
                lane_out = SweepPolicyLane::SCHEMA_CHANGE_AUDIT;
                return true;
            }
            if (text == "WAL_AFTER_EXPORT")
            {
                lane_out = SweepPolicyLane::WAL_AFTER_EXPORT;
                return true;
            }
            if (text == "PAGE_SPOT_AUDIT")
            {
                lane_out = SweepPolicyLane::PAGE_SPOT_AUDIT;
                return true;
            }
            if (text == "SHADOW_CAPTURE")
            {
                lane_out = SweepPolicyLane::SHADOW_CAPTURE;
                return true;
            }
            if (text == "COMPOSITE")
            {
                lane_out = SweepPolicyLane::COMPOSITE;
                return true;
            }
            return false;
        }

        std::string encodeSweepPolicyLanes(const std::vector<SweepPolicyLane>& lanes)
        {
            std::string out;
            for (size_t i = 0; i < lanes.size(); ++i)
            {
                if (i > 0)
                {
                    out += ",";
                }
                out += sweepPolicyLaneToString(lanes[i]);
            }
            return out;
        }

        auto parseSweepPolicyLanes(const std::string& csv,
                                   std::vector<SweepPolicyLane>& lanes_out) -> bool
        {
            lanes_out.clear();
            std::stringstream in(csv);
            std::string token;
            while (std::getline(in, token, ','))
            {
                if (token.empty())
                {
                    continue;
                }
                SweepPolicyLane lane = SweepPolicyLane::NORMAL;
                if (!parseSweepPolicyLane(token, lane))
                {
                    return false;
                }
                lanes_out.push_back(lane);
            }
            return true;
        }

        bool lanesRequireLocalEvidence(const std::vector<SweepPolicyLane>& lanes)
        {
            return !(lanes.empty() ||
                     (lanes.size() == 1 && lanes.front() == SweepPolicyLane::NORMAL));
        }

        bool lanesRequireDownstreamQueue(const std::vector<SweepPolicyLane>& lanes)
        {
            return std::find(lanes.begin(), lanes.end(), SweepPolicyLane::WAL_AFTER_EXPORT) !=
                       lanes.end() ||
                   std::find(lanes.begin(), lanes.end(), SweepPolicyLane::PAGE_SPOT_AUDIT) !=
                       lanes.end() ||
                   std::find(lanes.begin(), lanes.end(), SweepPolicyLane::SHADOW_CAPTURE) !=
                       lanes.end() ||
                   std::find(lanes.begin(), lanes.end(), SweepPolicyLane::COMPOSITE) !=
                       lanes.end();
        }

        bool hasSweepPolicyLane(const std::vector<SweepPolicyLane>& lanes, SweepPolicyLane lane)
        {
            return std::find(lanes.begin(), lanes.end(), lane) != lanes.end();
        }

        std::string chooseEvidenceClass(const std::vector<SweepPolicyLane>& lanes)
        {
            if (std::find(lanes.begin(), lanes.end(), SweepPolicyLane::SCHEMA_CHANGE_AUDIT) !=
                lanes.end())
            {
                return "TX_DDL_EVENT";
            }
            if (std::find(lanes.begin(), lanes.end(), SweepPolicyLane::OBJECT_TOUCH_AUDIT) !=
                lanes.end())
            {
                return "TX_OBJECT_TOUCH";
            }
            return "TX_LINEAGE_SUMMARY";
        }

        std::filesystem::path buildSweepSpoolRoot(const Database* db)
        {
            std::filesystem::path root(db ? db->path() : std::string());
            root += ".forensics";
            root /= "sweep_spool";
            return root;
        }

        std::filesystem::path buildWalAfterLogRoot(const Database* db)
        {
            std::filesystem::path root(db ? db->path() : std::string());
            root += ".forensics";
            root /= "wal_after_log";
            return root;
        }

        std::filesystem::path buildShadowCaptureRoot(const Database* db)
        {
            std::filesystem::path root(db ? db->path() : std::string());
            root += ".forensics";
            root /= "shadow_capture";
            return root;
        }

        auto ensureSweepSpoolRoot(const Database* db,
                                  std::filesystem::path& root_out,
                                  ErrorContext* ctx) -> Status
        {
            if (!db)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Sweep spool requires an open database");
                return Status::INVALID_ARGUMENT;
            }

            root_out = buildSweepSpoolRoot(db);
            std::error_code ec;
            std::filesystem::create_directories(root_out, ec);
            if (ec)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "SWEEP_EVIDENCE_LOCAL_PERSIST_FAILED: failed to create sweep spool directory");
                return Status::IO_ERROR;
            }
            return Status::OK;
        }

        auto ensureWalAfterLogRoot(const Database* db,
                                   std::filesystem::path& root_out,
                                   ErrorContext* ctx) -> Status
        {
            if (!db)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "wal_after_log sink requires an open database");
                return Status::INVALID_ARGUMENT;
            }

            root_out = buildWalAfterLogRoot(db);
            std::error_code ec;
            std::filesystem::create_directories(root_out, ec);
            if (ec)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                                  "WAL_AFTER_EXPORT_PERSIST_FAILED: failed to create wal_after_log directory");
                return Status::IO_ERROR;
            }
            return Status::OK;
        }

        auto ensureShadowCaptureRoot(const Database* db,
                                     std::filesystem::path& root_out,
                                     ErrorContext* ctx) -> Status
        {
            if (!db)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "shadow capture requires an open database");
                return Status::INVALID_ARGUMENT;
            }

            root_out = buildShadowCaptureRoot(db);
            std::error_code ec;
            std::filesystem::create_directories(root_out, ec);
            if (ec)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                                  "SWEEP_SHADOW_CAPTURE_BLOCKED: failed to create shadow_capture directory");
                return Status::IO_ERROR;
            }
            return Status::OK;
        }

        auto writeDurableTextFile(const std::filesystem::path& path,
                                  const std::string& contents,
                                  ErrorContext* ctx) -> Status
        {
            std::error_code ec;
            std::filesystem::path tmp_path = path;
            tmp_path += ".part";
            std::filesystem::remove(tmp_path, ec);

            {
                std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
                if (!out.is_open())
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "SWEEP_EVIDENCE_LOCAL_PERSIST_FAILED: failed to create sweep spool manifest");
                    return Status::IO_ERROR;
                }
                out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
                if (!out.good())
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "SWEEP_EVIDENCE_LOCAL_PERSIST_FAILED: failed to write sweep spool manifest");
                    return Status::IO_ERROR;
                }
            }

            std::filesystem::rename(tmp_path, path, ec);
            if (ec)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "SWEEP_EVIDENCE_LOCAL_PERSIST_FAILED: failed to finalize sweep spool manifest");
                return Status::IO_ERROR;
            }
            return Status::OK;
        }

        bool extractManifestField(const std::string& manifest,
                                  const std::string& key,
                                  std::string& value_out)
        {
            const std::string prefix = key + "=";
            size_t start = manifest.find(prefix);
            if (start == std::string::npos)
            {
                return false;
            }
            start += prefix.size();
            size_t end = manifest.find('\n', start);
            if (end == std::string::npos)
            {
                end = manifest.size();
            }
            value_out.assign(manifest.data() + start, end - start);
            return true;
        }

        bool extractManifestUint64Field(const std::string& manifest,
                                        const std::string& key,
                                        uint64_t& value_out)
        {
            std::string text;
            if (!extractManifestField(manifest, key, text))
            {
                return false;
            }
            value_out = std::strtoull(text.c_str(), nullptr, 10);
            return true;
        }

        std::string transactionLineageEventKindToString(
            CatalogManager::TransactionLineageEventKind kind)
        {
            switch (kind)
            {
                case CatalogManager::TransactionLineageEventKind::TX_BEGIN:
                    return "TX_BEGIN";
                case CatalogManager::TransactionLineageEventKind::TX_CONTEXT_BOUND:
                    return "TX_CONTEXT_BOUND";
                case CatalogManager::TransactionLineageEventKind::TX_MUTATION_BATCH:
                    return "TX_MUTATION_BATCH";
                case CatalogManager::TransactionLineageEventKind::TX_DDL_BATCH:
                    return "TX_DDL_BATCH";
                case CatalogManager::TransactionLineageEventKind::TX_COMMIT:
                    return "TX_COMMIT";
                case CatalogManager::TransactionLineageEventKind::TX_ROLLBACK:
                    return "TX_ROLLBACK";
                case CatalogManager::TransactionLineageEventKind::TX_ARCHIVE_TRANSFERRED:
                    return "TX_ARCHIVE_TRANSFERRED";
            }
            return "TX_BEGIN";
        }

        bool isTerminalTransactionLineageKind(CatalogManager::TransactionLineageEventKind kind)
        {
            using TLEK = CatalogManager::TransactionLineageEventKind;
            return kind == TLEK::TX_COMMIT || kind == TLEK::TX_ROLLBACK;
        }

        void appendOrderedUnique(std::vector<std::string>& values, const std::string& value)
        {
            if (value.empty())
            {
                return;
            }
            if (std::find(values.begin(), values.end(), value) == values.end())
            {
                values.push_back(value);
            }
        }

        std::string joinStrings(const std::vector<std::string>& values)
        {
            std::string out;
            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                {
                    out += ",";
                }
                out += values[i];
            }
            return out;
        }

        struct SweepLineageGroup
        {
            ID tx_uuid{};
            uint64_t txid = 0;
            uint64_t first_time = 0;
            uint64_t last_time = 0;
            std::vector<CatalogManager::TransactionLineageEventCatalogInfo> events;
        };

        struct WalAfterLogSourceItem
        {
            ID work_item_id{};
            ID source_sink_profile_id{};
            ID tx_uuid{};
            uint64_t txid = 0;
            uint64_t source_segment_seq = 0;
            uint64_t source_created_time = 0;
            std::string spool_path;
            std::string source_manifest;
            std::vector<SweepPolicyLane> lanes;
        };

        struct WalAfterLogPayload
        {
            uint64_t commit_time = 0;
            std::string statement_hashes_csv;
            std::string lineage_event_ids_csv;
            std::string lineage_event_kinds_csv;
            std::string schema_epoch_refs_csv;
        };

        struct ShadowCapturePayload
        {
            ID primary_object_uuid{};
            std::string object_ids_csv;
            std::string statement_hashes_csv;
            std::string lineage_event_ids_csv;
            std::string lineage_event_kinds_csv;
            std::string terminal_event_kind;
        };

        auto hasTerminalEvent(const SweepLineageGroup& group) -> bool
        {
            for (const auto& event : group.events)
            {
                if (event.event_kind == CatalogManager::TransactionLineageEventKind::TX_COMMIT ||
                    event.event_kind == CatalogManager::TransactionLineageEventKind::TX_ROLLBACK)
                {
                    return true;
                }
            }
            return false;
        }

        auto loadSweepLineageGroups(CatalogManager* catalog,
                                    uint64_t oit_before,
                                    uint64_t oit_after,
                                    std::vector<SweepLineageGroup>& groups_out,
                                    ErrorContext* ctx) -> Status
        {
            groups_out.clear();
            if (!catalog || oit_after <= oit_before)
            {
                return Status::OK;
            }

            std::vector<CatalogManager::TransactionLineageEventCatalogInfo> rows;
            Status status = catalog->listTransactionLineageEventCatalogEntries(ID{}, 0, rows, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            SweepLineageGroup current{};
            bool has_current = false;
            for (const auto& row : rows)
            {
                if (row.txid < oit_before || row.txid >= oit_after)
                {
                    continue;
                }

                if (!has_current || current.tx_uuid != row.tx_uuid)
                {
                    if (has_current)
                    {
                        groups_out.push_back(current);
                    }
                    current = SweepLineageGroup{};
                    current.tx_uuid = row.tx_uuid;
                    current.txid = row.txid;
                    current.first_time = row.created_time;
                    current.last_time = row.created_time;
                    has_current = true;
                }

                current.events.push_back(row);
                current.last_time = row.created_time;
            }

            if (has_current)
            {
                groups_out.push_back(current);
            }
            return Status::OK;
        }

        auto loadCommittedWalAfterLogPayload(CatalogManager* catalog,
                                             const ID& tx_uuid,
                                             uint64_t txid,
                                             WalAfterLogPayload& payload_out,
                                             ErrorContext* ctx) -> Status
        {
            payload_out = WalAfterLogPayload{};
            if (!catalog)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "wal_after_log export requires a catalog");
                return Status::INVALID_ARGUMENT;
            }

            std::vector<CatalogManager::TransactionLineageEventCatalogInfo> rows;
            Status status =
                catalog->listTransactionLineageEventCatalogEntries(tx_uuid, txid, rows, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            bool committed = false;
            bool rolled_back = false;
            std::vector<std::string> statement_hashes;
            std::vector<std::string> lineage_event_ids;
            std::vector<std::string> lineage_event_kinds;
            std::vector<std::string> schema_epoch_refs;

            for (const auto& row : rows)
            {
                lineage_event_ids.push_back(row.lineage_event_id.toString());
                lineage_event_kinds.push_back(transactionLineageEventKindToString(row.event_kind));
                if (row.has_statement_hash)
                {
                    appendOrderedUnique(statement_hashes, std::to_string(row.statement_hash));
                }

                if (!row.payload_json.empty())
                {
                    auto payload_json = nlohmann::json::parse(row.payload_json, nullptr, false);
                    if (payload_json.is_discarded() || !payload_json.is_object())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                          "transaction_lineage_event payload_json is invalid");
                        return Status::DATA_CORRUPTED;
                    }

                    for (const char* key : {"schema_epoch_uuid",
                                            "schema_epoch_before_uuid",
                                            "schema_epoch_after_uuid"})
                    {
                        auto it = payload_json.find(key);
                        if (it != payload_json.end() && it->is_string())
                        {
                            appendOrderedUnique(schema_epoch_refs, it->get<std::string>());
                        }
                    }

                    if (row.event_kind ==
                        CatalogManager::TransactionLineageEventKind::TX_COMMIT)
                    {
                        committed = true;
                        auto it = payload_json.find("commit_time");
                        if (it != payload_json.end() && it->is_number_unsigned())
                        {
                            payload_out.commit_time = it->get<uint64_t>();
                        }
                    }
                    else if (row.event_kind ==
                             CatalogManager::TransactionLineageEventKind::TX_ROLLBACK)
                    {
                        rolled_back = true;
                    }
                }
                else if (row.event_kind ==
                         CatalogManager::TransactionLineageEventKind::TX_COMMIT)
                {
                    committed = true;
                }
                else if (row.event_kind ==
                         CatalogManager::TransactionLineageEventKind::TX_ROLLBACK)
                {
                    rolled_back = true;
                }

                if (row.event_kind == CatalogManager::TransactionLineageEventKind::TX_COMMIT &&
                    payload_out.commit_time == 0)
                {
                    payload_out.commit_time = row.created_time;
                }
            }

            if (rolled_back && !committed)
            {
                return Status::NOT_FOUND;
            }
            if (!committed)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                  "wal_after_log export requires a committed terminal lineage event");
                return Status::DATA_CORRUPTED;
            }

            payload_out.statement_hashes_csv = joinStrings(statement_hashes);
            payload_out.lineage_event_ids_csv = joinStrings(lineage_event_ids);
            payload_out.lineage_event_kinds_csv = joinStrings(lineage_event_kinds);
            payload_out.schema_epoch_refs_csv = joinStrings(schema_epoch_refs);
            return Status::OK;
        }

        auto loadShadowCapturePayload(CatalogManager* catalog,
                                      const ID& tx_uuid,
                                      uint64_t txid,
                                      ShadowCapturePayload& payload_out,
                                      ErrorContext* ctx) -> Status
        {
            payload_out = ShadowCapturePayload{};
            if (!catalog)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "shadow capture requires a catalog");
                return Status::INVALID_ARGUMENT;
            }

            std::vector<CatalogManager::TransactionLineageEventCatalogInfo> rows;
            Status status =
                catalog->listTransactionLineageEventCatalogEntries(tx_uuid, txid, rows, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            if (rows.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                  "SWEEP_SHADOW_CAPTURE_BLOCKED: transaction lineage missing for shadow capture");
                return Status::DATA_CORRUPTED;
            }

            std::vector<std::string> object_ids;
            std::vector<std::string> statement_hashes;
            std::vector<std::string> lineage_event_ids;
            std::vector<std::string> lineage_event_kinds;
            ID primary_object_uuid{};
            bool multiple_objects = false;
            bool saw_terminal = false;

            for (const auto& row : rows)
            {
                appendOrderedUnique(lineage_event_ids, row.lineage_event_id.toString());
                appendOrderedUnique(
                    lineage_event_kinds,
                    transactionLineageEventKindToString(row.event_kind));
                if (row.has_statement_hash)
                {
                    appendOrderedUnique(statement_hashes,
                                        std::to_string(row.statement_hash));
                }
                if (!isZeroIdLocal(row.object_id))
                {
                    appendOrderedUnique(object_ids, row.object_id.toString());
                    if (isZeroIdLocal(primary_object_uuid))
                    {
                        primary_object_uuid = row.object_id;
                    }
                    else if (primary_object_uuid != row.object_id)
                    {
                        multiple_objects = true;
                    }
                }
                if (isTerminalTransactionLineageKind(row.event_kind))
                {
                    payload_out.terminal_event_kind =
                        transactionLineageEventKindToString(row.event_kind);
                    saw_terminal = true;
                }
            }

            if (!saw_terminal)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                  "SWEEP_SHADOW_CAPTURE_BLOCKED: terminal lineage event missing for shadow capture");
                return Status::DATA_CORRUPTED;
            }

            if (multiple_objects)
            {
                primary_object_uuid = ID{};
            }

            payload_out.primary_object_uuid = primary_object_uuid;
            payload_out.object_ids_csv = joinStrings(object_ids);
            payload_out.statement_hashes_csv = joinStrings(statement_hashes);
            payload_out.lineage_event_ids_csv = joinStrings(lineage_event_ids);
            payload_out.lineage_event_kinds_csv = joinStrings(lineage_event_kinds);
            return Status::OK;
        }

        std::string buildShadowCaptureManifest(const Database* db,
                                               const SweepEvidenceWorkItem& source,
                                               const CatalogManager::AuditExportSegmentCatalogInfo& source_segment,
                                               const ShadowCapturePayload& payload,
                                               const ID& sink_profile_id,
                                               const ID& manifest_id,
                                               const std::filesystem::path& shadow_path,
                                               uint64_t created_time,
                                               const std::string& capture_scope,
                                               const std::string& capture_format)
        {
            const std::array<uint8_t, 32> zero_hash{};
            const std::string source_manifest_hash =
                hashBytesToHex(AuditLogger::computeExportSegmentHash(source_segment.payload_manifest,
                                                                    zero_hash));

            std::ostringstream payload_block;
            payload_block << "tx_uuid=" << source.tx_uuid.toString() << "\n"
                          << "txid=" << source.txid << "\n"
                          << "capture_scope=" << capture_scope << "\n"
                          << "capture_format=" << capture_format << "\n"
                          << "object_ids=" << payload.object_ids_csv << "\n"
                          << "statement_hashes=" << payload.statement_hashes_csv << "\n"
                          << "lineage_event_ids=" << payload.lineage_event_ids_csv << "\n"
                          << "lineage_event_kinds=" << payload.lineage_event_kinds_csv << "\n"
                          << "terminal_event_kind=" << payload.terminal_event_kind << "\n";
            const std::string payload_hash =
                hashBytesToHex(AuditLogger::computeExportSegmentHash(payload_block.str(), zero_hash));

            std::ostringstream out;
            out << kShadowCaptureManifestMagic << "\n"
                << "manifest_id=" << manifest_id.toString() << "\n"
                << "database_id=" << (db ? db->uuid().toString() : ID{}.toString()) << "\n"
                << "sink_profile_uuid=" << sink_profile_id.toString() << "\n"
                << "tx_uuid=" << source.tx_uuid.toString() << "\n"
                << "txid=" << source.txid << "\n"
                << "object_uuid="
                << (isZeroIdLocal(payload.primary_object_uuid) ? std::string()
                                                               : payload.primary_object_uuid.toString())
                << "\n"
                << "capture_scope=" << capture_scope << "\n"
                << "capture_format=" << capture_format << "\n"
                << "source_work_item_uuid=" << source.work_item_id.toString() << "\n"
                << "source_sink_profile_uuid=" << source.sink_profile_id.toString() << "\n"
                << "source_segment_seq=" << source.segment_seq << "\n"
                << "source_delivery_state=" << source.delivery_state << "\n"
                << "source_manifest_path=" << source.spool_path << "\n"
                << "source_manifest_hash=" << source_manifest_hash << "\n"
                << "shadow_path=" << shadow_path.string() << "\n"
                << "payload_hash=" << payload_hash << "\n"
                << "created_time=" << created_time << "\n"
                << "object_ids=" << payload.object_ids_csv << "\n"
                << "statement_hashes=" << payload.statement_hashes_csv << "\n"
                << "lineage_event_ids=" << payload.lineage_event_ids_csv << "\n"
                << "lineage_event_kinds=" << payload.lineage_event_kinds_csv << "\n"
                << "terminal_event_kind=" << payload.terminal_event_kind << "\n";
            return out.str();
        }

        std::string buildWalAfterLogManifest(const Database* db,
                                             const WalAfterLogSourceItem& source,
                                             const WalAfterLogPayload& payload,
                                             const ID& sink_profile_id,
                                             const ID& segment_id,
                                             uint64_t stream_seq,
                                             const std::filesystem::path& segment_path,
                                             uint64_t created_time)
        {
            std::ostringstream out;
            out << kWalAfterLogManifestMagic << "\n";
            out << "segment_uuid=" << segment_id.toString() << "\n";
            out << "database_uuid=" << (db ? db->uuid().toString() : ID{}.toString()) << "\n";
            out << "sink_profile_uuid=" << sink_profile_id.toString() << "\n";
            out << "source_sink_profile_uuid=" << source.source_sink_profile_id.toString() << "\n";
            out << "source_work_item_uuid=" << source.work_item_id.toString() << "\n";
            out << "source_segment_seq=" << source.source_segment_seq << "\n";
            out << "stream_partition=" << (db ? db->uuid().toString() : ID{}.toString()) << "\n";
            out << "stream_seq=" << stream_seq << "\n";
            out << "shipping_mode=DEBUG\n";
            out << "tx_uuid=" << source.tx_uuid.toString() << "\n";
            out << "txid=" << source.txid << "\n";
            out << "commit_time=" << payload.commit_time << "\n";
            out << "statement_hashes=" << payload.statement_hashes_csv << "\n";
            out << "schema_epoch_refs=" << payload.schema_epoch_refs_csv << "\n";
            out << "lineage_event_ids=" << payload.lineage_event_ids_csv << "\n";
            out << "lineage_event_kinds=" << payload.lineage_event_kinds_csv << "\n";
            out << "source_spool_path=" << source.spool_path << "\n";
            out << "wal_segment_path=" << segment_path.string() << "\n";
            out << "created_time=" << created_time << "\n";
            return out.str();
        }

        auto buildSweepManifest(const Database* db,
                                const SweepLineageGroup& group,
                                const std::vector<SweepPolicyLane>& lanes,
                                const ID& sink_profile_id,
                                uint64_t oit_before,
                                uint64_t oit_after,
                                const ID& work_item_id,
                                const std::filesystem::path& spool_path,
                                const std::string& delivery_state,
                                uint64_t created_time) -> std::string
        {
            std::string event_ids;
            std::string event_kinds;
            for (size_t i = 0; i < group.events.size(); ++i)
            {
                if (i > 0)
                {
                    event_ids += ",";
                    event_kinds += ",";
                }
                event_ids += group.events[i].lineage_event_id.toString();
                switch (group.events[i].event_kind)
                {
                    case CatalogManager::TransactionLineageEventKind::TX_BEGIN:
                        event_kinds += "TX_BEGIN";
                        break;
                    case CatalogManager::TransactionLineageEventKind::TX_CONTEXT_BOUND:
                        event_kinds += "TX_CONTEXT_BOUND";
                        break;
                    case CatalogManager::TransactionLineageEventKind::TX_MUTATION_BATCH:
                        event_kinds += "TX_MUTATION_BATCH";
                        break;
                    case CatalogManager::TransactionLineageEventKind::TX_DDL_BATCH:
                        event_kinds += "TX_DDL_BATCH";
                        break;
                    case CatalogManager::TransactionLineageEventKind::TX_COMMIT:
                        event_kinds += "TX_COMMIT";
                        break;
                    case CatalogManager::TransactionLineageEventKind::TX_ROLLBACK:
                        event_kinds += "TX_ROLLBACK";
                        break;
                    case CatalogManager::TransactionLineageEventKind::TX_ARCHIVE_TRANSFERRED:
                        event_kinds += "TX_ARCHIVE_TRANSFERRED";
                        break;
                }
            }

            std::ostringstream out;
            out << kSweepManifestMagic << "\n";
            out << "work_item_uuid=" << work_item_id.toString() << "\n";
            out << "database_uuid=" << (db ? db->uuid().toString() : ID{}.toString()) << "\n";
            out << "sink_profile_uuid=" << sink_profile_id.toString() << "\n";
            out << "tx_uuid=" << group.tx_uuid.toString() << "\n";
            out << "txid=" << group.txid << "\n";
            out << "sweep_oit_before=" << oit_before << "\n";
            out << "sweep_oit_after=" << oit_after << "\n";
            out << "policy_lanes=" << encodeSweepPolicyLanes(lanes) << "\n";
            out << "event_count=" << group.events.size() << "\n";
            out << "lineage_event_ids=" << event_ids << "\n";
            out << "lineage_event_kinds=" << event_kinds << "\n";
            out << "spool_path=" << spool_path.string() << "\n";
            out << "delivery_state=" << delivery_state << "\n";
            out << "created_time=" << created_time << "\n";
            return out.str();
        }

        struct PageAuditObservation
        {
            uint64_t page_id = 0;
            std::string page_type;
            std::string error_code;
            std::string severity;
            std::string details_json;
        };

        auto isRecognizedPageTypeForAudit(uint16_t page_type) -> bool
        {
            if (validateVNextPageTypeKnown(page_type) != Status::OK)
            {
                return false;
            }
            switch (page_type)
            {
                case PAGE_TYPE_DATABASE_HEADER:
                case PAGE_TYPE_SYSTEM_STATE:
                case PAGE_TYPE_CATALOG_ROOT:
                case PAGE_TYPE_CATALOG_PAGE:
                case PAGE_TYPE_FSM_ROOT:
                case PAGE_TYPE_FSM_PAGE:
                case PAGE_TYPE_TRANSACTION_MAP:
                case PAGE_TYPE_HEAP:
                case PAGE_TYPE_TOAST_META:
                case PAGE_TYPE_TOAST_CHUNK:
                case PAGE_TYPE_LOB_META:
                case PAGE_TYPE_LOB_CHUNK:
                case PAGE_TYPE_TEMP_HEAP:
                case PAGE_TYPE_NAME_REGISTRY:
                case PAGE_TYPE_BOOTSTRAP_RESERVED:
                case PAGE_TYPE_FILESPACE_HEADER:
                    return true;
                default:
                    return isCanonicalIndexPageType(page_type) || isKnownVNextPageType(page_type);
            }
        }

        auto pageTypeToAuditString(uint16_t page_type) -> std::string
        {
            switch (page_type)
            {
                case PAGE_TYPE_DATABASE_HEADER: return "DATABASE_HEADER";
                case PAGE_TYPE_CATALOG_ROOT: return "CATALOG_ROOT";
                case PAGE_TYPE_CATALOG_PAGE: return "CATALOG_PAGE";
                case PAGE_TYPE_FSM_ROOT: return "FSM_ROOT";
                case PAGE_TYPE_FSM_PAGE: return "FSM_PAGE";
                case PAGE_TYPE_TRANSACTION_MAP: return "TRANSACTION_MAP";
                case PAGE_TYPE_HEAP: return "HEAP";
                case PAGE_TYPE_TOAST_META: return "TOAST_META";
                case PAGE_TYPE_TOAST_CHUNK: return "TOAST_CHUNK";
                case PAGE_TYPE_LOB_META: return "LOB_META";
                case PAGE_TYPE_LOB_CHUNK: return "LOB_CHUNK";
                case PAGE_TYPE_TEMP_HEAP: return "TEMP_HEAP";
                case PAGE_TYPE_FILESPACE_HEADER: return "FILESPACE_HEADER";
                default:
                    if (isCanonicalIndexPageType(page_type))
                    {
                        return "INDEX";
                    }
                    if (isKnownVNextPageType(page_type))
                    {
                        return "VNEXT";
                    }
                    return "UNKNOWN";
            }
        }

        auto makePageAuditDetails(std::initializer_list<std::pair<const char*, std::string>> fields)
            -> std::string
        {
            nlohmann::json json = nlohmann::json::object();
            for (const auto& field : fields)
            {
                json[field.first] = field.second;
            }
            return json.dump();
        }

        auto appendPageAuditObservation(std::vector<PageAuditObservation>& findings,
                                        uint64_t page_id,
                                        const std::string& page_type,
                                        const std::string& error_code,
                                        const std::string& severity,
                                        const std::string& details_json) -> void
        {
            findings.push_back(PageAuditObservation{
                page_id,
                page_type,
                error_code,
                severity,
                details_json,
            });
        }
    } // namespace

    SweepManager::SweepManager(Database *db) : db_(db), txn_manager_(nullptr), buffer_pool_(nullptr)
    {
    }

    SweepManager::~SweepManager()
    {
        // Wait for any ongoing sweep to complete
        while (sweep_in_progress_.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    Status SweepManager::initialize(ErrorContext *ctx)
    {
        if (!db_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
            return Status::INVALID_ARGUMENT;
        }

        txn_manager_ = db_->transaction_manager();
        buffer_pool_ = db_->buffer_pool();

        if (!txn_manager_ || !buffer_pool_)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "TransactionManager or BufferPool not available");
            return Status::IO_ERROR;
        }

        LOG_INFO(VACUUM, "SweepManager initialized");
        return Status::OK;
    }

    Status SweepManager::loadSweepProgressState(SweepProgressState *state_out,
                                                ErrorContext *ctx) const
    {
        if (state_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "state_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }
        *state_out = SweepProgressState{};

        if (buffer_pool_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer = nullptr;
        Status status = buffer_pool_->pinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *state_page = static_cast<BootstrapSystemStatePage *>(page_buffer);
        if (state_page->page_header.page_type != PAGE_TYPE_SYSTEM_STATE ||
            state_page->page_header.page_size != db_->page_size())
        {
            buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid system state bootstrap page");
            return Status::PAGE_CORRUPT;
        }

        state_out->generation_id = state_page->reserved[kSweepProgressSlotGeneration];
        state_out->active = state_page->reserved[kSweepProgressSlotActive] != 0;
        state_out->start_horizon = state_page->reserved[kSweepProgressSlotStartHorizon];
        state_out->last_relation_id = decodeIdFromSlots(
            state_page->reserved[kSweepProgressSlotRelationHi],
            state_page->reserved[kSweepProgressSlotRelationLo]);
        state_out->last_page_cursor = state_page->reserved[kSweepProgressSlotPageCursor];
        state_out->reclaimed_version_count =
            state_page->reserved[kSweepProgressSlotReclaimedVersions];
        state_out->reclaimed_bytes = state_page->reserved[kSweepProgressSlotReclaimedBytes];
        state_out->index_backlog_count = state_page->reserved[kSweepProgressSlotIndexBacklog];
        unpackSweepProgressResumeMeta(state_page->reserved[kSweepProgressSlotResumeMeta],
                                      &state_out->stage,
                                      &state_out->resume_lane_mask,
                                      &state_out->resume_strict_audit,
                                      &state_out->resume_oit_before);

        buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);

        if (state_out->active)
        {
            if (state_out->stage == SweepProgressStage::NONE)
            {
                if (state_out->last_page_cursor != 0 || state_out->reclaimed_version_count != 0 ||
                    state_out->reclaimed_bytes != 0 || state_out->index_backlog_count != 0 ||
                    !isZeroIdLocal(state_out->last_relation_id))
                {
                    state_out->stage = SweepProgressStage::RECLAIM_PENDING;
                }
                else
                {
                    state_out->stage = SweepProgressStage::LOCAL_EVIDENCE_PENDING;
                }
            }

            if (state_out->resume_oit_before == 0 && txn_manager_ != nullptr)
            {
                state_out->resume_oit_before = txn_manager_->getOldestXid();
            }
            if (state_out->resume_lane_mask == 0)
            {
                state_out->resume_lane_mask =
                    static_cast<uint16_t>(uint16_t{1}
                                          << static_cast<uint8_t>(SweepPolicyLane::NORMAL));
            }
        }
        else
        {
            state_out->stage = SweepProgressStage::NONE;
            state_out->resume_lane_mask = 0;
            state_out->resume_strict_audit = true;
            state_out->resume_oit_before = 0;
        }

        return Status::OK;
    }

    Status SweepManager::persistSweepProgressState(const SweepProgressState &state,
                                                   ErrorContext *ctx)
    {
        if (buffer_pool_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        if ((state.resume_oit_before & ~kSweepProgressResumeOitMask) != 0)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::INVALID_ARGUMENT,
                              "Sweep resume OIT exceeds checkpoint encoding capacity");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer = nullptr;
        Status status = buffer_pool_->pinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *state_page = static_cast<BootstrapSystemStatePage *>(page_buffer);
        if (state_page->page_header.page_type != PAGE_TYPE_SYSTEM_STATE ||
            state_page->page_header.page_size != db_->page_size())
        {
            buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid system state bootstrap page");
            return Status::PAGE_CORRUPT;
        }

        uint64_t relation_hi = 0;
        uint64_t relation_lo = 0;
        encodeIdToSlots(state.last_relation_id, relation_hi, relation_lo);
        state_page->reserved[kSweepProgressSlotGeneration] = state.generation_id;
        state_page->reserved[kSweepProgressSlotActive] = state.active ? 1u : 0u;
        state_page->reserved[kSweepProgressSlotStartHorizon] = state.start_horizon;
        state_page->reserved[kSweepProgressSlotRelationHi] = relation_hi;
        state_page->reserved[kSweepProgressSlotRelationLo] = relation_lo;
        state_page->reserved[kSweepProgressSlotPageCursor] = state.last_page_cursor;
        state_page->reserved[kSweepProgressSlotReclaimedVersions] =
            state.reclaimed_version_count;
        state_page->reserved[kSweepProgressSlotReclaimedBytes] = state.reclaimed_bytes;
        state_page->reserved[kSweepProgressSlotIndexBacklog] = state.index_backlog_count;
        state_page->reserved[kSweepProgressSlotResumeMeta] =
            packSweepProgressResumeMeta(state.stage,
                                        state.resume_lane_mask,
                                        state.resume_strict_audit,
                                        state.resume_oit_before);

        if (db_ != nullptr && db_->mga_failpoint_manager() != nullptr)
        {
            Status failpoint_status = db_->mga_failpoint_manager()->trip(
                MgaFailpointTriggers::kSweepCheckpointWriteLoss,
                {},
                ctx);
            if (failpoint_status != Status::OK)
            {
                buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, false, ctx);
                return failpoint_status;
            }
        }

        buffer_pool_->unpinPage(BOOTSTRAP_PAGE_SYSTEM_STATE, true, ctx);
        return Status::OK;
    }

    Status SweepManager::setPolicyBindings(const std::vector<SweepPolicyBinding>& bindings,
                                           ErrorContext* ctx)
    {
        std::vector<SweepPolicyBinding> normalized = bindings;
        std::vector<std::pair<SweepScopeKind, ID>> seen;
        seen.reserve(normalized.size());

        for (auto& binding : normalized)
        {
            if (binding.lanes.empty())
            {
                binding.lanes.push_back(SweepPolicyLane::NORMAL);
            }
            if (std::find(binding.lanes.begin(), binding.lanes.end(), SweepPolicyLane::NORMAL) !=
                    binding.lanes.end() &&
                binding.lanes.size() > 1)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "SWEEP_POLICY_INVALID: NORMAL cannot be combined with non-normal lanes");
                return Status::INVALID_ARGUMENT;
            }
            const auto duplicate = std::find_if(
                seen.begin(), seen.end(), [&binding](const auto& entry) {
                    return entry.first == binding.scope_kind && entry.second == binding.scope_id;
                });
            if (duplicate != seen.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                                  "SWEEP_POLICY_INVALID: duplicate sweep policy binding scope");
                return Status::CONSTRAINT_VIOLATION;
            }
            seen.emplace_back(binding.scope_kind, binding.scope_id);

            if (isZeroIdLocal(binding.binding_id))
            {
                binding.binding_id = generateUuidV7();
            }
            if (binding.created_time == 0)
            {
                binding.created_time = currentSystemMicros();
            }
        }

        std::lock_guard<std::mutex> lock(policy_mutex_);
        policy_bindings_ = std::move(normalized);
        return Status::OK;
    }

    Status SweepManager::resolvePolicyBinding(const std::vector<SweepPolicyScope>& scope_chain,
                                              SweepPolicyBinding& binding_out,
                                              ErrorContext* ctx) const
    {
        std::lock_guard<std::mutex> lock(policy_mutex_);
        for (const auto& scope : scope_chain)
        {
            auto match = std::find_if(
                policy_bindings_.begin(), policy_bindings_.end(), [&scope](const auto& binding) {
                    return binding.scope_kind == scope.scope_kind && binding.scope_id == scope.scope_id;
                });
            if (match != policy_bindings_.end())
            {
                binding_out = *match;
                return Status::OK;
            }
        }

        auto default_db = std::find_if(
            policy_bindings_.begin(), policy_bindings_.end(), [](const auto& binding) {
                return binding.scope_kind == SweepScopeKind::DATABASE && isZeroIdLocal(binding.scope_id);
            });
        if (default_db != policy_bindings_.end())
        {
            binding_out = *default_db;
            return Status::OK;
        }

        binding_out = SweepPolicyBinding{};
        binding_out.scope_kind = SweepScopeKind::DATABASE;
        binding_out.scope_id = db_ ? db_->uuid() : ID{};
        binding_out.lanes = {SweepPolicyLane::NORMAL};
        binding_out.strict_audit = false;
        binding_out.created_time = currentSystemMicros();
        (void)ctx;
        return Status::OK;
    }

    Status SweepManager::listEvidenceWorkItems(std::vector<SweepEvidenceWorkItem>& rows_out,
                                               ErrorContext* ctx) const
    {
        rows_out.clear();
        if (!db_ || !db_->catalog_manager())
        {
            return Status::OK;
        }

        std::vector<CatalogManager::AuditSinkProfileCatalogInfo> profiles;
        Status status = db_->catalog_manager()->listAuditSinkProfileCatalogEntries(profiles, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& profile : profiles)
        {
            std::vector<CatalogManager::AuditExportSegmentCatalogInfo> segments;
            status = db_->catalog_manager()->listAuditExportSegmentCatalogEntries(
                profile.audit_sink_profile_id, segments, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            for (const auto& segment : segments)
            {
                if (segment.payload_manifest.rfind(kSweepManifestMagic, 0) != 0)
                {
                    continue;
                }

                SweepEvidenceWorkItem item{};
                item.work_item_id = segment.audit_export_segment_id;
                item.sink_profile_id = segment.audit_sink_profile_id;
                item.segment_seq = segment.segment_seq;
                item.evidence_class = segment.evidence_class;
                item.delivery_state = segment.delivery_state;
                item.created_time = segment.created_time;

                std::string text;
                if (!extractManifestField(segment.payload_manifest, "tx_uuid", text) ||
                    !parseUuidFromString(text, item.tx_uuid))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                      "Persisted sweep manifest carries an invalid tx_uuid");
                    return Status::DATA_CORRUPTED;
                }
                if (!extractManifestField(segment.payload_manifest, "txid", text))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                      "Persisted sweep manifest is missing txid");
                    return Status::DATA_CORRUPTED;
                }
                item.txid = std::strtoull(text.c_str(), nullptr, 10);

                if (extractManifestField(segment.payload_manifest, "sweep_oit_before", text))
                {
                    item.sweep_oit_before = std::strtoull(text.c_str(), nullptr, 10);
                }
                if (extractManifestField(segment.payload_manifest, "sweep_oit_after", text))
                {
                    item.sweep_oit_after = std::strtoull(text.c_str(), nullptr, 10);
                }
                if (extractManifestField(segment.payload_manifest, "spool_path", text))
                {
                    item.spool_path = std::move(text);
                }
                if (extractManifestField(segment.payload_manifest, "policy_lanes", text))
                {
                    std::vector<SweepPolicyLane> parsed_lanes;
                    if (!parseSweepPolicyLanes(text, parsed_lanes))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                          "Persisted sweep manifest carries invalid policy_lanes");
                        return Status::DATA_CORRUPTED;
                    }
                    item.lanes_csv = std::move(text);
                }

                rows_out.push_back(std::move(item));
            }
        }

        std::sort(rows_out.begin(), rows_out.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.segment_seq != rhs.segment_seq)
            {
                return lhs.segment_seq < rhs.segment_seq;
            }
            return lhs.created_time < rhs.created_time;
        });
        return Status::OK;
    }

    Status SweepManager::listWalAfterLogSegments(std::vector<SweepWalAfterLogSegment>& rows_out,
                                                 ErrorContext* ctx) const
    {
        rows_out.clear();
        if (!db_ || !db_->catalog_manager())
        {
            return Status::OK;
        }

        std::vector<CatalogManager::AuditSinkProfileCatalogInfo> profiles;
        Status status = db_->catalog_manager()->listAuditSinkProfileCatalogEntries(profiles, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& profile : profiles)
        {
            std::vector<CatalogManager::AuditExportSegmentCatalogInfo> segments;
            status = db_->catalog_manager()->listAuditExportSegmentCatalogEntries(
                profile.audit_sink_profile_id, segments, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            for (const auto& segment : segments)
            {
                if (segment.payload_manifest.rfind(kWalAfterLogManifestMagic, 0) != 0)
                {
                    continue;
                }

                SweepWalAfterLogSegment item{};
                item.segment_id = segment.audit_export_segment_id;
                item.sink_profile_id = segment.audit_sink_profile_id;
                item.created_time = segment.created_time;

                std::string text;
                if (!extractManifestField(segment.payload_manifest, "source_work_item_uuid", text) ||
                    !parseUuidFromString(text, item.source_work_item_id))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                      "Persisted wal_after_log manifest carries an invalid source_work_item_uuid");
                    return Status::DATA_CORRUPTED;
                }
                if (!extractManifestField(segment.payload_manifest, "source_sink_profile_uuid", text) ||
                    !parseUuidFromString(text, item.source_sink_profile_id))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                      "Persisted wal_after_log manifest carries an invalid source_sink_profile_uuid");
                    return Status::DATA_CORRUPTED;
                }
                if (!extractManifestField(segment.payload_manifest, "tx_uuid", text) ||
                    !parseUuidFromString(text, item.tx_uuid))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                      "Persisted wal_after_log manifest carries an invalid tx_uuid");
                    return Status::DATA_CORRUPTED;
                }
                if (!extractManifestUint64Field(segment.payload_manifest, "txid", item.txid) ||
                    !extractManifestUint64Field(segment.payload_manifest, "stream_seq", item.stream_seq) ||
                    !extractManifestUint64Field(segment.payload_manifest, "commit_time", item.commit_time))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                      "Persisted wal_after_log manifest is missing numeric fields");
                    return Status::DATA_CORRUPTED;
                }
                if (extractManifestField(segment.payload_manifest, "shipping_mode", text))
                {
                    item.shipping_mode = std::move(text);
                }
                if (extractManifestField(segment.payload_manifest, "statement_hashes", text))
                {
                    item.statement_hashes_csv = std::move(text);
                }
                if (extractManifestField(segment.payload_manifest, "wal_segment_path", text))
                {
                    item.segment_path = std::move(text);
                }

                rows_out.push_back(std::move(item));
            }
        }

        std::sort(rows_out.begin(), rows_out.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.stream_seq != rhs.stream_seq)
            {
                return lhs.stream_seq < rhs.stream_seq;
            }
            return lhs.created_time < rhs.created_time;
        });
        return Status::OK;
    }

    Status SweepManager::listPageAuditFindings(std::vector<SweepPageAuditFinding>& rows_out,
                                               ErrorContext* ctx) const
    {
        rows_out.clear();
        if (!db_ || !db_->catalog_manager())
        {
            return Status::OK;
        }

        std::vector<CatalogManager::PageAuditFindingCatalogInfo> findings;
        Status status = db_->catalog_manager()->listPageAuditFindingCatalogEntries(findings, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        rows_out.reserve(findings.size());
        for (const auto& finding : findings)
        {
            SweepPageAuditFinding row{};
            row.finding_id = finding.finding_id;
            row.finding_time = finding.finding_time;
            row.scan_mode = finding.scan_mode;
            row.trigger_source = finding.trigger_source;
            row.filespace_uuid = finding.filespace_uuid;
            row.page_id = finding.page_id;
            row.page_type = finding.page_type;
            row.error_code = finding.error_code;
            row.severity = finding.severity;
            row.related_tx_uuid = finding.related_tx_uuid;
            row.related_capsule_uuid = finding.related_capsule_uuid;
            row.details_json = finding.details_json;
            row.is_valid = finding.is_valid;
            rows_out.push_back(std::move(row));
        }
        return Status::OK;
    }

    Status SweepManager::listShadowCaptureManifests(
        std::vector<SweepShadowCaptureManifest>& rows_out,
        ErrorContext* ctx) const
    {
        rows_out.clear();
        if (!db_ || !db_->catalog_manager())
        {
            return Status::OK;
        }

        std::vector<CatalogManager::ShadowCaptureManifestCatalogInfo> manifests;
        Status status =
            db_->catalog_manager()->listShadowCaptureManifestCatalogEntries(manifests, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        rows_out.reserve(manifests.size());
        for (const auto& manifest : manifests)
        {
            SweepShadowCaptureManifest row{};
            row.manifest_id = manifest.manifest_id;
            row.tx_uuid = manifest.tx_uuid;
            row.object_uuid = manifest.object_uuid;
            row.sink_profile_id = manifest.sink_profile_uuid;
            row.created_time = manifest.created_time;
            row.has_retention_deadline_time = manifest.has_retention_deadline_time;
            row.retention_deadline_time = manifest.retention_deadline_time;
            row.capture_scope = manifest.capture_scope;
            row.capture_format = manifest.capture_format;
            row.payload_manifest = manifest.payload_manifest;
            row.is_valid = manifest.is_valid;
            rows_out.push_back(std::move(row));
        }
        return Status::OK;
    }

    bool SweepManager::checkSweepTrigger(ErrorContext *ctx)
    {
        // Don't trigger if sweep is already in progress
        if (sweep_in_progress_.load(std::memory_order_acquire))
        {
            return false;
        }

        ReclaimHorizonSnapshot horizons{};
        Status horizon_status = txn_manager_->captureReclaimHorizons(horizons, ctx);
        if (horizon_status != Status::OK)
        {
            return false;
        }

        // No sweep needed if no reclaim horizon can be proven.
        if (horizons.heap_reclaim_horizon == 0 || horizons.heap_reclaim_horizon == UINT64_MAX)
        {
            return false;
        }

        // Calculate transaction gap
        uint64_t gap = (horizons.heap_reclaim_horizon > horizons.oldest_interesting_xid)
                           ? (horizons.heap_reclaim_horizon - horizons.oldest_interesting_xid)
                           : 0;

        // Phase 4 Enhancement: Read sweep_interval from config
        // For now, use hardcoded default of 20000 (safe default value)
        uint32_t sweep_interval = config::DEFAULT_SWEEP_INTERVAL;

        // Trigger sweep if gap exceeds threshold
        if (gap > sweep_interval)
        {
            LOG_INFO(VACUUM,
                     "Sweep trigger condition met: gap=%lu, interval=%u, oit=%lu, reclaim_horizon=%lu, ost=%lu",
                     gap,
                     sweep_interval,
                     horizons.oldest_interesting_xid,
                     horizons.heap_reclaim_horizon,
                     horizons.oldest_snapshot_xid);

            // Trigger background sweep (non-blocking)
            Status s = executeSweep(false, ctx);
            if (s != Status::OK)
            {
                LOG_ERROR(VACUUM, "Failed to trigger sweep: %d", static_cast<int>(s));
                return false;
            }

            return true;
        }

        return false;
    }

    Status SweepManager::executeSweep(bool foreground, ErrorContext *ctx)
    {
        // Check if sweep is already running
        bool expected = false;
        if (!sweep_in_progress_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            LOG_WARNING(VACUUM, "Sweep already in progress, skipping");
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Sweep already in progress");
            return Status::IO_ERROR;
        }

        LOG_INFO(VACUUM, "Starting sweep: mode=%s", foreground ? "foreground" : "background");

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.sweep_in_progress = true;
        }

        auto start_time = std::chrono::steady_clock::now();
        ReclaimHorizonSnapshot horizons{};
        Status horizon_status = txn_manager_->captureReclaimHorizons(horizons, ctx);
        if (horizon_status != Status::OK)
        {
            sweep_in_progress_.store(false, std::memory_order_release);
            return horizon_status;
        }

        SweepPolicyBinding active_binding{};
        Status binding_status = resolvePolicyBinding(
            {SweepPolicyScope{SweepScopeKind::DATABASE, db_->uuid()}}, active_binding, ctx);
        if (binding_status != Status::OK)
        {
            sweep_in_progress_.store(false, std::memory_order_release);
            return binding_status;
        }

        SweepProgressState sweep_progress{};
        Status s = loadSweepProgressState(&sweep_progress, ctx);
        if (s != Status::OK)
        {
            sweep_in_progress_.store(false, std::memory_order_release);
            return s;
        }

        const bool resuming_incomplete = sweep_progress.active;
        if (!resuming_incomplete)
        {
            sweep_progress.generation_id++;
            sweep_progress.active = true;
            sweep_progress.start_horizon = horizons.heap_reclaim_horizon;
            if (sweep_progress.start_horizon == UINT64_MAX)
            {
                sweep_progress.start_horizon = horizons.current_xid;
            }
            sweep_progress.resume_oit_before = horizons.oldest_interesting_xid;
            sweep_progress.resume_lane_mask = encodeSweepPolicyLaneMask(active_binding.lanes);
            sweep_progress.resume_strict_audit = active_binding.strict_audit;
            sweep_progress.last_relation_id = ID{};
            sweep_progress.last_page_cursor = 0;
            sweep_progress.reclaimed_version_count = 0;
            sweep_progress.reclaimed_bytes = 0;
            sweep_progress.index_backlog_count = 0;
            sweep_progress.stage = SweepProgressStage::LOCAL_EVIDENCE_PENDING;
        }
        else if (sweep_progress.resume_oit_before == 0)
        {
            sweep_progress.resume_oit_before = horizons.oldest_interesting_xid;
        }

        if (sweep_progress.resume_lane_mask == 0)
        {
            sweep_progress.resume_lane_mask = encodeSweepPolicyLaneMask(active_binding.lanes);
        }

        auto persistCheckpoint = [&]() -> Status { return persistSweepProgressState(sweep_progress, ctx); };

        s = persistCheckpoint();
        if (s != Status::OK)
        {
            sweep_in_progress_.store(false, std::memory_order_release);
            return s;
        }

        const uint64_t sweep_oit_before = sweep_progress.resume_oit_before;
        uint64_t new_oit = findFirstUncommittedTransaction(ctx);
        uint64_t sweep_oit_after = new_oit;
        if (sweep_oit_after == 0 && resuming_incomplete)
        {
            sweep_oit_after = horizons.oldest_interesting_xid;
        }

        if (!resuming_incomplete && (sweep_oit_after == 0 || sweep_oit_after == sweep_oit_before))
        {
            // No change needed, but still update statistics
            LOG_INFO(VACUUM, "Sweep completed: OIT unchanged (oit=%lu)", sweep_oit_before);

            auto end_time = std::chrono::steady_clock::now();
            uint64_t duration_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                    .count();
            updateStatistics(sweep_oit_before,
                             sweep_oit_before,
                             duration_ms,
                             0,
                             0,
                             0,
                             0,
                             0,
                             false,
                             false,
                             false,
                             false,
                             false,
                             false);

            sweep_progress.active = false;
            sweep_progress.resume_oit_before = 0;
            sweep_progress.stage = SweepProgressStage::NONE;
            (void)persistCheckpoint();

            sweep_in_progress_.store(false, std::memory_order_release);
            return Status::OK;
        }

        if (sweep_oit_after == 0)
        {
            sweep_oit_after = horizons.oldest_interesting_xid;
        }

        // 2. Update OIT in database header for a newly-started pass. Resumed
        // passes keep the already-advanced frontier and continue the
        // evidence/reclaim handoff from the persisted stage.
        if (!resuming_incomplete)
        {
            s = txn_manager_->setOldestXid(sweep_oit_after, ctx);
            if (s != Status::OK)
            {
                LOG_ERROR(VACUUM, "Failed to update OIT: %d", static_cast<int>(s));
                sweep_in_progress_.store(false, std::memory_order_release);
                return s;
            }
        }

        uint64_t evidence_items_emitted = 0;
        uint64_t wal_after_segments_emitted = 0;
        uint64_t wal_after_backlog_depth = 0;
        uint64_t page_audit_findings_emitted = 0;
        uint64_t shadow_capture_manifests_emitted = 0;
        bool prune_blocked = false;
        bool evidence_failure = false;
        bool page_audit_failure = false;
        bool page_audit_mode_downgraded = false;
        bool shadow_capture_failure = false;
        bool wal_after_failure = false;

        if (sweep_progress.stage == SweepProgressStage::LOCAL_EVIDENCE_PENDING)
        {
            // 3. Mandatory local evidence spool for non-normal lanes before prune handoff.
            s = emitLocalEvidenceForSweep(
                sweep_oit_before,
                sweep_oit_after,
                sweep_progress.resume_lane_mask,
                sweep_progress.resume_strict_audit,
                &evidence_items_emitted,
                &prune_blocked,
                ctx);
            if (s != Status::OK)
            {
                evidence_failure = true;
                prune_blocked = true;

                auto end_time = std::chrono::steady_clock::now();
                uint64_t duration_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                        .count();
                updateStatistics(sweep_oit_before,
                                 sweep_oit_after,
                                 duration_ms,
                                 evidence_items_emitted,
                                 0,
                                 0,
                                 0,
                                 0,
                                 true,
                                 true,
                                 false,
                                 false,
                                 false,
                                 false);

                if (db_->garbage_collector() != nullptr)
                {
                    db_->garbage_collector()->notifySweepEvidenceBlocked(sweep_oit_before,
                                                                         sweep_oit_after);
                }

                sweep_in_progress_.store(false, std::memory_order_release);
                return s;
            }

            sweep_progress.stage = SweepProgressStage::PAGE_AUDIT_PENDING;
            s = persistCheckpoint();
            if (s != Status::OK)
            {
                sweep_in_progress_.store(false, std::memory_order_release);
                return s;
            }
        }

        if (sweep_progress.stage == SweepProgressStage::PAGE_AUDIT_PENDING)
        {
            // 4. Page spot audit is read-only but its findings are mandatory local
            // evidence for the PAGE_SPOT_AUDIT lane.
            s = emitPageSpotAuditFindings(
                foreground,
                sweep_progress.resume_lane_mask,
                &page_audit_findings_emitted,
                &page_audit_mode_downgraded,
                ctx);
            if (s != Status::OK)
            {
                page_audit_failure = true;
                prune_blocked = true;

                auto end_time = std::chrono::steady_clock::now();
                uint64_t duration_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                        .count();
                updateStatistics(sweep_oit_before,
                                 sweep_oit_after,
                                 duration_ms,
                                 evidence_items_emitted,
                                 0,
                                 0,
                                 page_audit_findings_emitted,
                                 0,
                                 true,
                                 evidence_failure,
                                 true,
                                 page_audit_mode_downgraded,
                                 false,
                                 false);

                if (db_->garbage_collector() != nullptr)
                {
                    db_->garbage_collector()->notifySweepEvidenceBlocked(sweep_oit_before,
                                                                         sweep_oit_after);
                }

                sweep_in_progress_.store(false, std::memory_order_release);
                return s;
            }

            sweep_progress.stage = SweepProgressStage::SHADOW_CAPTURE_PENDING;
            s = persistCheckpoint();
            if (s != Status::OK)
            {
                sweep_in_progress_.store(false, std::memory_order_release);
                return s;
            }
        }

        if (sweep_progress.stage == SweepProgressStage::SHADOW_CAPTURE_PENDING)
        {
            // 5. Logical shadow capture is a separate local evidence lane and
            // blocks prune when persistence fails.
            s = emitShadowCaptureManifests(&shadow_capture_manifests_emitted,
                                           sweep_progress.resume_lane_mask,
                                           sweep_progress.resume_strict_audit,
                                           ctx);
            if (s != Status::OK)
            {
                shadow_capture_failure = true;
                prune_blocked = true;

                auto end_time = std::chrono::steady_clock::now();
                uint64_t duration_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                        .count();
                updateStatistics(sweep_oit_before,
                                 sweep_oit_after,
                                 duration_ms,
                                 evidence_items_emitted,
                                 0,
                                 0,
                                 page_audit_findings_emitted,
                                 shadow_capture_manifests_emitted,
                                 true,
                                 evidence_failure,
                                 page_audit_failure,
                                 page_audit_mode_downgraded,
                                 true,
                                 false);

                if (db_->garbage_collector() != nullptr)
                {
                    db_->garbage_collector()->notifySweepEvidenceBlocked(sweep_oit_before,
                                                                         sweep_oit_after);
                }

                sweep_in_progress_.store(false, std::memory_order_release);
                return s;
            }

            sweep_progress.stage = SweepProgressStage::WAL_AFTER_PENDING;
            s = persistCheckpoint();
            if (s != Status::OK)
            {
                sweep_in_progress_.store(false, std::memory_order_release);
                return s;
            }
        }

        if (sweep_progress.stage == SweepProgressStage::WAL_AFTER_PENDING)
        {
            // 6. Derivative wal_after_log export is downstream of local immutable
            // evidence and never becomes prune or recovery truth.
            ErrorContext wal_ctx;
            Status wal_status = emitDerivativeWalAfterLog(
                &wal_after_segments_emitted,
                &wal_after_backlog_depth,
                sweep_progress.resume_lane_mask,
                &wal_ctx);
            if (wal_status != Status::OK && wal_status != Status::NOT_FOUND)
            {
                wal_after_failure = true;
                LOG_WARNING(VACUUM,
                            "wal_after_log derivative export deferred: status=%d detail=%s",
                            static_cast<int>(wal_status),
                            wal_ctx.message.c_str());
            }

            sweep_progress.stage = SweepProgressStage::RECLAIM_PENDING;
            s = persistCheckpoint();
            if (s != Status::OK)
            {
                sweep_in_progress_.store(false, std::memory_order_release);
                return s;
            }
        }

        if (sweep_progress.stage == SweepProgressStage::RECLAIM_PENDING && foreground)
        {
            // 7. Optional: Remove old tuple versions (if foreground).
            s = reclaimSpace(sweep_oit_after, &sweep_progress, ctx);
            if (s != Status::OK)
            {
                LOG_WARNING(VACUUM, "Space reclamation failed: %d", static_cast<int>(s));

                auto end_time = std::chrono::steady_clock::now();
                uint64_t duration_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                        .count();

                updateStatistics(sweep_oit_before,
                                 sweep_oit_after,
                                 duration_ms,
                                 evidence_items_emitted,
                                 wal_after_segments_emitted,
                                 wal_after_backlog_depth,
                                 page_audit_findings_emitted,
                                 shadow_capture_manifests_emitted,
                                 prune_blocked,
                                 evidence_failure,
                                 page_audit_failure,
                                 page_audit_mode_downgraded,
                                 shadow_capture_failure,
                                 wal_after_failure);

                sweep_in_progress_.store(false, std::memory_order_release);
                return s;
            }
        }

        // 8. Update statistics
        auto end_time = std::chrono::steady_clock::now();
        uint64_t duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        updateStatistics(sweep_oit_before,
                         sweep_oit_after,
                         duration_ms,
                         evidence_items_emitted,
                         wal_after_segments_emitted,
                         wal_after_backlog_depth,
                         page_audit_findings_emitted,
                         shadow_capture_manifests_emitted,
                         prune_blocked,
                         evidence_failure,
                         page_audit_failure,
                         page_audit_mode_downgraded,
                         shadow_capture_failure,
                         wal_after_failure);

        LOG_INFO(VACUUM,
                 "Sweep completed: old_oit=%lu, new_oit=%lu, duration=%lums",
                 sweep_oit_before,
                 sweep_oit_after,
                 duration_ms);

        sweep_progress.active = false;
        sweep_progress.start_horizon =
            (sweep_progress.start_horizon == 0) ? sweep_oit_after : sweep_progress.start_horizon;
        sweep_progress.resume_oit_before = 0;
        sweep_progress.resume_lane_mask = 0;
        sweep_progress.resume_strict_audit = true;
        sweep_progress.stage = SweepProgressStage::NONE;
        (void)persistCheckpoint();

        // 9. Notify garbage collector that OIT has advanced and the local evidence gate is clear
        // This allows GC to identify more garbage tuples for removal
        if (db_->garbage_collector() != nullptr)
        {
            db_->garbage_collector()->notifySweepComplete(sweep_oit_before, sweep_oit_after);
        }

        sweep_in_progress_.store(false, std::memory_order_release);
        return Status::OK;
    }

    uint64_t SweepManager::findFirstUncommittedTransaction(ErrorContext *ctx) const
    {
        uint64_t current_oit = txn_manager_->getOldestXid();
        uint64_t new_oit = current_oit;
        Status status = txn_manager_->findOldestInterestingXidFromInventory(new_oit, ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(VACUUM,
                        "TIP horizon walk failed while resolving new OIT; keeping conservative frontier at %lu",
                        current_oit);
            return current_oit;
        }

        LOG_DEBUG(VACUUM,
                  "Resolved new OIT from TIP walk: old_oit=%lu, new_oit=%lu, delta=%ld",
                  current_oit,
                  new_oit,
                  static_cast<long>(new_oit - current_oit));
        return new_oit;
    }

    Status SweepManager::reclaimSpace(uint64_t new_oit,
                                      SweepProgressState *progress,
                                      ErrorContext *ctx)
    {
        if (progress == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "progress cannot be null");
            return Status::INVALID_ARGUMENT;
        }
        if (db_ == nullptr || db_->page_manager() == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "PageManager not available");
            return Status::INVALID_ARGUMENT;
        }

        GcManager gc(db_);
        const uint64_t total_pages = db_->total_pages();
        uint64_t start_page = 0;
        if (progress->last_page_cursor != 0 && progress->last_page_cursor < total_pages)
        {
            start_page = progress->last_page_cursor + 1;
        }

        LOG_INFO(VACUUM,
                 "Starting foreground reclaim: generation=%lu, start_horizon=%lu, page_cursor=%lu, new_oit=%lu",
                 progress->generation_id,
                 progress->start_horizon,
                 progress->last_page_cursor,
                 new_oit);

        for (uint64_t page_id = start_page; page_id < total_pages; ++page_id)
        {
            if (!db_->page_manager()->isAllocated(static_cast<uint32_t>(page_id)))
            {
                progress->last_page_cursor = page_id;
                Status persist_status = persistSweepProgressState(*progress, ctx);
                if (persist_status != Status::OK)
                {
                    return persist_status;
                }
                continue;
            }

            void *page_buffer = nullptr;
            Status pin_status = buffer_pool_->pinPage(static_cast<uint32_t>(page_id),
                                                      &page_buffer,
                                                      ctx,
                                                      BufferPool::AccessStrategy::Vacuum);
            if (pin_status != Status::OK)
            {
                progress->last_page_cursor = page_id;
                Status persist_status = persistSweepProgressState(*progress, ctx);
                if (persist_status != Status::OK)
                {
                    return persist_status;
                }
                continue;
            }

            auto *page_header = static_cast<PageHeader *>(page_buffer);
            if (page_header->page_type != PAGE_TYPE_HEAP)
            {
                buffer_pool_->unpinPage(static_cast<uint32_t>(page_id), false, ctx);
                progress->last_page_cursor = page_id;
                Status persist_status = persistSweepProgressState(*progress, ctx);
                if (persist_status != Status::OK)
                {
                    return persist_status;
                }
                continue;
            }

            const auto *special = reinterpret_cast<const HeapPageSpecial *>(
                static_cast<const uint8_t *>(page_buffer) + db_->page_size() - sizeof(HeapPageSpecial));
            const ID table_id = (special != nullptr) ? special->table_id : ID{};
            buffer_pool_->unpinPage(static_cast<uint32_t>(page_id), false, ctx);

            GcStats page_stats;
            ErrorContext page_ctx;
            Status gc_status = gc.gcPage(table_id, static_cast<uint32_t>(page_id), &page_stats, &page_ctx);
            if (gc_status == Status::DATA_CORRUPTED || gc_status == Status::PAGE_CORRUPT)
            {
                LOG_WARNING(VACUUM,
                            "Foreground reclaim skipped page %lu because chain repair was required: %s",
                            page_id,
                            page_ctx.message.c_str());
            }
            else if (gc_status != Status::OK)
            {
                return gc_status;
            }
            else
            {
                progress->last_relation_id = table_id;
                progress->reclaimed_version_count +=
                    page_stats.dead_tuples_removed + page_stats.version_chains_pruned;
                progress->reclaimed_bytes += page_stats.free_space_recovered;
            }

            progress->last_page_cursor = page_id;
            Status persist_status = persistSweepProgressState(*progress, ctx);
            if (persist_status != Status::OK)
            {
                return persist_status;
            }
        }

        return Status::OK;
    }

    Status SweepManager::emitLocalEvidenceForSweep(uint64_t oit_before,
                                                   uint64_t oit_after,
                                                   uint16_t lane_mask,
                                                   bool strict_audit,
                                                   uint64_t* evidence_items_emitted,
                                                   bool* prune_blocked_out,
                                                   ErrorContext* ctx)
    {
        if (evidence_items_emitted)
        {
            *evidence_items_emitted = 0;
        }
        if (prune_blocked_out)
        {
            *prune_blocked_out = false;
        }

        if (!db_ || !db_->catalog_manager())
        {
            return Status::OK;
        }

        const std::vector<SweepPolicyLane> lanes = decodeSweepPolicyLaneMask(lane_mask);
        if (!lanesRequireLocalEvidence(lanes))
        {
            return Status::OK;
        }

        Status status = Status::OK;
        ID sink_profile_id{};
        if (isZeroIdLocal(sink_profile_id))
        {
            std::vector<CatalogManager::AuditSinkProfileCatalogInfo> profiles;
            status = db_->catalog_manager()->listAuditSinkProfileCatalogEntries(profiles, ctx);
            if (status != Status::OK)
            {
                if (prune_blocked_out)
                {
                    *prune_blocked_out = true;
                }
                return status;
            }

            auto existing = std::find_if(
                profiles.begin(), profiles.end(), [](const auto& profile) {
                    return profile.profile_name == kSweepEvidenceProfileName;
                });

            CatalogManager::AuditSinkProfileCatalogInfo profile{};
            if (existing != profiles.end())
            {
                profile = *existing;
            }
            else
            {
                profile.audit_sink_profile_id = generateUuidV7();
                profile.profile_name = kSweepEvidenceProfileName;
                profile.sink_type = "LOCAL_APPEND_ONLY";
            }
            profile.failure_policy = strict_audit ? "STRICT_AUDIT" : "BEST_EFFORT";
            profile.is_enabled = true;

            const std::string lanes_csv = encodeSweepPolicyLanes(lanes);
            const auto spool_root = buildSweepSpoolRoot(db_);
            profile.config_json =
                "{\"profile_kind\":\"SWEEP_LOCAL_EVIDENCE\",\"queue_root\":\"" +
                spool_root.string() + "\",\"policy_lanes\":\"" + lanes_csv + "\"}";

            status = db_->catalog_manager()->upsertAuditSinkProfileCatalogEntry(profile, ctx);
            if (status != Status::OK)
            {
                if (prune_blocked_out)
                {
                    *prune_blocked_out = true;
                }
                return status;
            }
            sink_profile_id = profile.audit_sink_profile_id;
        }
        else
        {
            CatalogManager::AuditSinkProfileCatalogInfo profile{};
            status = db_->catalog_manager()->getAuditSinkProfileCatalogEntry(
                sink_profile_id, profile, ctx);
            if (status != Status::OK)
            {
                if (prune_blocked_out)
                {
                    *prune_blocked_out = true;
                }
                return status;
            }
        }

        std::filesystem::path spool_root;
        status = ensureSweepSpoolRoot(db_, spool_root, ctx);
        if (status != Status::OK)
        {
            if (prune_blocked_out)
            {
                *prune_blocked_out = true;
            }
            return status;
        }

        std::vector<CatalogManager::AuditExportSegmentCatalogInfo> prior_segments;
        status = db_->catalog_manager()->listAuditExportSegmentCatalogEntries(
            sink_profile_id, prior_segments, ctx);
        if (status != Status::OK)
        {
            if (prune_blocked_out)
            {
                *prune_blocked_out = true;
            }
            return status;
        }

        uint64_t next_segment_seq = 1;
        std::array<uint8_t, 32> prior_segment_hash{};
        if (!prior_segments.empty())
        {
            next_segment_seq = prior_segments.back().segment_seq + 1;
            prior_segment_hash = prior_segments.back().hash_curr;
        }

        std::vector<SweepEvidenceWorkItem> existing_items;
        status = listEvidenceWorkItems(existing_items, ctx);
        if (status != Status::OK)
        {
            if (prune_blocked_out)
            {
                *prune_blocked_out = true;
            }
            return status;
        }

        std::vector<std::string> existing_tx_uuids;
        existing_tx_uuids.reserve(existing_items.size());
        for (const auto& item : existing_items)
        {
            existing_tx_uuids.push_back(item.tx_uuid.toString());
        }

        std::vector<SweepLineageGroup> groups;
        status = loadSweepLineageGroups(db_->catalog_manager(), oit_before, oit_after, groups, ctx);
        if (status != Status::OK)
        {
            if (prune_blocked_out)
            {
                *prune_blocked_out = true;
            }
            return status;
        }

        for (const auto& group : groups)
        {
            const std::string tx_uuid_text = group.tx_uuid.toString();
            if (std::find(existing_tx_uuids.begin(), existing_tx_uuids.end(), tx_uuid_text) !=
                existing_tx_uuids.end())
            {
                continue;
            }
            if (group.events.empty() || !hasTerminalEvent(group))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                  "SWEEP_AUDIT_MANIFEST_MISSING: transaction lineage is incomplete");
                if (prune_blocked_out)
                {
                    *prune_blocked_out = true;
                }
                return Status::DATA_CORRUPTED;
            }

            const ID work_item_id = generateUuidV7();
            const std::string delivery_state =
                lanesRequireDownstreamQueue(lanes) ? "REMOTE_PENDING" : "LOCAL_COMMITTED";
            const uint64_t created_time = currentSystemMicros();
            const std::filesystem::path spool_path =
                spool_root / (std::to_string(group.txid) + "-" + tx_uuid_text + ".sbspool");
            const std::string manifest = buildSweepManifest(db_,
                                                            group,
                                                            lanes,
                                                            sink_profile_id,
                                                            oit_before,
                                                            oit_after,
                                                            work_item_id,
                                                            spool_path,
                                                            delivery_state,
                                                            created_time);

            status = writeDurableTextFile(spool_path, manifest, ctx);
            if (status != Status::OK)
            {
                if (prune_blocked_out)
                {
                    *prune_blocked_out = true;
                }
                return status;
            }

            CatalogManager::AuditExportSegmentCatalogInfo segment{};
            segment.audit_export_segment_id = work_item_id;
            segment.audit_sink_profile_id = sink_profile_id;
            segment.evidence_class = chooseEvidenceClass(lanes);
            segment.segment_seq = next_segment_seq++;
            segment.range_start_time = group.first_time;
            segment.range_end_time = group.last_time;
            segment.payload_manifest = manifest;
            segment.hash_prev = prior_segment_hash;
            segment.hash_curr =
                AuditLogger::computeExportSegmentHash(segment.payload_manifest, segment.hash_prev);
            segment.delivery_state = delivery_state;
            segment.is_valid = true;
            segment.created_time = created_time;

            status = db_->catalog_manager()->appendAuditExportSegmentCatalogEntry(segment, ctx);
            if (status != Status::OK)
            {
                if (prune_blocked_out)
                {
                    *prune_blocked_out = true;
                }
                return status;
            }

            prior_segment_hash = segment.hash_curr;
            existing_tx_uuids.push_back(tx_uuid_text);
            if (evidence_items_emitted)
            {
                ++(*evidence_items_emitted);
            }
        }

        return Status::OK;
    }

    Status SweepManager::emitPageSpotAuditFindings(bool foreground,
                                                   uint16_t lane_mask,
                                                   uint64_t* findings_emitted,
                                                   bool* mode_downgraded,
                                                   ErrorContext* ctx)
    {
        if (findings_emitted)
        {
            *findings_emitted = 0;
        }
        if (mode_downgraded)
        {
            *mode_downgraded = false;
        }

        if (!db_ || !db_->catalog_manager())
        {
            return Status::OK;
        }

        if (!hasSweepPolicyLaneInMask(lane_mask, SweepPolicyLane::PAGE_SPOT_AUDIT))
        {
            return Status::OK;
        }

        Status status = Status::OK;

        const std::string scan_mode = foreground ? "LIGHT" : "DIAGNOSTIC";
        const std::string trigger_source = foreground ? "SWEEP_FOREGROUND" : "SWEEP_BACKGROUND";
        if (foreground && mode_downgraded)
        {
            *mode_downgraded = true;
        }

        std::vector<uint8_t> page_buffer(db_->page_size());
        PageManager* page_manager = db_->page_manager();
        if (page_manager == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "PageManager not available for page audit");
            return Status::INVALID_ARGUMENT;
        }

        const uint64_t total_pages = db_->total_pages();
        for (uint32_t page_id = 0; page_id < total_pages; ++page_id)
        {
            if (!page_manager->isAllocated(page_id))
            {
                continue;
            }

            ErrorContext page_ctx;
            Status read_status =
                db_->read_page_partial(page_id, page_buffer.data(), db_->page_size(), 0, &page_ctx);
            if (read_status != Status::OK)
            {
                CatalogManager::PageAuditFindingCatalogInfo finding{};
                finding.finding_id = generateUuidV7();
                finding.scan_mode = scan_mode;
                finding.trigger_source = trigger_source;
                finding.page_id = page_id;
                finding.page_type = "UNKNOWN";
                finding.error_code = "PAGE_HEADER_CORRUPT";
                finding.severity = "CRITICAL";
                finding.details_json = makePageAuditDetails({
                    {"status", std::to_string(static_cast<int>(read_status))},
                    {"message", page_ctx.message},
                });
                status = db_->catalog_manager()->appendPageAuditFindingCatalogEntry(finding, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                if (findings_emitted)
                {
                    ++(*findings_emitted);
                }
                continue;
            }

            const auto* header = reinterpret_cast<const PageHeader*>(page_buffer.data());
            std::vector<PageAuditObservation> observations;
            const std::string page_type_text = pageTypeToAuditString(header->page_type);

            Status header_status =
                validatePageHeaderContract(*header, db_->page_size(), 0xFFFFu, nullptr, nullptr);
            if (header_status != Status::OK)
            {
                appendPageAuditObservation(
                    observations,
                    page_id,
                    page_type_text,
                    "PAGE_HEADER_CORRUPT",
                    "CRITICAL",
                    makePageAuditDetails({
                        {"status", std::to_string(static_cast<int>(header_status))},
                        {"page_type_raw", std::to_string(header->page_type)},
                    }));
            }
            else
            {
                if (!isRecognizedPageTypeForAudit(header->page_type))
                {
                    appendPageAuditObservation(
                        observations,
                        page_id,
                        page_type_text,
                        "PAGE_TYPE_INVALID",
                        "ERROR",
                        makePageAuditDetails({
                            {"page_type_raw", std::to_string(header->page_type)},
                        }));
                }

                if (!validatePageChecksum(page_buffer.data(), db_->page_size()))
                {
                    appendPageAuditObservation(
                        observations,
                        page_id,
                        page_type_text,
                        "PAGE_CHECKSUM_FAIL",
                        "ERROR",
                        makePageAuditDetails({
                            {"stored_checksum", std::to_string(header->checksum)},
                            {"computed_checksum",
                             std::to_string(calculatePageChecksum(page_buffer.data(), db_->page_size()))},
                        }));
                }

                if (scan_mode == "DIAGNOSTIC")
                {
                    if (header->page_type == PAGE_TYPE_HEAP ||
                        header->page_type == PAGE_TYPE_TOAST_CHUNK ||
                        header->page_type == PAGE_TYPE_LOB_CHUNK)
                    {
                        HeapToastLobDiagnosticReport report{};
                        ErrorContext diag_ctx;
                        Status diag_status = HeapToastLobDiagnostics::walkPage(
                            page_buffer.data(), db_->page_size(), &report, &diag_ctx);
                        if (diag_status != Status::OK && !report.issues.empty())
                        {
                            bool saw_slot = false;
                            bool saw_record = false;
                            bool saw_lob = false;
                            for (const auto& issue : report.issues)
                            {
                                switch (issue.code)
                                {
                                    case HeapToastLobIssueCode::INVALID_ITEM_POINTER:
                                        if (!saw_slot)
                                        {
                                            appendPageAuditObservation(
                                                observations,
                                                page_id,
                                                page_type_text,
                                                "SLOT_DIRECTORY_CORRUPT",
                                                "ERROR",
                                                makePageAuditDetails({
                                                    {"item_id", std::to_string(issue.item_id)},
                                                    {"offset", std::to_string(issue.offset)},
                                                }));
                                            saw_slot = true;
                                        }
                                        break;
                                    case HeapToastLobIssueCode::INVALID_TUPLE_HEADER:
                                    case HeapToastLobIssueCode::INVALID_PAYLOAD_LENGTH:
                                        if (!saw_record)
                                        {
                                            appendPageAuditObservation(
                                                observations,
                                                page_id,
                                                page_type_text,
                                                "RECORD_CHAIN_CORRUPT",
                                                "ERROR",
                                                makePageAuditDetails({
                                                    {"item_id", std::to_string(issue.item_id)},
                                                    {"offset", std::to_string(issue.offset)},
                                                }));
                                            saw_record = true;
                                        }
                                        break;
                                    case HeapToastLobIssueCode::INVALID_TOAST_POINTER:
                                    case HeapToastLobIssueCode::TOAST_FLAG_MISMATCH:
                                    case HeapToastLobIssueCode::LOB_CHUNK_MISSING:
                                        if (!saw_lob)
                                        {
                                            appendPageAuditObservation(
                                                observations,
                                                page_id,
                                                page_type_text,
                                                "LOB_CHAIN_CORRUPT",
                                                "ERROR",
                                                makePageAuditDetails({
                                                    {"item_id", std::to_string(issue.item_id)},
                                                    {"offset", std::to_string(issue.offset)},
                                                }));
                                            saw_lob = true;
                                        }
                                        break;
                                    default:
                                        break;
                                }
                            }
                        }
                    }
                    else if (isCanonicalIndexPageType(header->page_type))
                    {
                        if (header->special_size < sizeof(IndexPageHeader) ||
                            pageSpecial(*header) + sizeof(IndexPageHeader) > db_->page_size())
                        {
                            appendPageAuditObservation(
                                observations,
                                page_id,
                                page_type_text,
                                "INDEX_ENTRY_CORRUPT",
                                "ERROR",
                                makePageAuditDetails({
                                    {"special_size", std::to_string(header->special_size)},
                                    {"special_offset", std::to_string(pageSpecial(*header))},
                                }));
                        }
                        else
                        {
                            const auto* index_header = reinterpret_cast<const IndexPageHeader*>(
                                page_buffer.data() + pageSpecial(*header));
                            if (!isValidIndexPageHeaderBasic(*index_header, index_header->opaque_len) ||
                                !isValidIndexSiblingContract(*index_header))
                            {
                                appendPageAuditObservation(
                                    observations,
                                    page_id,
                                    page_type_text,
                                    "INDEX_ENTRY_CORRUPT",
                                    "ERROR",
                                    makePageAuditDetails({
                                        {"page_level", std::to_string(index_header->page_level)},
                                        {"flags", std::to_string(index_header->flags)},
                                    }));
                            }
                        }
                    }
                    else if (header->page_type == PAGE_TYPE_FSM_ROOT ||
                             header->page_type == PAGE_TYPE_FSM_PAGE)
                    {
                        if (pageLower(*header) < sizeof(PageHeader) + sizeof(uint32_t) * 3)
                        {
                            appendPageAuditObservation(
                                observations,
                                page_id,
                                page_type_text,
                                "FSM_MISMATCH",
                                "WARNING",
                                makePageAuditDetails({
                                    {"lower", std::to_string(pageLower(*header))},
                                }));
                        }
                        else
                        {
                            struct FsmAuditPayload
                            {
                                uint32_t total_pages;
                                uint32_t free_pages;
                                uint32_t next_fsm_page;
                            };
                            const auto* fsm_payload = reinterpret_cast<const FsmAuditPayload*>(
                                page_buffer.data() + sizeof(PageHeader));
                            if (fsm_payload->total_pages == 0 ||
                                fsm_payload->free_pages > fsm_payload->total_pages)
                            {
                                appendPageAuditObservation(
                                    observations,
                                    page_id,
                                    page_type_text,
                                    "FSM_MISMATCH",
                                    "WARNING",
                                    makePageAuditDetails({
                                        {"total_pages", std::to_string(fsm_payload->total_pages)},
                                        {"free_pages", std::to_string(fsm_payload->free_pages)},
                                    }));
                            }
                        }
                    }
                }
            }

            for (auto& observation : observations)
            {
                CatalogManager::PageAuditFindingCatalogInfo finding{};
                finding.finding_id = generateUuidV7();
                finding.scan_mode = scan_mode;
                finding.trigger_source = trigger_source;
                finding.page_id = observation.page_id;
                finding.page_type = observation.page_type;
                finding.error_code = observation.error_code;
                finding.severity = observation.severity;
                finding.details_json = observation.details_json;
                status = db_->catalog_manager()->appendPageAuditFindingCatalogEntry(finding, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                if (findings_emitted)
                {
                    ++(*findings_emitted);
                }
            }
        }

        return Status::OK;
    }

    Status SweepManager::emitShadowCaptureManifests(uint64_t* manifests_emitted,
                                                    uint16_t lane_mask,
                                                    bool strict_audit,
                                                    ErrorContext* ctx)
    {
        if (manifests_emitted)
        {
            *manifests_emitted = 0;
        }

        if (!db_ || !db_->catalog_manager())
        {
            return Status::OK;
        }

        if (!hasSweepPolicyLaneInMask(lane_mask, SweepPolicyLane::SHADOW_CAPTURE))
        {
            return Status::OK;
        }

        Status status = Status::OK;

        std::filesystem::path shadow_root;
        status = ensureShadowCaptureRoot(db_, shadow_root, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<CatalogManager::AuditSinkProfileCatalogInfo> profiles;
        status = db_->catalog_manager()->listAuditSinkProfileCatalogEntries(profiles, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        CatalogManager::AuditSinkProfileCatalogInfo shadow_profile{};
        auto existing_profile =
            std::find_if(profiles.begin(), profiles.end(), [](const auto& profile) {
                return profile.profile_name == kShadowCaptureProfileName;
            });
        if (existing_profile != profiles.end())
        {
            shadow_profile = *existing_profile;
        }
        else
        {
            shadow_profile.audit_sink_profile_id = generateUuidV7();
            shadow_profile.profile_name = kShadowCaptureProfileName;
            shadow_profile.sink_type = "LOCAL_APPEND_ONLY";
        }
        shadow_profile.failure_policy = strict_audit ? "STRICT_AUDIT" : "BEST_EFFORT";
        shadow_profile.is_enabled = true;
        shadow_profile.config_json =
            "{\"profile_kind\":\"SWEEP_SHADOW_CAPTURE\",\"queue_root\":\"" +
            shadow_root.string() + "\",\"capture_format\":\"LOGICAL_TX_SUMMARY\"}";
        status = db_->catalog_manager()->upsertAuditSinkProfileCatalogEntry(shadow_profile, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SweepEvidenceWorkItem> evidence_items;
        status = listEvidenceWorkItems(evidence_items, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SweepShadowCaptureManifest> existing_manifests;
        status = listShadowCaptureManifests(existing_manifests, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<std::string> captured_source_ids;
        captured_source_ids.reserve(existing_manifests.size());
        for (const auto& manifest : existing_manifests)
        {
            std::string source_work_item_uuid;
            if (!extractManifestField(manifest.payload_manifest,
                                      "source_work_item_uuid",
                                      source_work_item_uuid))
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::DATA_CORRUPTED,
                                  "Persisted shadow capture manifest is missing source_work_item_uuid");
                return Status::DATA_CORRUPTED;
            }
            captured_source_ids.push_back(std::move(source_work_item_uuid));
        }

        for (const auto& item : evidence_items)
        {
            std::vector<SweepPolicyLane> lanes;
            if (!parseSweepPolicyLanes(item.lanes_csv, lanes))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                  "Persisted sweep manifest carries invalid policy_lanes");
                return Status::DATA_CORRUPTED;
            }
            if (!hasSweepPolicyLane(lanes, SweepPolicyLane::SHADOW_CAPTURE))
            {
                continue;
            }
            if (std::find(captured_source_ids.begin(),
                          captured_source_ids.end(),
                          item.work_item_id.toString()) != captured_source_ids.end())
            {
                continue;
            }

            CatalogManager::AuditExportSegmentCatalogInfo source_segment{};
            status = db_->catalog_manager()->getAuditExportSegmentCatalogEntry(
                item.work_item_id, source_segment, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            ShadowCapturePayload payload{};
            status = loadShadowCapturePayload(
                db_->catalog_manager(), item.tx_uuid, item.txid, payload, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            const std::string capture_scope =
                isZeroIdLocal(payload.primary_object_uuid) ? "TRANSACTION" : "OBJECT";
            const std::string capture_format = "LOGICAL_TX_SUMMARY";
            const ID manifest_id = generateUuidV7();
            const uint64_t created_time = currentSystemMicros();
            const std::filesystem::path shadow_path =
                shadow_root /
                (std::to_string(item.segment_seq) + "-" + std::to_string(item.txid) + "-" +
                 item.tx_uuid.toString() + ".sbshadow");
            const std::string manifest = buildShadowCaptureManifest(db_,
                                                                    item,
                                                                    source_segment,
                                                                    payload,
                                                                    shadow_profile.audit_sink_profile_id,
                                                                    manifest_id,
                                                                    shadow_path,
                                                                    created_time,
                                                                    capture_scope,
                                                                    capture_format);

            status = writeDurableTextFile(shadow_path, manifest, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx,
                                  status,
                                  "SWEEP_SHADOW_CAPTURE_BLOCKED: failed to persist logical shadow capture");
                return status;
            }

            CatalogManager::ShadowCaptureManifestCatalogInfo manifest_info{};
            manifest_info.manifest_id = manifest_id;
            manifest_info.tx_uuid = item.tx_uuid;
            manifest_info.object_uuid = payload.primary_object_uuid;
            manifest_info.capture_scope = capture_scope;
            manifest_info.capture_format = capture_format;
            manifest_info.sink_profile_uuid = shadow_profile.audit_sink_profile_id;
            manifest_info.payload_manifest = manifest;
            manifest_info.created_time = created_time;
            manifest_info.is_valid = true;
            status = db_->catalog_manager()->appendShadowCaptureManifestCatalogEntry(
                manifest_info, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            captured_source_ids.push_back(item.work_item_id.toString());
            if (manifests_emitted)
            {
                ++(*manifests_emitted);
            }
        }

        return Status::OK;
    }

    Status SweepManager::emitDerivativeWalAfterLog(uint64_t* segments_emitted,
                                                   uint64_t* backlog_depth,
                                                   uint16_t lane_mask,
                                                   ErrorContext* ctx)
    {
        if (segments_emitted)
        {
            *segments_emitted = 0;
        }
        if (backlog_depth)
        {
            *backlog_depth = 0;
        }

        if (!db_ || !db_->catalog_manager())
        {
            return Status::OK;
        }

        if (!hasSweepPolicyLaneInMask(lane_mask, SweepPolicyLane::WAL_AFTER_EXPORT))
        {
            return Status::OK;
        }

        Status status = Status::OK;

        std::filesystem::path wal_root = buildWalAfterLogRoot(db_);

        std::vector<CatalogManager::AuditSinkProfileCatalogInfo> profiles;
        status = db_->catalog_manager()->listAuditSinkProfileCatalogEntries(profiles, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        ID wal_sink_profile_id{};
        auto existing_profile = std::find_if(
            profiles.begin(), profiles.end(), [](const auto& profile) {
                return profile.profile_name == kWalAfterLogProfileName;
            });
        CatalogManager::AuditSinkProfileCatalogInfo wal_profile{};
        if (existing_profile != profiles.end())
        {
            wal_profile = *existing_profile;
        }
        else
        {
            wal_profile.audit_sink_profile_id = generateUuidV7();
            wal_profile.profile_name = kWalAfterLogProfileName;
            wal_profile.sink_type = "LOCAL_APPEND_ONLY";
        }
        wal_profile.failure_policy = "BEST_EFFORT";
        wal_profile.is_enabled = true;
        wal_profile.config_json =
            "{\"profile_kind\":\"SWEEP_WAL_AFTER_LOG\",\"queue_root\":\"" +
            wal_root.string() + "\",\"shipping_mode\":\"DEBUG\"}";
        status = db_->catalog_manager()->upsertAuditSinkProfileCatalogEntry(wal_profile, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        wal_sink_profile_id = wal_profile.audit_sink_profile_id;

        std::vector<SweepEvidenceWorkItem> evidence_items;
        status = listEvidenceWorkItems(evidence_items, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SweepWalAfterLogSegment> existing_wal_segments;
        status = listWalAfterLogSegments(existing_wal_segments, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<std::string> exported_source_ids;
        exported_source_ids.reserve(existing_wal_segments.size());
        for (const auto& segment : existing_wal_segments)
        {
            exported_source_ids.push_back(segment.source_work_item_id.toString());
        }

        uint64_t next_stream_seq = 1;
        std::array<uint8_t, 32> prior_segment_hash{};
        std::vector<CatalogManager::AuditExportSegmentCatalogInfo> prior_segments;
        status = db_->catalog_manager()->listAuditExportSegmentCatalogEntries(
            wal_sink_profile_id, prior_segments, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (!prior_segments.empty())
        {
            next_stream_seq = prior_segments.back().segment_seq + 1;
            prior_segment_hash = prior_segments.back().hash_curr;
        }

        uint64_t pending_count = 0;
        for (const auto& item : evidence_items)
        {
            std::vector<SweepPolicyLane> lanes;
            if (!parseSweepPolicyLanes(item.lanes_csv, lanes))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                  "Persisted sweep manifest carries invalid policy_lanes");
                return Status::DATA_CORRUPTED;
            }
            if (!hasSweepPolicyLane(lanes, SweepPolicyLane::WAL_AFTER_EXPORT))
            {
                continue;
            }
            if (std::find(exported_source_ids.begin(),
                          exported_source_ids.end(),
                          item.work_item_id.toString()) == exported_source_ids.end())
            {
                ++pending_count;
            }
        }

        status = ensureWalAfterLogRoot(db_, wal_root, ctx);
        if (status != Status::OK)
        {
            if (backlog_depth)
            {
                *backlog_depth = pending_count;
            }
            return status;
        }

        for (const auto& item : evidence_items)
        {
            std::vector<SweepPolicyLane> lanes;
            if (!parseSweepPolicyLanes(item.lanes_csv, lanes))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                  "Persisted sweep manifest carries invalid policy_lanes");
                return Status::DATA_CORRUPTED;
            }
            if (!hasSweepPolicyLane(lanes, SweepPolicyLane::WAL_AFTER_EXPORT))
            {
                continue;
            }
            if (std::find(exported_source_ids.begin(),
                          exported_source_ids.end(),
                          item.work_item_id.toString()) != exported_source_ids.end())
            {
                continue;
            }

            CatalogManager::AuditExportSegmentCatalogInfo source_segment{};
            status = db_->catalog_manager()->getAuditExportSegmentCatalogEntry(
                item.work_item_id, source_segment, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            WalAfterLogPayload payload{};
            ErrorContext payload_ctx;
            status = loadCommittedWalAfterLogPayload(
                db_->catalog_manager(), item.tx_uuid, item.txid, payload, &payload_ctx);
            if (status == Status::NOT_FOUND)
            {
                --pending_count;
                continue;
            }
            if (status != Status::OK)
            {
                if (ctx && payload_ctx.code != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, payload_ctx.code, payload_ctx.message.c_str());
                    if (!payload_ctx.vnext_code.empty())
                    {
                        ctx->setVNextCode(payload_ctx.vnext_code.c_str());
                    }
                    if (!payload_ctx.sqlstate_text.empty())
                    {
                        ctx->setSQLState(payload_ctx.sqlstate_text.c_str());
                    }
                }
                return status;
            }

            WalAfterLogSourceItem source{};
            source.work_item_id = item.work_item_id;
            source.source_sink_profile_id = item.sink_profile_id;
            source.tx_uuid = item.tx_uuid;
            source.txid = item.txid;
            source.source_segment_seq = item.segment_seq;
            source.source_created_time = item.created_time;
            source.spool_path = item.spool_path;
            source.source_manifest = source_segment.payload_manifest;
            source.lanes = std::move(lanes);

            const ID segment_id = generateUuidV7();
            const uint64_t created_time = currentSystemMicros();
            const std::filesystem::path segment_path =
                wal_root /
                (std::to_string(next_stream_seq) + "-" + std::to_string(item.txid) + "-" +
                 item.tx_uuid.toString() + ".sbwal");
            const std::string manifest = buildWalAfterLogManifest(
                db_,
                source,
                payload,
                wal_sink_profile_id,
                segment_id,
                next_stream_seq,
                segment_path,
                created_time);

            status = writeDurableTextFile(segment_path, manifest, ctx);
            if (status != Status::OK)
            {
                if (backlog_depth)
                {
                    *backlog_depth = pending_count;
                }
                return status;
            }

            CatalogManager::AuditExportSegmentCatalogInfo segment{};
            segment.audit_export_segment_id = segment_id;
            segment.audit_sink_profile_id = wal_sink_profile_id;
            segment.evidence_class = "WAL_AFTER_LOG";
            segment.segment_seq = next_stream_seq;
            segment.range_start_time = payload.commit_time;
            segment.range_end_time = payload.commit_time;
            segment.payload_manifest = manifest;
            segment.hash_prev = prior_segment_hash;
            segment.hash_curr =
                AuditLogger::computeExportSegmentHash(segment.payload_manifest, segment.hash_prev);
            segment.delivery_state = "LOCAL_COMMITTED";
            segment.is_valid = true;
            segment.created_time = created_time;
            status = db_->catalog_manager()->appendAuditExportSegmentCatalogEntry(segment, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            prior_segment_hash = segment.hash_curr;
            exported_source_ids.push_back(item.work_item_id.toString());
            ++next_stream_seq;
            --pending_count;
            if (segments_emitted)
            {
                ++(*segments_emitted);
            }
        }

        if (backlog_depth)
        {
            *backlog_depth = pending_count;
        }
        return Status::OK;
    }

    void SweepManager::updateStatistics(uint64_t oit_before, uint64_t oit_after,
                                        uint64_t duration_ms,
                                        uint64_t evidence_items_emitted,
                                        uint64_t wal_after_segments_emitted,
                                        uint64_t wal_after_backlog_depth,
                                        uint64_t page_audit_findings_emitted,
                                        uint64_t shadow_capture_manifests_emitted,
                                        bool prune_blocked,
                                        bool evidence_failure,
                                        bool page_audit_failure,
                                        bool page_audit_mode_downgraded,
                                        bool shadow_capture_failure,
                                        bool wal_after_failure)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);

        stats_.sweep_count++;
        stats_.last_sweep_time = currentSystemMicros();
        stats_.last_sweep_duration_ms = duration_ms;
        stats_.last_oit_before = oit_before;
        stats_.last_oit_after = oit_after;
        stats_.total_transactions_swept += (oit_after - oit_before);
        stats_.total_evidence_items_emitted += evidence_items_emitted;
        stats_.last_evidence_items_emitted = evidence_items_emitted;
        stats_.total_wal_after_segments_emitted += wal_after_segments_emitted;
        stats_.last_wal_after_segments_emitted = wal_after_segments_emitted;
        stats_.wal_after_backlog_depth = wal_after_backlog_depth;
        stats_.total_page_audit_findings_emitted += page_audit_findings_emitted;
        stats_.last_page_audit_findings_emitted = page_audit_findings_emitted;
        stats_.total_shadow_capture_manifests_emitted += shadow_capture_manifests_emitted;
        stats_.last_shadow_capture_manifests_emitted = shadow_capture_manifests_emitted;
        stats_.prune_blocked = prune_blocked;
        if (evidence_failure)
        {
            stats_.evidence_persist_failures++;
        }
        if (page_audit_failure)
        {
            stats_.page_audit_persist_failures++;
        }
        if (page_audit_mode_downgraded)
        {
            stats_.page_audit_mode_downgrades++;
        }
        if (shadow_capture_failure)
        {
            stats_.shadow_capture_failures++;
        }
        if (wal_after_failure)
        {
            stats_.wal_after_export_failures++;
        }
        stats_.sweep_in_progress = false;

        LOG_DEBUG(VACUUM,
                  "Statistics updated: count=%lu, transactions_swept=%lu, evidence_items=%lu, page_audit_findings=%lu, shadow_manifests=%lu, wal_after_segments=%lu, wal_after_backlog=%lu, prune_blocked=%d",
                  stats_.sweep_count, stats_.total_transactions_swept,
                  stats_.total_evidence_items_emitted,
                  stats_.total_page_audit_findings_emitted,
                  stats_.total_shadow_capture_manifests_emitted,
                  stats_.total_wal_after_segments_emitted,
                  stats_.wal_after_backlog_depth,
                  prune_blocked ? 1 : 0);
    }

    SweepStatistics SweepManager::getStatistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

} // namespace scratchbird::core
