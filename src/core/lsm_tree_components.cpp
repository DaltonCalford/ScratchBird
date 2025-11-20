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
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logger.h"
#include <fcntl.h>
#include <unistd.h>
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
        bool visible = txn_mgr->isVersionVisible(entry.xmin, current_xid);

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
            bool visible = txn_mgr->isVersionVisible(entry.xmin, current_xid);

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

// ============================================================================
// SSTableWriter Implementation
// ============================================================================

SSTableWriter::SSTableWriter(const std::string &file_path, size_t block_size)
    : file_path_(file_path), block_size_(block_size), fd_(-1), num_entries_(0), data_offset_(0)
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
    fd_ = ::open(file_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

    // Write footer: [min_key][max_key][num_entries][index]
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

    // Write footer
    ssize_t written = ::write(fd_, footer.data(), footer.size());
    if (written < 0 || (size_t)written != footer.size())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write SSTable footer");
        return Status::IO_ERROR;
    }

    // Flush to disk
    if (::fsync(fd_) < 0)
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

SSTableReader::SSTableReader(const std::string &file_path)
    : file_path_(file_path), fd_(-1), file_size_(0), num_entries_(0)
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
    fd_ = ::open(file_path_.c_str(), O_RDONLY);
    if (fd_ < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND,
                         ("Failed to open SSTable file: " + file_path_).c_str());
        return Status::FILE_NOT_FOUND;
    }

    // Get file size
    off_t size = ::lseek(fd_, 0, SEEK_END);
    if (size < 0)
    {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to get SSTable file size");
        return Status::IO_ERROR;
    }
    file_size_ = size;

    // Read footer (simplified: read last 1KB)
    const size_t footer_size = 1024;
    if (file_size_ < footer_size)
    {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "SSTable file too small");
        return Status::PAGE_CORRUPT;
    }

    off_t footer_offset = file_size_ - footer_size;
    if (::lseek(fd_, footer_offset, SEEK_SET) != footer_offset)
    {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek to SSTable footer");
        return Status::IO_ERROR;
    }

    std::vector<uint8_t> footer(footer_size);
    ssize_t nread = ::read(fd_, footer.data(), footer_size);
    if (nread < 0)
    {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read SSTable footer");
        return Status::IO_ERROR;
    }

    // Parse footer (min_key, max_key, num_entries, index)
    size_t pos = 0;

    // Min key
    uint32_t min_key_len;
    std::memcpy(&min_key_len, footer.data() + pos, sizeof(min_key_len));
    pos += sizeof(min_key_len);
    min_key_.assign(footer.begin() + pos, footer.begin() + pos + min_key_len);
    pos += min_key_len;

    // Max key
    uint32_t max_key_len;
    std::memcpy(&max_key_len, footer.data() + pos, sizeof(max_key_len));
    pos += sizeof(max_key_len);
    max_key_.assign(footer.begin() + pos, footer.begin() + pos + max_key_len);
    pos += max_key_len;

    // Num entries
    std::memcpy(&num_entries_, footer.data() + pos, sizeof(num_entries_));
    pos += sizeof(num_entries_);

    // Index (simplified: load all index entries)
    uint64_t index_count;
    std::memcpy(&index_count, footer.data() + pos, sizeof(index_count));
    pos += sizeof(index_count);

    for (uint64_t i = 0; i < index_count && pos < footer.size(); i++)
    {
        uint32_t key_len;
        std::memcpy(&key_len, footer.data() + pos, sizeof(key_len));
        pos += sizeof(key_len);

        if (pos + key_len > footer.size())
            break;

        std::vector<uint8_t> key(footer.begin() + pos, footer.begin() + pos + key_len);
        pos += key_len;

        uint64_t offset;
        std::memcpy(&offset, footer.data() + pos, sizeof(offset));
        pos += sizeof(offset);

        index_[key] = offset;
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

    // Find starting offset using index
    uint64_t search_offset = 0;
    auto it = index_.upper_bound(key);
    if (it != index_.begin())
    {
        --it;
        search_offset = it->second;
    }

    // Sequential scan from search_offset
    if (::lseek(fd_, search_offset, SEEK_SET) != (off_t)search_offset)
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
            bool visible = txn_mgr->isVersionVisible(xmin, current_xid);

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
    if (::lseek(fd_, 0, SEEK_SET) != 0)
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
            ::lseek(fd_, value_len + sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint64_t) * 2, SEEK_CUR);
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
        bool visible = txn_mgr->isVersionVisible(xmin, current_xid);

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

} // namespace core
} // namespace scratchbird
