#ifndef SCRATCHBIRD_ENGINE_WAL_H
#define SCRATCHBIRD_ENGINE_WAL_H

#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    enum class WalRecKind : std::uint8_t { Insert = 1, Delete = 2, RootUpdate = 3 };

    struct WalRecord {
        WalRecKind kind{WalRecKind::Insert};
        std::vector<std::uint8_t> key_bytes; // composite key encoding
        std::uint64_t row_id{0};
        std::string payload; // for inserts
        std::uint64_t lsn{0};
    };

    class WalManager
    {
      public:
        explicit WalManager(const std::string& path);
        ~WalManager();
        WalManager(const WalManager&) = delete;
        WalManager& operator=(const WalManager&) = delete;

        // Append logical operations; returns LSN
        std::uint64_t append_insert(const std::vector<std::uint8_t>& key_enc, std::uint64_t row_id,
                                    const std::string& payload);
        std::uint64_t append_delete(const std::vector<std::uint8_t>& key_enc);
        std::uint64_t append_root_update(std::uint32_t new_root_page);

        void flush();

        // Reader API: load all records in order
        std::vector<WalRecord> read_all() const;

        std::uint64_t next_lsn() const
        {
            return next_lsn_;
        }

        // Group commit policy: if group_size==1 => flush each append (always). If >1, flush after
        // group_size appends.
        void set_group_commit(std::uint32_t group_size)
        {
            group_size_ = group_size ? group_size : 1;
        }

        // Global listener API: allows online index builds to tap WAL regardless of instance
        using WalListener = std::function<void(const WalRecord&)>;
        static void register_global_listener(const WalListener& fn);
        static void clear_global_listeners();

      private:
        void append(const WalRecord& rec);
        void maybe_inject_crash();
        static void notify_global(const WalRecord& rec);

        std::string path_;
        FILE* fp_{nullptr};
        std::mutex mu_{};
        std::uint64_t next_lsn_{1};
        std::uint64_t append_count_{0};
        std::uint64_t crash_after_{0};
        std::uint32_t group_size_{1};

        // Global listeners shared across instances
        static std::mutex g_listeners_mu_;
        static std::vector<WalListener> g_listeners_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_WAL_H
