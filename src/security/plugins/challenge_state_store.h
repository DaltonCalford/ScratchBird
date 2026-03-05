/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "scratchbird/security/auth_plugin_abi_v1.h"

namespace scratchbird {
namespace security {
namespace plugins {
namespace challenge {

inline uint64_t monotonicNowMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

template <typename StateT>
class ExchangeStore {
public:
    enum class TakeStatus {
        Found,
        Missing,
        Expired,
    };

    explicit ExchangeStore(uint64_t ttl_ms = 300000) : ttl_ms_(ttl_ms) {}

    void setTtlMs(uint64_t ttl_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        ttl_ms_ = ttl_ms;
    }

    void put(sb_auth_exchange_t exchange, StateT state) {
        const uint64_t now_ms = monotonicNowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        pruneExpiredLocked(now_ms);
        entries_[exchange] = Entry{std::move(state), now_ms + ttl_ms_};
    }

    TakeStatus take(sb_auth_exchange_t exchange, StateT* out_state) {
        const uint64_t now_ms = monotonicNowMs();
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = entries_.find(exchange);
        if (it == entries_.end()) {
            pruneExpiredLocked(now_ms);
            return TakeStatus::Missing;
        }
        if (it->second.expires_at_ms <= now_ms) {
            entries_.erase(it);
            pruneExpiredLocked(now_ms);
            return TakeStatus::Expired;
        }

        if (out_state) {
            *out_state = std::move(it->second.state);
        }
        entries_.erase(it);
        pruneExpiredLocked(now_ms);
        return TakeStatus::Found;
    }

    bool erase(sb_auth_exchange_t exchange) {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.erase(exchange) > 0;
    }

    template <typename Predicate>
    std::size_t eraseIf(Predicate predicate) {
        const uint64_t now_ms = monotonicNowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        pruneExpiredLocked(now_ms);

        std::size_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (predicate(it->second.state)) {
                it = entries_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    std::size_t size() {
        const uint64_t now_ms = monotonicNowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        pruneExpiredLocked(now_ms);
        return entries_.size();
    }

    std::size_t pruneExpired() {
        const uint64_t now_ms = monotonicNowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        return pruneExpiredLocked(now_ms);
    }

private:
    struct Entry {
        StateT state;
        uint64_t expires_at_ms;
    };

    std::size_t pruneExpiredLocked(uint64_t now_ms) {
        std::size_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second.expires_at_ms <= now_ms) {
                it = entries_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    std::mutex mutex_;
    std::unordered_map<sb_auth_exchange_t, Entry> entries_;
    uint64_t ttl_ms_;
};

class ReplayCache {
public:
    explicit ReplayCache(uint64_t ttl_ms = 300000) : ttl_ms_(ttl_ms) {}

    void setTtlMs(uint64_t ttl_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        ttl_ms_ = ttl_ms;
    }

    bool remember(const std::string& replay_key) {
        if (replay_key.empty()) {
            return false;
        }

        const uint64_t now_ms = monotonicNowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        pruneExpiredLocked(now_ms);

        auto it = entries_.find(replay_key);
        if (it != entries_.end() && it->second > now_ms) {
            return false;
        }

        entries_[replay_key] = now_ms + ttl_ms_;
        return true;
    }

    bool contains(const std::string& replay_key) {
        if (replay_key.empty()) {
            return false;
        }

        const uint64_t now_ms = monotonicNowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        pruneExpiredLocked(now_ms);

        auto it = entries_.find(replay_key);
        return it != entries_.end() && it->second > now_ms;
    }

    std::size_t size() {
        const uint64_t now_ms = monotonicNowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        pruneExpiredLocked(now_ms);
        return entries_.size();
    }

    std::size_t pruneExpired() {
        const uint64_t now_ms = monotonicNowMs();
        std::lock_guard<std::mutex> lock(mutex_);
        return pruneExpiredLocked(now_ms);
    }

private:
    std::size_t pruneExpiredLocked(uint64_t now_ms) {
        std::size_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second <= now_ms) {
                it = entries_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    std::mutex mutex_;
    std::unordered_map<std::string, uint64_t> entries_;
    uint64_t ttl_ms_;
};

}  // namespace challenge
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
