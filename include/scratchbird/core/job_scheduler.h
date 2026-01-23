#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include "scratchbird/core/status.h"

namespace scratchbird::core {

class Database;
class ErrorContext;

class JobScheduler {
public:
    struct Config {
        uint32_t polling_interval_seconds = 10;
        uint32_t max_jobs_per_tick = 16;
        uint32_t cron_fallback_seconds = 60;
    };

    explicit JobScheduler(Database* db, const Config& config = Config{});
    ~JobScheduler();

    JobScheduler(const JobScheduler&) = delete;
    JobScheduler& operator=(const JobScheduler&) = delete;

    Status start(ErrorContext* ctx = nullptr);
    void stop();
    bool isRunning() const { return running_.load(); }

private:
    void runLoop();
    void processDueJobs();

    Database* db_;
    Config config_;

    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_requested_ = false;
    std::thread worker_;
};

}  // namespace scratchbird::core
