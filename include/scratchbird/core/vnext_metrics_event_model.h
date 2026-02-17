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

#include "scratchbird/core/telemetry.h"

#include <mutex>
#include <string>

namespace scratchbird::core
{
    class VNextMetricsEventModel
    {
    public:
        static auto recordOptimizerEvent(const std::string& event,
                                         const std::string& outcome,
                                         const std::string& code = "NONE",
                                         double count = 1.0) -> void
        {
            if (count <= 0.0)
            {
                return;
            }
            if (auto* counter = optimizerCounter())
            {
                counter->inc(count, {event, outcome, normalizeCode(code)});
            }
        }

        static auto recordExecutorEvent(const std::string& event,
                                        const std::string& outcome,
                                        const std::string& code = "NONE",
                                        double count = 1.0) -> void
        {
            if (count <= 0.0)
            {
                return;
            }
            if (auto* counter = executorCounter())
            {
                counter->inc(count, {event, outcome, normalizeCode(code)});
            }
        }

        static auto recordSecurityEvent(const std::string& event,
                                        const std::string& outcome,
                                        const std::string& code = "NONE",
                                        double count = 1.0) -> void
        {
            if (count <= 0.0)
            {
                return;
            }
            if (auto* counter = securityCounter())
            {
                counter->inc(count, {event, outcome, normalizeCode(code)});
            }
        }

        static auto recordStorageEvent(const std::string& event,
                                       const std::string& outcome,
                                       const std::string& code = "NONE",
                                       double count = 1.0) -> void
        {
            if (count <= 0.0)
            {
                return;
            }
            if (auto* counter = storageCounter())
            {
                counter->inc(count, {event, outcome, normalizeCode(code)});
            }
        }

    private:
        static auto normalizeCode(const std::string& code) -> std::string
        {
            return code.empty() ? "NONE" : code;
        }

        static auto optimizerCounter() -> Counter*
        {
            static std::once_flag once;
            static Counter* counter = nullptr;
            std::call_once(once, [] {
                ScratchBirdMetrics::getInstance().initialize();
                counter = MetricsRegistry::getInstance().registerCounter(
                    "scratchbird_vnext_optimizer_events_total",
                    "vNext optimizer event counter",
                    {"event", "outcome", "code"});
            });
            return counter;
        }

        static auto executorCounter() -> Counter*
        {
            static std::once_flag once;
            static Counter* counter = nullptr;
            std::call_once(once, [] {
                ScratchBirdMetrics::getInstance().initialize();
                counter = MetricsRegistry::getInstance().registerCounter(
                    "scratchbird_vnext_executor_events_total",
                    "vNext executor event counter",
                    {"event", "outcome", "code"});
            });
            return counter;
        }

        static auto securityCounter() -> Counter*
        {
            static std::once_flag once;
            static Counter* counter = nullptr;
            std::call_once(once, [] {
                ScratchBirdMetrics::getInstance().initialize();
                counter = MetricsRegistry::getInstance().registerCounter(
                    "scratchbird_vnext_security_events_total",
                    "vNext security event counter",
                    {"event", "outcome", "code"});
            });
            return counter;
        }

        static auto storageCounter() -> Counter*
        {
            static std::once_flag once;
            static Counter* counter = nullptr;
            std::call_once(once, [] {
                ScratchBirdMetrics::getInstance().initialize();
                counter = MetricsRegistry::getInstance().registerCounter(
                    "scratchbird_vnext_storage_events_total",
                    "vNext storage event counter",
                    {"event", "outcome", "code"});
            });
            return counter;
        }
    };
} // namespace scratchbird::core

