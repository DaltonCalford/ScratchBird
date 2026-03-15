/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * LSM-Tree Components Implementation
 *
 * Implements:
 * - Memtable (in-memory write buffer)
 * - SSTableWriter (disk writer)
 * - SSTableReader (disk reader)
 *
 * MGA Compliance:
 * - All entries have xmin/xmax for transaction visibility
 * - Uses TransactionManager::isVersionVisible() for TIP-based checks
 * - No PostgreSQL snapshots
 */

#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/lsm_bloom_filter.h"
#include "scratchbird/core/lsm_compression.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/portable_file_io.h"
#include <fcntl.h>
#include <cstring>
#include <algorithm>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Memtable Implementation
// ============================================================================

Memtable::Memtable(size_t max_size_bytes)
    : max_size_(max_size_bytes), current_size_(0), sequence_(0)
{
}

Status Memtable::put(const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &value,
                     uint64_t xmin,
                     ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Create entry
    MemtableEntry entry;
    entry.key = key;
    entry.value = value;
    entry.sequence_number = ++sequence_;
    entry.entry_type = ENTRY_TYPE_INSERT;
    entry.xmin = xmin;
    entry.xmax = 0;

    // Calculate size
    size_t entry_size = key.size() + value.size() + sizeof(MemtableEntry);

    // Check if memtable will exceed limit
    if (current_size_ + entry_size > max_size_)
    {
        return Status::OOM;  // Signals memtable is full
    }

    // Insert into map (key -> list of versions)
    entries_[key].push_back(entry);
    current_size_ += entry_size;

    return Status::OK;
}

Status Memtable::remove(const std::vector<uint8_t> &key,
                        uint64_t xmax,
                        ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Create tombstone entry
    MemtableEntry entry;
    entry.key = key;
    // value left empty for tombstone
    entry.sequence_number = ++sequence_;
    entry.entry_type = ENTRY_TYPE_DELETE;
    entry.xmin = xmax;  // Tombstone created by this transaction
    entry.xmax = 0;

    // Calculate size
    size_t entry_size = key.size() + sizeof(MemtableEntry);

    // Check if memtable will exceed limit
    if (current_size_ + entry_size > max_size_)
    {
        return Status::OOM;  // Signals memtable is full
    }

    // Insert tombstone
    entries_[key].push_back(entry);
    current_size_ += entry_size;

    return Status::OK;
}

Status Memtable::get(const std::vector<uint8_t> &key,
                     uint64_t current_xid,
                     TransactionManager *txn_mgr,
                     std::vector<uint8_t> *value_out,
                     bool *found,
                     ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    *found = false;

    auto it = entries_.find(key);
    if (it == entries_.end())
    {
        return Status::OK;  // Key not found
    }

    // Search through versions (newest first - reverse iteration)
    const auto &versions = it->second;
    for (auto ver_it = versions.rbegin(); ver_it != versions.rend(); ++ver_it)
    {
        const MemtableEntry &entry = *ver_it;

        // MGA visibility check (Firebird TIP-based)
        bool visible = txn_mgr->isInventoryTransactionVisible(entry.xmin, current_xid);

        if (visible)
        {
            if (entry.entry_type == ENTRY_TYPE_INSERT)
            {
                *value_out = entry.value;
                *found = true;
                return Status::OK;
            }
            else  // ENTRY_TYPE_DELETE
            {
                // Tombstone - key is deleted
                *found = false;
                return Status::OK;
            }
        }
    }

    // No visible version found
    return Status::OK;
}

Status Memtable::scan(const std::vector<uint8_t> *start_key,
                      const std::vector<uint8_t> *end_key,
                      uint64_t current_xid,
                      TransactionManager *txn_mgr,
                      std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> *entries_out,
                      ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    entries_out->clear();

    for (const auto &kv : entries_)
    {
        const std::vector<uint8_t> &key = kv.first;
        const auto &versions = kv.second;

        // Check range bounds
        if (start_key && key < *start_key)
            continue;
        if (end_key && key >= *end_key)
            continue;

        // Find newest visible version (reverse iterate - newest first)
        for (auto ver_it = versions.rbegin(); ver_it != versions.rend(); ++ver_it)
        {
            const MemtableEntry &entry = *ver_it;

            // MGA visibility check
            bool visible = txn_mgr->isInventoryTransactionVisible(entry.xmin, current_xid);

            if (visible)
            {
                if (entry.entry_type == ENTRY_TYPE_INSERT)
                {
                    entries_out->push_back({key, entry.value});
                }
                // If tombstone, skip this key (it's deleted)
                break;  // Found newest visible version
            }
        }
    }

    return Status::OK;
}

Status Memtable::getAllEntries(std::vector<MemtableEntry> *entries_out,
                                ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    entries_out->clear();

    // Flatten all key-value versions into a single sorted list
    for (const auto &kv : entries_)
    {
        const auto &versions = kv.second;
        for (const auto &entry : versions)
        {
            entries_out->push_back(entry);
        }
    }

    // Sort by key, then by sequence number (descending - newest first)
    std::sort(entries_out->begin(), entries_out->end(),
              [](const MemtableEntry &a, const MemtableEntry &b) {
                  if (a.key != b.key)
                      return a.key < b.key;
                  return a.sequence_number > b.sequence_number;
              });

    return Status::OK;
}

Status Memtable::updateTIDsAfterMigration(const std::unordered_map<TID, TID> &tid_mapping,
                                          uint64_t *entries_updated_out,
                                          ErrorContext *ctx)
{
    (void)ctx;
    if (entries_updated_out != nullptr)
    {
        *entries_updated_out = 0;
    }

    if (tid_mapping.empty())
    {
        return Status::OK;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t updated = 0;
    for (auto &kv : entries_)
    {
        for (auto &entry : kv.second)
        {
            if (entry.entry_type != ENTRY_TYPE_INSERT)
            {
                continue;
            }

            if (entry.value.size() != (sizeof(uint64_t) + sizeof(uint16_t)))
            {
                continue;
            }

            uint64_t gpid = 0;
            for (size_t i = 0; i < sizeof(uint64_t); ++i)
            {
                gpid |= (static_cast<uint64_t>(entry.value[i]) << (i * 8));
            }

            uint16_t slot = 0;
            slot |= static_cast<uint16_t>(entry.value[sizeof(uint64_t)]);
            slot |= static_cast<uint16_t>(entry.value[sizeof(uint64_t) + 1]) << 8;

            TID old_tid{gpid, slot};

            auto it = tid_mapping.find(old_tid);
            if (it != tid_mapping.end())
            {
                TID new_tid = it->second;
                for (size_t i = 0; i < sizeof(uint64_t); ++i)
                {
                    entry.value[i] = static_cast<uint8_t>((new_tid.gpid >> (i * 8)) & 0xFF);
                }
                entry.value[sizeof(uint64_t)] = static_cast<uint8_t>(new_tid.slot & 0xFF);
                entry.value[sizeof(uint64_t) + 1] = static_cast<uint8_t>((new_tid.slot >> 8) & 0xFF);
                updated++;
            }
        }
    }

    if (entries_updated_out != nullptr)
    {
        *entries_updated_out = updated;
    }

    return Status::OK;
}

Status Memtable::removeEntriesIf(const std::function<bool(const MemtableEntry &)> &predicate,
                                 uint64_t *entries_removed_out,
                                 ErrorContext *ctx)
{
    if (entries_removed_out)
    {
        *entries_removed_out = 0;
    }

    if (!predicate)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "predicate is empty");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t removed = 0;
    for (auto it = entries_.begin(); it != entries_.end(); )
    {
        auto &versions = it->second;
        size_t before = versions.size();
        versions.erase(std::remove_if(versions.begin(), versions.end(), predicate), versions.end());
        removed += static_cast<uint64_t>(before - versions.size());

        if (versions.empty())
        {
            it = entries_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (entries_removed_out)
    {
        *entries_removed_out = removed;
    }

    return Status::OK;
}

// ============================================================================
// SSTableWriter Implementation
// ============================================================================

SSTableWriter::SSTableWriter(const std::string &file_path, size_t block_size, CompressionType compression)
    : file_path_(file_path),
      block_size_(block_size),
      fd_(-1),
      num_entries_(0),
      data_offset_(0),
      compression_type_(compression),
      compressor_(LsmCompressionFactory::create(compression))
{
}

SSTableWriter::~SSTableWriter()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
    }
}

Status SSTableWriter::open(ErrorContext *ctx)
{
    // Open file for writing (create if not exists, truncate if exists)
    fd_ = platform::openFd(file_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_ < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                         ("Failed to open SSTable file: " + file_path_).c_str());
        return Status::IO_ERROR;
    }

    data_offset_ = 0;
    num_entries_ = 0;
    min_key_.clear();
    max_key_.clear();
    index_.clear();
    current_block_.clear();

    // Initialize Bloom filter (estimate ~10K keys, 1% false positive rate)
    // This will be resized dynamically if needed
    bloom_filter_ = std::make_unique<LSMBloomFilter>(10000, 0.01);
    tid_bloom_filter_ = std::make_unique<LSMBloomFilter>(10000, 0.01);
    min_tid_ = INVALID_TID;
    max_tid_ = INVALID_TID;
    tid_entries_ = 0;

    return Status::OK;
}

Status SSTableWriter::addEntry(const std::vector<uint8_t> &key,
                                const std::vector<uint8_t> &value,
                                uint64_t sequence_number,
                                uint8_t entry_type,
                                uint64_t xmin,
                                uint64_t xmax,
                                ErrorContext *ctx)
{
    if (fd_ < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "SSTableWriter not open");
        return Status::INVALID_ARGUMENT;
    }

    // Add key to Bloom filter
    if (bloom_filter_)
    {
        bloom_filter_->add(key);
    }

    if (value.size() == (sizeof(uint64_t) + sizeof(uint16_t)))
    {
        uint64_t gpid = 0;
        for (size_t i = 0; i < sizeof(uint64_t); ++i)
        {
            gpid |= (static_cast<uint64_t>(value[i]) << (i * 8));
        }
        uint16_t slot = 0;
        slot |= static_cast<uint16_t>(value[sizeof(uint64_t)]);
        slot |= static_cast<uint16_t>(value[sizeof(uint64_t) + 1]) << 8;

        TID tid{gpid, slot};
        if (!min_tid_.isValid())
        {
            min_tid_ = tid;
            max_tid_ = tid;
        }
        else
        {
            if (tid < min_tid_)
            {
                min_tid_ = tid;
            }
            if (max_tid_ < tid)
            {
                max_tid_ = tid;
            }
        }

        if (tid_bloom_filter_)
        {
            OnDiskTID on_disk = toOnDiskTID(tid);
            std::vector<uint8_t> tid_bytes(sizeof(OnDiskTID));
            std::memcpy(tid_bytes.data(), &on_disk, sizeof(OnDiskTID));
            tid_bloom_filter_->add(tid_bytes);
        }

        tid_entries_++;
    }

    // Update min/max keys
    if (num_entries_ == 0)
    {
        min_key_ = key;
    }
    max_key_ = key;

    // Serialize entry: [key_len][key][value_len][value][seq][type][xmin][xmax]
    std::vector<uint8_t> serialized;

    // Key length + key
    uint32_t key_len = key.size();
    serialized.insert(serialized.end(), (uint8_t *)&key_len, (uint8_t *)&key_len + sizeof(key_len));
    serialized.insert(serialized.end(), key.begin(), key.end());

    // Value length + value
    uint32_t value_len = value.size();
    serialized.insert(serialized.end(), (uint8_t *)&value_len, (uint8_t *)&value_len + sizeof(value_len));
    serialized.insert(serialized.end(), value.begin(), value.end());

    // Metadata
    serialized.insert(serialized.end(), (uint8_t *)&sequence_number, (uint8_t *)&sequence_number + sizeof(sequence_number));
    serialized.push_back(entry_type);
    serialized.insert(serialized.end(), (uint8_t *)&xmin, (uint8_t *)&xmin + sizeof(xmin));
    serialized.insert(serialized.end(), (uint8_t *)&xmax, (uint8_t *)&xmax + sizeof(xmax));

    // Record index entry (key -> file offset)
    index_.push_back({key, data_offset_});

    // Write to file
    ssize_t written = ::write(fd_, serialized.data(), serialized.size());
    if (written < 0 || (size_t)written != serialized.size())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write entry to SSTable");
        return Status::IO_ERROR;
    }

    data_offset_ += serialized.size();
    num_entries_++;

    return Status::OK;
}

Status SSTableWriter::finish(ErrorContext *ctx)
{
    if (fd_ < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "SSTableWriter not open");
        return Status::INVALID_ARGUMENT;
    }

    // Write footer: [min_key][max_key][num_entries][index][bloom_filter][footer_magic]
    std::vector<uint8_t> footer;

    // Min key
    uint32_t min_key_len = min_key_.size();
    footer.insert(footer.end(), (uint8_t *)&min_key_len, (uint8_t *)&min_key_len + sizeof(min_key_len));
    footer.insert(footer.end(), min_key_.begin(), min_key_.end());

    // Max key
    uint32_t max_key_len = max_key_.size();
    footer.insert(footer.end(), (uint8_t *)&max_key_len, (uint8_t *)&max_key_len + sizeof(max_key_len));
    footer.insert(footer.end(), max_key_.begin(), max_key_.end());

    // Num entries
    footer.insert(footer.end(), (uint8_t *)&num_entries_, (uint8_t *)&num_entries_ + sizeof(num_entries_));

    // Index (simplified: store every 10th entry)
    uint64_t index_count = (index_.size() + 9) / 10;  // Round up
    footer.insert(footer.end(), (uint8_t *)&index_count, (uint8_t *)&index_count + sizeof(index_count));

    for (size_t i = 0; i < index_.size(); i += 10)
    {
        const auto &idx_entry = index_[i];
        uint32_t key_len = idx_entry.first.size();
        uint64_t offset = idx_entry.second;

        footer.insert(footer.end(), (uint8_t *)&key_len, (uint8_t *)&key_len + sizeof(key_len));
        footer.insert(footer.end(), idx_entry.first.begin(), idx_entry.first.end());
        footer.insert(footer.end(), (uint8_t *)&offset, (uint8_t *)&offset + sizeof(offset));
    }

    // Bloom filter
    if (bloom_filter_)
    {
        std::vector<uint8_t> bloom_data;
        bloom_filter_->serialize(&bloom_data);

        // Write Bloom filter size + data
        uint32_t bloom_size = bloom_data.size();
        footer.insert(footer.end(), (uint8_t *)&bloom_size, (uint8_t *)&bloom_size + sizeof(bloom_size));
        footer.insert(footer.end(), bloom_data.begin(), bloom_data.end());
    }
    else
    {
        // No Bloom filter - write size 0
        uint32_t bloom_size = 0;
        footer.insert(footer.end(), (uint8_t *)&bloom_size, (uint8_t *)&bloom_size + sizeof(bloom_size));
    }

    // Compression type (1 byte)
    uint8_t compression_byte = static_cast<uint8_t>(compression_type_);
    footer.push_back(compression_byte);

    // TID metadata (optional)
    uint8_t tid_meta_present = (tid_entries_ > 0 && tid_bloom_filter_) ? 1 : 0;
    footer.push_back(tid_meta_present);
    if (tid_meta_present)
    {
        OnDiskTID min_tid_disk = toOnDiskTID(min_tid_);
        OnDiskTID max_tid_disk = toOnDiskTID(max_tid_);
        footer.insert(footer.end(),
                      reinterpret_cast<uint8_t *>(&min_tid_disk),
                      reinterpret_cast<uint8_t *>(&min_tid_disk) + sizeof(OnDiskTID));
        footer.insert(footer.end(),
                      reinterpret_cast<uint8_t *>(&max_tid_disk),
                      reinterpret_cast<uint8_t *>(&max_tid_disk) + sizeof(OnDiskTID));
        footer.insert(footer.end(),
                      reinterpret_cast<uint8_t *>(&tid_entries_),
                      reinterpret_cast<uint8_t *>(&tid_entries_) + sizeof(tid_entries_));

        std::vector<uint8_t> tid_bloom_data;
        tid_bloom_filter_->serialize(&tid_bloom_data);
        uint32_t tid_bloom_size = static_cast<uint32_t>(tid_bloom_data.size());
        footer.insert(footer.end(),
                      reinterpret_cast<uint8_t *>(&tid_bloom_size),
                      reinterpret_cast<uint8_t *>(&tid_bloom_size) + sizeof(tid_bloom_size));
        footer.insert(footer.end(), tid_bloom_data.begin(), tid_bloom_data.end());
    }

    // Footer magic number (to verify footer integrity)
    const uint32_t FOOTER_MAGIC = 0x5353544C;  // "SSTL" in hex
    footer.insert(footer.end(), (uint8_t *)&FOOTER_MAGIC, (uint8_t *)&FOOTER_MAGIC + sizeof(FOOTER_MAGIC));

    // Write footer payload
    ssize_t written = ::write(fd_, footer.data(), footer.size());
    if (written < 0 || (size_t)written != footer.size())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write SSTable footer");
        return Status::IO_ERROR;
    }

    // Append footer size + magic trailer so readers can locate variable-sized footers.
    uint32_t footer_size = static_cast<uint32_t>(footer.size());
    uint8_t trailer[sizeof(footer_size) + sizeof(FOOTER_MAGIC)];
    std::memcpy(trailer, &footer_size, sizeof(footer_size));
    std::memcpy(trailer + sizeof(footer_size), &FOOTER_MAGIC, sizeof(FOOTER_MAGIC));

    written = ::write(fd_, trailer, sizeof(trailer));
    if (written < 0 || static_cast<size_t>(written) != sizeof(trailer))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write SSTable footer trailer");
        return Status::IO_ERROR;
    }

    // Flush to disk
    if (platform::syncFd(fd_) < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to fsync SSTable");
        return Status::IO_ERROR;
    }

    return Status::OK;
}

Status SSTableWriter::close(ErrorContext *ctx)
{
    if (fd_ >= 0)
    {
        if (::close(fd_) < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to close SSTable file");
            fd_ = -1;
            return Status::IO_ERROR;
        }
        fd_ = -1;
    }

    return Status::OK;
}

// ============================================================================
// SSTableReader Implementation
// ============================================================================

SSTableReader::SSTableReader(const std::string &file_path, size_t block_size)
    : file_path_(file_path),
      block_size_(block_size),
      fd_(-1),
      file_size_(0),
      data_end_offset_(0),
      num_entries_(0),
      compression_type_(CompressionType::NONE)
{
}

SSTableReader::~SSTableReader()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
    }
}

Status SSTableReader::open(ErrorContext *ctx)
{
    // Open file for reading
    fd_ = platform::openFd(file_path_.c_str(), O_RDONLY);
    if (fd_ < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND,
                         ("Failed to open SSTable file: " + file_path_).c_str());
        return Status::FILE_NOT_FOUND;
    }

    // Get file size
    off_t size = platform::seekFd(fd_, 0, SEEK_END);
    if (size < 0)
    {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to get SSTable file size");
        return Status::IO_ERROR;
    }
    file_size_ = size;
    has_tid_metadata_ = false;
    min_tid_ = INVALID_TID;
    max_tid_ = INVALID_TID;
    tid_count_ = 0;
    tid_bloom_filter_.reset();

    // Footer format: [min_key][max_key][num_entries][index][bloom_filter_size][bloom_data][magic]
    // Trailer (new): [footer_size(uint32)][magic(uint32)] to locate variable-sized footer.
    const uint32_t FOOTER_MAGIC = 0x5353544C;  // "SSTL"
    std::vector<uint8_t> footer;
    off_t footer_offset = 0;
    bool has_trailer = false;

    if (file_size_ >= static_cast<uint64_t>(sizeof(uint32_t) * 2))
    {
        uint8_t trailer[sizeof(uint32_t) * 2];
        if (platform::seekFd(fd_, static_cast<off_t>(file_size_ - sizeof(trailer)), SEEK_SET) ==
            static_cast<off_t>(file_size_ - sizeof(trailer)))
        {
            ssize_t nread = ::read(fd_, trailer, sizeof(trailer));
            if (nread == static_cast<ssize_t>(sizeof(trailer)))
            {
                uint32_t footer_size = 0;
                uint32_t magic = 0;
                std::memcpy(&footer_size, trailer, sizeof(footer_size));
                std::memcpy(&magic, trailer + sizeof(footer_size), sizeof(magic));
                if (magic == FOOTER_MAGIC &&
                    footer_size > 0 &&
                    footer_size + sizeof(trailer) <= file_size_)
                {
                    footer_offset = static_cast<off_t>(file_size_ - sizeof(trailer) - footer_size);
                    footer.resize(footer_size);
                    if (platform::seekFd(fd_, footer_offset, SEEK_SET) == footer_offset)
                    {
                        nread = ::read(fd_, footer.data(), footer.size());
                        if (nread == static_cast<ssize_t>(footer.size()))
                        {
                            has_trailer = true;
                            data_end_offset_ = static_cast<uint64_t>(footer_offset);
                        }
                    }
                }
            }
        }
    }

    if (!has_trailer)
    {
        // Fallback to legacy fixed-block footer.
        const size_t footer_size = block_size_;
        if (file_size_ < (int64_t)footer_size)
        {
            ::close(fd_);
            fd_ = -1;
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "SSTable file too small");
            return Status::PAGE_CORRUPT;
        }

        footer_offset = file_size_ - footer_size;
        if (platform::seekFd(fd_, footer_offset, SEEK_SET) != footer_offset)
        {
            ::close(fd_);
            fd_ = -1;
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek to SSTable footer");
            return Status::IO_ERROR;
        }

        footer.resize(footer_size);
        ssize_t nread = ::read(fd_, footer.data(), footer_size);
        if (nread < 0)
        {
            ::close(fd_);
            fd_ = -1;
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read SSTable footer");
            return Status::IO_ERROR;
        }
        data_end_offset_ = static_cast<uint64_t>(footer_offset);
    }

    // Parse footer
    size_t pos = 0;

    // Min key
    if (pos + sizeof(uint32_t) > footer.size()) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Footer too small");
        return Status::PAGE_CORRUPT;
    }
    uint32_t min_key_len;
    std::memcpy(&min_key_len, footer.data() + pos, sizeof(min_key_len));
    pos += sizeof(min_key_len);
    if (pos + min_key_len > footer.size()) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid min key length");
        return Status::PAGE_CORRUPT;
    }
    min_key_.assign(footer.begin() + pos, footer.begin() + pos + min_key_len);
    pos += min_key_len;

    // Max key
    if (pos + sizeof(uint32_t) > footer.size()) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Footer corrupt");
        return Status::PAGE_CORRUPT;
    }
    uint32_t max_key_len;
    std::memcpy(&max_key_len, footer.data() + pos, sizeof(max_key_len));
    pos += sizeof(max_key_len);
    if (pos + max_key_len > footer.size()) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid max key length");
        return Status::PAGE_CORRUPT;
    }
    max_key_.assign(footer.begin() + pos, footer.begin() + pos + max_key_len);
    pos += max_key_len;

    // Num entries
    if (pos + sizeof(uint64_t) > footer.size()) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Footer corrupt");
        return Status::PAGE_CORRUPT;
    }
    std::memcpy(&num_entries_, footer.data() + pos, sizeof(num_entries_));
    pos += sizeof(num_entries_);

    // Index (load all index entries)
    if (pos + sizeof(uint64_t) > footer.size()) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Footer corrupt");
        return Status::PAGE_CORRUPT;
    }
    uint64_t index_count;
    std::memcpy(&index_count, footer.data() + pos, sizeof(index_count));
    pos += sizeof(index_count);

    for (uint64_t i = 0; i < index_count && pos < footer.size(); i++)
    {
        if (pos + sizeof(uint32_t) > footer.size())
            break;

        uint32_t key_len;
        std::memcpy(&key_len, footer.data() + pos, sizeof(key_len));
        pos += sizeof(key_len);

        if (pos + key_len > footer.size())
            break;

        std::vector<uint8_t> key(footer.begin() + pos, footer.begin() + pos + key_len);
        pos += key_len;

        if (pos + sizeof(uint64_t) > footer.size())
            break;

        uint64_t offset;
        std::memcpy(&offset, footer.data() + pos, sizeof(offset));
        pos += sizeof(offset);

        index_[key] = offset;
    }

    // Bloom filter
    if (pos + sizeof(uint32_t) <= footer.size())
    {
        uint32_t bloom_size;
        std::memcpy(&bloom_size, footer.data() + pos, sizeof(bloom_size));
        pos += sizeof(bloom_size);

        if (bloom_size > 0 && pos + bloom_size <= footer.size())
        {
            // Deserialize Bloom filter
            std::vector<uint8_t> bloom_data(footer.begin() + pos, footer.begin() + pos + bloom_size);
            bloom_filter_.reset(LSMBloomFilter::deserialize(bloom_data));
            pos += bloom_size;
        }
    }

    // Compression type (1 byte, added November 22, 2025)
    if (pos + sizeof(uint8_t) <= footer.size())
    {
        uint8_t compression_byte;
        std::memcpy(&compression_byte, footer.data() + pos, sizeof(compression_byte));
        pos += sizeof(compression_byte);

        compression_type_ = static_cast<CompressionType>(compression_byte);
        compressor_ = LsmCompressionFactory::create(compression_type_);
    }
    else
    {
        // Older SSTable without compression info - assume no compression
        compression_type_ = CompressionType::NONE;
        compressor_ = LsmCompressionFactory::create(CompressionType::NONE);
    }

    size_t remaining = (pos <= footer.size()) ? (footer.size() - pos) : 0;
    if (remaining > sizeof(uint32_t))
    {
        uint8_t tid_meta_present = footer[pos];
        pos += sizeof(uint8_t);
        remaining = (pos <= footer.size()) ? (footer.size() - pos) : 0;

        if (tid_meta_present != 0)
        {
            if (remaining < (sizeof(OnDiskTID) * 2 + sizeof(uint64_t) + sizeof(uint32_t)))
            {
                ::close(fd_);
                fd_ = -1;
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "SSTable TID metadata incomplete");
                return Status::PAGE_CORRUPT;
            }

            OnDiskTID min_tid_disk{};
            OnDiskTID max_tid_disk{};
            std::memcpy(&min_tid_disk, footer.data() + pos, sizeof(OnDiskTID));
            pos += sizeof(OnDiskTID);
            std::memcpy(&max_tid_disk, footer.data() + pos, sizeof(OnDiskTID));
            pos += sizeof(OnDiskTID);

            std::memcpy(&tid_count_, footer.data() + pos, sizeof(tid_count_));
            pos += sizeof(tid_count_);

            uint32_t tid_bloom_size = 0;
            std::memcpy(&tid_bloom_size, footer.data() + pos, sizeof(tid_bloom_size));
            pos += sizeof(tid_bloom_size);

            if (tid_bloom_size > 0 && pos + tid_bloom_size <= footer.size())
            {
                std::vector<uint8_t> tid_bloom_data(footer.begin() + pos,
                                                    footer.begin() + pos + tid_bloom_size);
                tid_bloom_filter_.reset(LSMBloomFilter::deserialize(tid_bloom_data));
                pos += tid_bloom_size;
            }

            min_tid_ = fromOnDiskTID(min_tid_disk);
            max_tid_ = fromOnDiskTID(max_tid_disk);
            has_tid_metadata_ = true;
        }
    }

    // Footer magic (optional verification)
    if (pos + sizeof(uint32_t) <= footer.size())
    {
        uint32_t magic;
        std::memcpy(&magic, footer.data() + pos, sizeof(magic));
        if (magic != FOOTER_MAGIC)
        {
            // Warning: footer may be from older version without magic
            // Don't fail, just log
        }
    }

    return Status::OK;
}

Status SSTableReader::get(const std::vector<uint8_t> &key,
                          uint64_t current_xid,
                          TransactionManager *txn_mgr,
                          std::vector<uint8_t> *value_out,
                          bool *found,
                          ErrorContext *ctx)
{
    if (fd_ < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "SSTableReader not open");
        return Status::INVALID_ARGUMENT;
    }

    *found = false;

    // Quick range check
    if (key < min_key_ || key > max_key_)
    {
        return Status::OK;  // Key not in range
    }

    // Bloom filter check (OPTIMIZATION: saves 90%+ disk reads)
    if (bloom_filter_ && !bloom_filter_->mightContain(key))
    {
        // Bloom filter says key is DEFINITELY NOT present
        // No disk I/O needed!
        return Status::OK;
    }
    // If Bloom filter says "might contain", we must check disk
    // (could be false positive)

    // Find starting offset using index
    uint64_t search_offset = 0;
    auto it = index_.upper_bound(key);
    if (it != index_.begin())
    {
        --it;
        search_offset = it->second;
    }

    // Sequential scan from search_offset
    if (platform::seekFd(fd_, search_offset, SEEK_SET) != (off_t)search_offset)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek in SSTable");
        return Status::IO_ERROR;
    }

    // Read entries until we find the key or pass it
    while (true)
    {
        // Read entry header
        uint32_t key_len;
        ssize_t nread = ::read(fd_, &key_len, sizeof(key_len));
        if (nread <= 0)
            break;  // End of file or entries

        // Read key
        std::vector<uint8_t> entry_key(key_len);
        nread = ::read(fd_, entry_key.data(), key_len);
        if (nread < 0 || (size_t)nread != key_len)
            break;

        // Check if this is our key
        if (entry_key > key)
            break;  // Passed the key

        // Read value
        uint32_t value_len;
        nread = ::read(fd_, &value_len, sizeof(value_len));
        if (nread < 0)
            break;

        std::vector<uint8_t> entry_value(value_len);
        nread = ::read(fd_, entry_value.data(), value_len);
        if (nread < 0 || (size_t)nread != value_len)
            break;

        // Read metadata
        uint64_t sequence_number;
        uint8_t entry_type;
        uint64_t xmin, xmax;

        nread = ::read(fd_, &sequence_number, sizeof(sequence_number));
        if (nread < 0)
            break;
        nread = ::read(fd_, &entry_type, sizeof(entry_type));
        if (nread < 0)
            break;
        nread = ::read(fd_, &xmin, sizeof(xmin));
        if (nread < 0)
            break;
        nread = ::read(fd_, &xmax, sizeof(xmax));
        if (nread < 0)
            break;

        // Found matching key
        if (entry_key == key)
        {
            // Check MGA visibility
            bool visible = txn_mgr->isInventoryTransactionVisible(xmin, current_xid);

            if (visible)
            {
                if (entry_type == ENTRY_TYPE_INSERT)
                {
                    *value_out = entry_value;
                    *found = true;
                    return Status::OK;
                }
                else
                {
                    // Tombstone
                    *found = false;
                    return Status::OK;
                }
            }
            // Continue searching for older visible version
        }
    }

    return Status::OK;
}

Status SSTableReader::scan(const std::vector<uint8_t> &start_key,
                           const std::vector<uint8_t> &end_key,
                           uint64_t current_xid,
                           TransactionManager *txn_mgr,
                           std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> *entries_out,
                           ErrorContext *ctx)
{
    if (fd_ < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "SSTableReader not open");
        return Status::INVALID_ARGUMENT;
    }

    entries_out->clear();

    // Seek to beginning
    if (platform::seekFd(fd_, 0, SEEK_SET) != 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek in SSTable");
        return Status::IO_ERROR;
    }

    std::vector<uint8_t> last_key;

    // Sequential scan
    while (true)
    {
        // Read entry header
        uint32_t key_len;
        ssize_t nread = ::read(fd_, &key_len, sizeof(key_len));
        if (nread <= 0)
            break;  // End of file

        // Read key
        std::vector<uint8_t> entry_key(key_len);
        nread = ::read(fd_, entry_key.data(), key_len);
        if (nread < 0 || (size_t)nread != key_len)
            break;

        // Check range
        if (!start_key.empty() && entry_key < start_key)
        {
            // Skip this entry (read and discard value + metadata)
            uint32_t value_len;
            ::read(fd_, &value_len, sizeof(value_len));
            platform::seekFd(fd_, value_len + sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint64_t) * 2, SEEK_CUR);
            continue;
        }
        if (!end_key.empty() && entry_key >= end_key)
            break;

        // Read value
        uint32_t value_len;
        nread = ::read(fd_, &value_len, sizeof(value_len));
        if (nread < 0)
            break;

        std::vector<uint8_t> entry_value(value_len);
        nread = ::read(fd_, entry_value.data(), value_len);
        if (nread < 0 || (size_t)nread != value_len)
            break;

        // Read metadata
        uint64_t sequence_number;
        uint8_t entry_type;
        uint64_t xmin, xmax;

        ::read(fd_, &sequence_number, sizeof(sequence_number));
        ::read(fd_, &entry_type, sizeof(entry_type));
        ::read(fd_, &xmin, sizeof(xmin));
        ::read(fd_, &xmax, sizeof(xmax));

        // Check MGA visibility
        bool visible = txn_mgr->isInventoryTransactionVisible(xmin, current_xid);

        if (visible)
        {
            // Only add newest version of each key (skip duplicates)
            if (entry_key != last_key)
            {
                if (entry_type == ENTRY_TYPE_INSERT)
                {
                    entries_out->push_back({entry_key, entry_value});
                }
                // Skip tombstones
                last_key = entry_key;
            }
        }
    }

    return Status::OK;
}

Status SSTableReader::close(ErrorContext *ctx)
{
    if (fd_ >= 0)
    {
        if (::close(fd_) < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to close SSTable file");
            fd_ = -1;
            return Status::IO_ERROR;
        }
        fd_ = -1;
    }

    return Status::OK;
}

// ============================================================================
// SSTableReader::Iterator Implementation
// ============================================================================

class SSTableReaderIterator : public SSTableReader::Iterator
{
public:
    SSTableReaderIterator(int fd, uint64_t file_size, uint64_t data_end_offset, size_t block_size)
        : fd_(fd),
          file_size_(file_size),
          block_size_(block_size),
          data_end_offset_(data_end_offset),
          current_offset_(0),
          valid_(false)
    {
        // Start reading from beginning of file
        next();
    }

    bool isValid() const override
    {
        return valid_;
    }

    void next() override
    {
        // Read next entry from SSTable
        // Entry format: [key_len][key][value_len][value][seq][type][xmin][xmax]

        if (current_offset_ >= data_end_offset_)
        {
            valid_ = false;
            return;
        }

        // Seek to current offset
        if (platform::seekFd(fd_, current_offset_, SEEK_SET) != (off_t)current_offset_)
        {
            valid_ = false;
            return;
        }

        // Read key length
        uint32_t key_len;
        if (::read(fd_, &key_len, sizeof(key_len)) != sizeof(key_len))
        {
            valid_ = false;
            return;
        }

        // Check if we've reached the footer (entries end before footer)
        // A key_len of 0 or impossibly large indicates end of data
        if (key_len == 0 || key_len > file_size_)
        {
            valid_ = false;
            return;
        }

        // Read key
        current_key_.resize(key_len);
        if (::read(fd_, current_key_.data(), key_len) != (ssize_t)key_len)
        {
            valid_ = false;
            return;
        }

        // Read value length
        uint32_t value_len;
        if (::read(fd_, &value_len, sizeof(value_len)) != sizeof(value_len))
        {
            valid_ = false;
            return;
        }

        // Read value
        current_value_.resize(value_len);
        if (value_len > 0 && ::read(fd_, current_value_.data(), value_len) != (ssize_t)value_len)
        {
            valid_ = false;
            return;
        }

        // Read metadata
        if (::read(fd_, &current_sequence_, sizeof(current_sequence_)) != sizeof(current_sequence_))
        {
            valid_ = false;
            return;
        }

        if (::read(fd_, &current_type_, sizeof(current_type_)) != sizeof(current_type_))
        {
            valid_ = false;
            return;
        }

        if (::read(fd_, &current_xmin_, sizeof(current_xmin_)) != sizeof(current_xmin_))
        {
            valid_ = false;
            return;
        }

        if (::read(fd_, &current_xmax_, sizeof(current_xmax_)) != sizeof(current_xmax_))
        {
            valid_ = false;
            return;
        }

        // Update offset to next entry
        current_offset_ += sizeof(key_len) + key_len + sizeof(value_len) + value_len +
                            sizeof(current_sequence_) + sizeof(current_type_) +
                            sizeof(current_xmin_) + sizeof(current_xmax_);

        valid_ = true;
    }

    const std::vector<uint8_t>& key() const override
    {
        return current_key_;
    }

    const std::vector<uint8_t>& value() const override
    {
        return current_value_;
    }

    uint64_t sequenceNumber() const override
    {
        return current_sequence_;
    }

    uint8_t entryType() const override
    {
        return current_type_;
    }

    uint64_t xmin() const override
    {
        return current_xmin_;
    }

    uint64_t xmax() const override
    {
        return current_xmax_;
    }

private:
    int fd_;
    uint64_t file_size_;
    size_t block_size_;
    uint64_t data_end_offset_;
    uint64_t current_offset_;
    bool valid_;

    std::vector<uint8_t> current_key_;
    std::vector<uint8_t> current_value_;
    uint64_t current_sequence_;
    uint8_t current_type_;
    uint64_t current_xmin_;
    uint64_t current_xmax_;
};

std::unique_ptr<SSTableReader::Iterator> SSTableReader::createIterator()
{
    if (fd_ < 0)
    {
        return nullptr;
    }

    return std::make_unique<SSTableReaderIterator>(fd_, file_size_, data_end_offset_, block_size_);
}

} // namespace core
} // namespace scratchbird
