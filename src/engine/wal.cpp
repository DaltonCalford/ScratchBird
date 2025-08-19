#include "scratchbird/engine/wal.h"

namespace scratchbird::engine
{
    // Basic WAL Manager implementation stub for compilation
    WalManager::WalManager(const WalConfig& config) : config_(config) {}
    WalManager::~WalManager() = default;

    bool WalManager::initialize()
    {
        return true;
    }
    bool WalManager::open()
    {
        return true;
    }
    void WalManager::close() {}

    std::uint64_t WalManager::log_begin(std::uint64_t, std::uint32_t, std::uint64_t)
    {
        return 1;
    }
    std::uint64_t WalManager::log_commit(std::uint64_t, std::uint64_t)
    {
        return 1;
    }
    std::uint64_t WalManager::log_rollback(std::uint64_t, std::uint64_t)
    {
        return 1;
    }
    std::uint64_t WalManager::log_heap_insert(std::uint64_t, std::uint64_t, std::uint32_t,
                                              std::uint32_t, std::uint16_t, const void*,
                                              std::uint32_t)
    {
        return 1;
    }
    std::uint64_t WalManager::log_heap_update(std::uint64_t, std::uint64_t, std::uint32_t,
                                              std::uint32_t, std::uint16_t, const void*,
                                              std::uint32_t, const void*, std::uint32_t)
    {
        return 1;
    }
    std::uint64_t WalManager::log_heap_delete(std::uint64_t, std::uint64_t, std::uint32_t,
                                              std::uint32_t, std::uint16_t, const void*,
                                              std::uint32_t)
    {
        return 1;
    }
    std::uint64_t WalManager::log_page_write(std::uint64_t, std::uint64_t, std::uint32_t,
                                             std::uint32_t, std::uint32_t, const void*,
                                             std::uint32_t)
    {
        return 1;
    }
    std::uint64_t
    WalManager::log_checkpoint(const std::vector<std::pair<std::uint32_t, std::uint32_t>>&)
    {
        return 1;
    }

    bool WalManager::perform_checkpoint()
    {
        return true;
    }
    bool WalManager::recover_database(FileMap&)
    {
        return true;
    }
    std::uint64_t WalManager::get_last_checkpoint_lsn() const
    {
        return 0;
    }
    std::uint64_t WalManager::get_current_lsn() const
    {
        return 1;
    }
    bool WalManager::truncate_wal(std::uint64_t)
    {
        return true;
    }
    void WalManager::flush() {}
    void WalManager::fsync() {}

    WalManager::WalStats WalManager::get_stats() const
    {
        WalStats stats{};
        return stats;
    }
} // namespace scratchbird::engine
