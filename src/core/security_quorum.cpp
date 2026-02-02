/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/security_quorum.h"

namespace scratchbird::core
{
    SecurityQuorum::SecurityQuorum(const SecurityQuorumConfig& config)
        : config_(config)
    {
    }

    void SecurityQuorum::configure(const SecurityQuorumConfig& config)
    {
        config_ = config;
    }

    SecurityQuorumConfig SecurityQuorum::config() const
    {
        return config_;
    }

    void SecurityQuorum::setStatusProvider(std::function<std::optional<bool>()> provider)
    {
        status_provider_ = std::move(provider);
    }

    SecurityQuorum::Decision SecurityQuorum::evaluate() const
    {
        bool satisfied = false;
        if (status_provider_)
        {
            auto status = status_provider_();
            if (status.has_value())
            {
                satisfied = status.value();
            }
        }

        if (!satisfied)
        {
            if (config_.required <= 1 || config_.total <= 1)
            {
                satisfied = true;
            }
        }

        if (satisfied)
        {
            return Decision::ALLOW_CACHE;
        }

        switch (config_.failure_mode)
        {
            case QuorumFailureMode::FAIL_OPEN:
                return Decision::BYPASS_CACHE;
            case QuorumFailureMode::FAIL_CLOSED:
            case QuorumFailureMode::REQUIRE_REMOTE:
                return Decision::DENY;
            default:
                return Decision::DENY;
        }
    }
} // namespace scratchbird::core
