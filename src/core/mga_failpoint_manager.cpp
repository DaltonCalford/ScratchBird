#include "scratchbird/core/mga_failpoint_manager.h"

#include "scratchbird/core/database.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <unordered_set>

namespace scratchbird::core
{

    namespace
    {
        auto isZeroId(const ID& id) -> bool
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

        auto nowMillis() -> uint64_t
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());
        }
    } // namespace

    MgaFailpointManager::MgaFailpointManager(Database* db) : db_(db) {}

    auto MgaFailpointManager::installSeed(const std::string& seed_id,
                                          const std::vector<MgaFailpointDefinition>& definitions,
                                          ErrorContext* ctx) -> Status
    {
        std::unordered_set<std::string> seen;
        std::vector<RuntimeDefinition> runtime;
        runtime.reserve(definitions.size());

        for (const auto& definition : definitions)
        {
            if (definition.trigger_name.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Failpoint trigger name cannot be empty");
                return Status::INVALID_ARGUMENT;
            }
            if (definition.fire_on_hit == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Failpoint fire_on_hit must be >= 1");
                return Status::INVALID_ARGUMENT;
            }
            if (!seen.insert(definition.trigger_name).second)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Duplicate failpoint trigger in seed installation");
                return Status::INVALID_ARGUMENT;
            }

            RuntimeDefinition runtime_def{};
            runtime_def.definition = definition;
            runtime.push_back(std::move(runtime_def));
        }

        std::lock_guard<std::mutex> lock(mutex_);
        seed_id_ = seed_id;
        runtime_definitions_ = std::move(runtime);
        events_.clear();
        next_event_sequence_ = 1;
        return Status::OK;
    }

    auto MgaFailpointManager::clear(ErrorContext* ctx) -> Status
    {
        (void)ctx;
        std::lock_guard<std::mutex> lock(mutex_);
        resetLocked();
        return Status::OK;
    }

    auto MgaFailpointManager::clearEvents(ErrorContext* ctx) -> Status
    {
        (void)ctx;
        std::lock_guard<std::mutex> lock(mutex_);
        events_.clear();
        next_event_sequence_ = 1;
        for (auto& runtime : runtime_definitions_)
        {
            runtime.hit_count = 0;
            runtime.fired = false;
        }
        return Status::OK;
    }

    auto MgaFailpointManager::trip(std::string_view trigger_name,
                                   const MgaFailpointInvocation& invocation,
                                   ErrorContext* ctx) -> Status
    {
        RuntimeDefinition runtime{};
        MgaFailpointEvent event{};
        bool should_fire = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = std::find_if(runtime_definitions_.begin(),
                                   runtime_definitions_.end(),
                                   [trigger_name](const RuntimeDefinition& candidate) {
                                       return candidate.definition.trigger_name == trigger_name;
                                   });
            if (it == runtime_definitions_.end())
            {
                return Status::OK;
            }

            ++it->hit_count;
            if (it->fired || it->hit_count != it->definition.fire_on_hit)
            {
                return Status::OK;
            }

            it->fired = true;
            runtime = *it;

            event.seed_id = seed_id_;
            event.trigger_name = std::string(trigger_name);
            event.outcome = defaultOutcome(runtime);
            event.has_txid = invocation.has_txid;
            event.txid = invocation.txid;
            event.occurred_at_ms = nowMillis();
            if (db_ != nullptr && !isZeroId(db_->uuid()))
            {
                event.has_db_uuid = true;
                event.db_uuid = db_->uuid();
            }

            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%s:%06lu",
                          event.seed_id.empty() ? "mgw" : event.seed_id.c_str(),
                          static_cast<unsigned long>(next_event_sequence_++));
            event.event_id = buffer;
            events_.push_back(event);
            should_fire = true;
        }

        if (!should_fire)
        {
            return Status::OK;
        }

        if (runtime.definition.action == MgaFailpointAction::STALL_THEN_CONTINUE &&
            runtime.definition.stall_ms > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(runtime.definition.stall_ms));
        }

        if (runtime.definition.action == MgaFailpointAction::RETURN_ERROR)
        {
            if (ctx != nullptr)
            {
                std::string message = "MGA failpoint triggered: " + event.trigger_name;
                if (!event.seed_id.empty())
                {
                    message += " seed=" + event.seed_id;
                }
                ctx->set(runtime.definition.injected_status,
                         message.c_str(),
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return runtime.definition.injected_status;
        }

        return Status::OK;
    }

    auto MgaFailpointManager::listEvents(std::vector<MgaFailpointEvent>& events_out,
                                         ErrorContext* ctx) const -> Status
    {
        (void)ctx;
        std::lock_guard<std::mutex> lock(mutex_);
        events_out = events_;
        return Status::OK;
    }

    auto MgaFailpointManager::currentSeed(std::string& seed_out,
                                          ErrorContext* ctx) const -> Status
    {
        (void)ctx;
        std::lock_guard<std::mutex> lock(mutex_);
        seed_out = seed_id_;
        return Status::OK;
    }

    auto MgaFailpointManager::resetLocked() -> void
    {
        seed_id_.clear();
        runtime_definitions_.clear();
        events_.clear();
        next_event_sequence_ = 1;
    }

    auto MgaFailpointManager::defaultOutcome(const RuntimeDefinition& runtime) -> std::string
    {
        if (!runtime.definition.outcome.empty())
        {
            return runtime.definition.outcome;
        }

        switch (runtime.definition.action)
        {
            case MgaFailpointAction::RETURN_ERROR:
                return "injected_error";
            case MgaFailpointAction::MARK_ONLY:
                return "marked";
            case MgaFailpointAction::STALL_THEN_CONTINUE:
                return "stalled";
        }
        return "unknown";
    }

} // namespace scratchbird::core
