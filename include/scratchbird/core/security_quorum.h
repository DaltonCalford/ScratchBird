#pragma once

#include <cstdint>
#include <functional>
#include <optional>

namespace scratchbird::core
{
    enum class QuorumFailureMode : uint8_t
    {
        FAIL_OPEN = 0,
        FAIL_CLOSED = 1,
        REQUIRE_REMOTE = 2
    };

    struct SecurityQuorumConfig
    {
        uint32_t required = 1;
        uint32_t total = 1;
        QuorumFailureMode failure_mode = QuorumFailureMode::FAIL_OPEN;
    };

    class SecurityQuorum
    {
    public:
        enum class Decision : uint8_t
        {
            ALLOW_CACHE = 0,
            BYPASS_CACHE = 1,
            DENY = 2
        };

        SecurityQuorum() = default;
        explicit SecurityQuorum(const SecurityQuorumConfig& config);

        void configure(const SecurityQuorumConfig& config);
        SecurityQuorumConfig config() const;

        void setStatusProvider(std::function<std::optional<bool>()> provider);

        Decision evaluate() const;

    private:
        SecurityQuorumConfig config_{};
        std::function<std::optional<bool>()> status_provider_;
    };
} // namespace scratchbird::core
