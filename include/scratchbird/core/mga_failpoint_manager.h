#pragma once

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace scratchbird::core
{
    class Database;

    namespace MgaFailpointTriggers
    {
        inline constexpr std::string_view kAfterTxidAllocationBeforeActive =
            "after_txid_allocation_before_active";
        inline constexpr std::string_view kAfterDirtyFlushBeforeTipTerminal =
            "after_dirty_flush_before_tip_terminal";
        inline constexpr std::string_view kAfterTipTerminalBeforeClientAck =
            "after_tip_terminal_before_client_ack";
        inline constexpr std::string_view kBetweenPreparedRecordAndTipPrepared =
            "between_prepared_record_and_tip_prepared";
        inline constexpr std::string_view kAfterTipLoadBeforeActiveNormalization =
            "after_tip_load_before_active_normalization";
        inline constexpr std::string_view kAfterChainUnlinkBeforeCompactionPublish =
            "after_chain_unlink_before_compaction_publish";
        inline constexpr std::string_view kAfterHeapReclaimBeforeDeadEntryDelete =
            "after_heap_reclaim_before_dead_entry_delete";
        inline constexpr std::string_view kDeadlockDetectorStall =
            "deadlock_detector_stall";
        inline constexpr std::string_view kDeadlockVictimSelectionFailure =
            "deadlock_victim_selection_failure";
        inline constexpr std::string_view kSweepCheckpointWriteLoss =
            "sweep_checkpoint_write_loss";
    } // namespace MgaFailpointTriggers

    enum class MgaFailpointAction : uint8_t
    {
        RETURN_ERROR = 0,
        MARK_ONLY = 1,
        STALL_THEN_CONTINUE = 2,
    };

    struct MgaFailpointDefinition
    {
        std::string trigger_name;
        MgaFailpointAction action = MgaFailpointAction::RETURN_ERROR;
        uint64_t fire_on_hit = 1;
        Status injected_status = Status::IO_ERROR;
        uint32_t stall_ms = 0;
        std::string outcome;
    };

    struct MgaFailpointInvocation
    {
        bool has_txid = false;
        uint64_t txid = 0;
    };

    struct MgaFailpointEvent
    {
        std::string event_id;
        std::string seed_id;
        std::string trigger_name;
        std::string outcome;
        bool has_db_uuid = false;
        ID db_uuid{};
        bool has_txid = false;
        uint64_t txid = 0;
        uint64_t occurred_at_ms = 0;
    };

    class MgaFailpointManager
    {
    public:
        explicit MgaFailpointManager(Database* db);

        auto installSeed(const std::string& seed_id,
                         const std::vector<MgaFailpointDefinition>& definitions,
                         ErrorContext* ctx = nullptr) -> Status;

        auto clear(ErrorContext* ctx = nullptr) -> Status;
        auto clearEvents(ErrorContext* ctx = nullptr) -> Status;

        auto trip(std::string_view trigger_name,
                  const MgaFailpointInvocation& invocation = {},
                  ErrorContext* ctx = nullptr) -> Status;

        auto listEvents(std::vector<MgaFailpointEvent>& events_out,
                        ErrorContext* ctx = nullptr) const -> Status;

        auto currentSeed(std::string& seed_out,
                         ErrorContext* ctx = nullptr) const -> Status;

    private:
        struct RuntimeDefinition
        {
            MgaFailpointDefinition definition;
            uint64_t hit_count = 0;
            bool fired = false;
        };

        auto resetLocked() -> void;
        static auto defaultOutcome(const RuntimeDefinition& runtime) -> std::string;

        Database* db_ = nullptr;
        mutable std::mutex mutex_;
        std::string seed_id_;
        std::vector<RuntimeDefinition> runtime_definitions_;
        std::vector<MgaFailpointEvent> events_;
        uint64_t next_event_sequence_ = 1;
    };

} // namespace scratchbird::core
