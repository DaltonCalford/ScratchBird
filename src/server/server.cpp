#include "scratchbird/server.h"

#include "scratchbird/engine/executor.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace scratchbird
{

    Server::Server() = default;
    Server::~Server() = default;

    bool Server::start(const std::string&, std::uint16_t)
    {
        start_background_jobs();
        return true; // stub
    }

    void Server::stop()
    {
        stop_background_jobs();
    }

    void Server::start_background_jobs()
    {
        if (bg_running_)
            return;
        bg_running_ = true;
        // Enable auto analyze and spawn tick thread
        engine::set_auto_analyze_enabled(true);
        bg_thread_ = std::thread([this]() {
            while (bg_running_) {
                engine::stats_auto_analyze_tick();
                std::this_thread::sleep_for(std::chrono::seconds(30));
            }
        });
    }

    void Server::stop_background_jobs()
    {
        if (!bg_running_)
            return;
        bg_running_ = false;
        if (bg_thread_.joinable())
            bg_thread_.join();
        engine::set_auto_analyze_enabled(false);
    }

} // namespace scratchbird
