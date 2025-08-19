#include "scratchbird/engine/wal.h"

#include <cstring>
#include <functional>
#include <stdexcept>

namespace scratchbird::engine
{
    std::mutex WalManager::g_listeners_mu_{};
    std::vector<WalManager::WalListener> WalManager::g_listeners_{};

    static void write_u64(FILE* fp, std::uint64_t v)
    {
        if (fwrite(&v, sizeof v, 1, fp) != 1)
            throw std::runtime_error("wal write u64");
    }
    static void write_u8(FILE* fp, std::uint8_t v)
    {
        if (fwrite(&v, sizeof v, 1, fp) != 1)
            throw std::runtime_error("wal write u8");
    }
    static void write_blob(FILE* fp, const void* data, std::size_t len)
    {
        std::uint64_t l = static_cast<std::uint64_t>(len);
        write_u64(fp, l);
        if (l && fwrite(data, 1, l, fp) != l)
            throw std::runtime_error("wal write blob");
    }

    static std::uint64_t read_u64(FILE* fp)
    {
        std::uint64_t v = 0;
        if (fread(&v, sizeof v, 1, fp) != 1)
            throw std::runtime_error("wal read u64");
        return v;
    }
    static std::uint8_t read_u8(FILE* fp)
    {
        std::uint8_t v = 0;
        if (fread(&v, sizeof v, 1, fp) != 1)
            throw std::runtime_error("wal read u8");
        return v;
    }
    static std::vector<std::uint8_t> read_blob(FILE* fp)
    {
        auto l = read_u64(fp);
        std::vector<std::uint8_t> out;
        out.resize(static_cast<std::size_t>(l));
        if (l && fread(out.data(), 1, l, fp) != l)
            throw std::runtime_error("wal read blob");
        return out;
    }

    WalManager::WalManager(const std::string& path) : path_(path)
    {
        fp_ = std::fopen(path_.c_str(), "wb+");
        if (!fp_)
            throw std::runtime_error("wal open");
        // Crash injection: read env
        if (const char* e = std::getenv("SB_WAL_CRASH_AFTER"))
            crash_after_ = std::strtoull(e, nullptr, 10);
    }

    WalManager::~WalManager()
    {
        if (fp_)
            std::fclose(fp_);
    }

    void WalManager::maybe_inject_crash()
    {
        if (crash_after_ && ++append_count_ == crash_after_) {
            std::fflush(fp_);
            std::fflush(stdout);
            std::_Exit(137);
        }
    }

    void WalManager::append(const WalRecord& rec)
    {
        std::lock_guard<std::mutex> g(mu_);
        write_u64(fp_, rec.lsn);
        write_u8(fp_, static_cast<std::uint8_t>(rec.kind));
        write_blob(fp_, rec.key_bytes.data(), rec.key_bytes.size());
        write_u64(fp_, rec.row_id);
        write_blob(fp_, rec.payload.data(), rec.payload.size());
        if ((append_count_ + 1) % group_size_ == 0)
            std::fflush(fp_);
        maybe_inject_crash();
        notify_global(rec);
    }

    std::uint64_t WalManager::append_insert(const std::vector<std::uint8_t>& key_enc,
                                            std::uint64_t row_id, const std::string& payload)
    {
        WalRecord rec;
        rec.kind = WalRecKind::Insert;
        rec.key_bytes = key_enc;
        rec.row_id = row_id;
        rec.payload = payload;
        rec.lsn = next_lsn_++;
        append(rec);
        return rec.lsn;
    }

    std::uint64_t WalManager::append_delete(const std::vector<std::uint8_t>& key_enc)
    {
        WalRecord rec;
        rec.kind = WalRecKind::Delete;
        rec.key_bytes = key_enc;
        rec.row_id = 0;
        rec.lsn = next_lsn_++;
        append(rec);
        return rec.lsn;
    }

    std::uint64_t WalManager::append_root_update(std::uint32_t new_root_page)
    {
        WalRecord rec;
        rec.kind = WalRecKind::RootUpdate;
        rec.row_id = new_root_page;
        rec.lsn = next_lsn_++;
        append(rec);
        return rec.lsn;
    }

    void WalManager::flush()
    {
        std::lock_guard<std::mutex> g(mu_);
        std::fflush(fp_);
    }

    std::vector<WalRecord> WalManager::read_all() const
    {
        std::vector<WalRecord> out;
        FILE* r = std::fopen(path_.c_str(), "rb");
        if (!r)
            return out;
        try {
            while (true) {
                WalRecord rec{};
                rec.lsn = read_u64(r);
                rec.kind = static_cast<WalRecKind>(read_u8(r));
                rec.key_bytes = read_blob(r);
                rec.row_id = read_u64(r);
                auto pl = read_blob(r);
                rec.payload.assign(reinterpret_cast<const char*>(pl.data()), pl.size());
                out.push_back(std::move(rec));
            }
        } catch (...) {
            // EOF or error; best-effort read
        }
        std::fclose(r);
        return out;
    }

    void WalManager::register_global_listener(const WalListener& fn)
    {
        std::lock_guard<std::mutex> lg(g_listeners_mu_);
        g_listeners_.push_back(fn);
    }

    void WalManager::clear_global_listeners()
    {
        std::lock_guard<std::mutex> lg(g_listeners_mu_);
        g_listeners_.clear();
    }

    void WalManager::notify_global(const WalRecord& rec)
    {
        std::lock_guard<std::mutex> lg(g_listeners_mu_);
        for (auto& fn : g_listeners_)
            fn(rec);
    }

} // namespace scratchbird::engine
