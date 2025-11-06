/**
 * LSM-Tree (Log-Structured Merge-Tree) Implementation
 *
 * See /MGA_RULES.md for Firebird MGA compliance requirements
 */

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include <algorithm>
#include <cmath>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Memtable Implementation
// ============================================================================

Memtable::Memtable(size_t max_size_bytes)
    : next_sequence_(0),
      current_size_(0),
      max_size_(max_size_bytes),
      is_full_(false)
{
}

Memtable::~Memtable()
{
}

Status Memtable::put(const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &value,
                     uint64_t current_xid,
                     ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Create entry
    MemtableEntry entry;
    entry.key = key;
    entry.value = value;
    entry.sequence_number = next_sequence_++;
    entry.entry_type = ENTRY_TYPE_INSERT;
    entry.xmin = current_xid;  // MGA: Transaction that created this entry
    entry.xmax = 0;            // MGA: Not deleted

    // Calculate size
    size_t entry_size = entry.getSize();

    // Check if adding this entry would exceed max size
    if (current_size_ + entry_size > max_size_)
    {
        is_full_ = true;  // Mark as full
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Memtable full");
        return Status::OOM;
    }

    // Insert into Red-Black Tree
    entries_[entry] = true;

    // Update size
    current_size_ += entry_size;

    return Status::OK;
}

Status Memtable::remove(const std::vector<uint8_t> &key,
                        uint64_t current_xid,
                        ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Create tombstone entry
    MemtableEntry entry;
    entry.key = key;
    entry.value = {};  // Empty value for tombstone
    entry.sequence_number = next_sequence_++;
    entry.entry_type = ENTRY_TYPE_DELETE;  // Tombstone
    entry.xmin = current_xid;              // MGA: Transaction that created tombstone
    entry.xmax = 0;                        // MGA: Not deleted

    // Calculate size
    size_t entry_size = entry.getSize();

    // Check if adding this entry would exceed max size
    if (current_size_ + entry_size > max_size_)
    {
        is_full_ = true;  // Mark as full
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Memtable full");
        return Status::OOM;
    }

    // Insert tombstone into Red-Black Tree
    entries_[entry] = true;

    // Update size
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

    // Create search key with max sequence (to start iteration from newest)
    MemtableEntry search_key;
    search_key.key = key;
    search_key.sequence_number = UINT64_MAX;  // Start from newest

    // Find first entry with this key (newest version due to operator<)
    auto it = entries_.lower_bound(search_key);

    // Iterate through all versions of this key (newest to oldest)
    while (it != entries_.end() && it->first.key == key)
    {
        const MemtableEntry &entry = it->first;

        // Check MGA visibility (Firebird rules)
        if (isEntryVisible(entry, current_xid, txn_mgr))
        {
            if (entry.entry_type == ENTRY_TYPE_INSERT)
            {
                // Found visible insert - return value
                *value_out = entry.value;
                *found = true;
                return Status::OK;
            }
            else if (entry.entry_type == ENTRY_TYPE_DELETE)
            {
                // Found visible tombstone - key is deleted
                *found = false;
                return Status::OK;
            }
        }

        ++it;
    }

    // Key not found or no visible version
    *found = false;
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

    // Determine iteration range
    auto it_start = entries_.begin();
    auto it_end = entries_.end();

    if (start_key != nullptr)
    {
        MemtableEntry search_key;
        search_key.key = *start_key;
        search_key.sequence_number = UINT64_MAX;  // Start from newest
        it_start = entries_.lower_bound(search_key);
    }

    // Track last key to skip duplicate versions
    std::vector<uint8_t> last_key;
    bool last_key_set = false;

    // Iterate through range
    for (auto it = it_start; it != it_end; ++it)
    {
        const MemtableEntry &entry = it->first;

        // Check end_key boundary
        if (end_key != nullptr && entry.key > *end_key)
        {
            break;
        }

        // Skip duplicate keys (we already processed a newer version)
        if (last_key_set && entry.key == last_key)
        {
            continue;
        }

        // Check MGA visibility
        if (isEntryVisible(entry, current_xid, txn_mgr))
        {
            if (entry.entry_type == ENTRY_TYPE_INSERT)
            {
                // Add to results
                entries_out->push_back({entry.key, entry.value});
            }
            // For DELETE entries, just skip (key is deleted)

            // Mark key as processed
            last_key = entry.key;
            last_key_set = true;
        }
    }

    return Status::OK;
}

bool Memtable::isFull() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return is_full_ || current_size_ >= max_size_;
}

size_t Memtable::getSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_;
}

Status Memtable::getAllEntries(std::vector<MemtableEntry> *entries_out,
                               ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    entries_out->clear();
    entries_out->reserve(entries_.size());

    // Copy all entries (already in sorted order from std::map)
    for (const auto &pair : entries_)
    {
        entries_out->push_back(pair.first);
    }

    return Status::OK;
}

size_t Memtable::getNumEntries() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

bool Memtable::isEntryVisible(const MemtableEntry &entry,
                              uint64_t current_xid,
                              TransactionManager *txn_mgr) const
{
    // FIREBIRD MGA VISIBILITY RULES
    // See /MGA_RULES.md for complete specification

    // Rule 1: Own changes always visible
    if (entry.xmin == current_xid)
    {
        return entry.xmax == 0 || entry.xmax != current_xid;
    }

    // Rule 2: Check if creator transaction is committed
    // Use TIP-based visibility (NO PostgreSQL Snapshot!)
    TransactionState xmin_state;
    Status status = txn_mgr->getTransactionState(entry.xmin, xmin_state, nullptr);
    if (status != Status::OK || xmin_state != TransactionState::COMMITTED)
    {
        // Not committed = not visible
        return false;
    }

    // Rule 3: Creator must be older than current transaction
    if (entry.xmin >= current_xid)
    {
        // Future transaction = not visible
        return false;
    }

    // Rule 4: Check if deleted
    if (entry.xmax != 0)
    {
        // Entry was deleted - check if deletion is visible
        if (entry.xmax == current_xid)
        {
            // Deleted by us = not visible
            return false;
        }

        TransactionState xmax_state;
        status = txn_mgr->getTransactionState(entry.xmax, xmax_state, nullptr);
        if (status == Status::OK && xmax_state == TransactionState::COMMITTED && entry.xmax < current_xid)
        {
            // Deletion committed and older = not visible
            return false;
        }
    }

    // Entry is visible
    return true;
}

// ============================================================================
// LSM Bloom Filter Implementation (Phase 6 stub - basic functionality)
// ============================================================================

LSMBloomFilter::LSMBloomFilter(size_t expected_keys, double false_positive_rate)
{
    // Ensure reasonable inputs
    if (expected_keys == 0)
        expected_keys = 1;
    if (false_positive_rate <= 0.0 || false_positive_rate >= 1.0)
        false_positive_rate = 0.01;

    // Calculate optimal bit array size
    // Formula: m = -(n * ln(p)) / (ln(2)^2)
    // where n = expected_keys, p = false_positive_rate
    double m = -(static_cast<double>(expected_keys) * std::log(false_positive_rate)) /
               (std::log(2.0) * std::log(2.0));

    // Clamp to reasonable size (max 1MB bit array = 8MB)
    if (m > 8 * 1024 * 1024)
        m = 8 * 1024 * 1024;
    if (m < 8)
        m = 8;

    num_bits_ = static_cast<size_t>(m);

    // Calculate optimal number of hash functions
    // Formula: k = (m / n) * ln(2)
    num_hashes_ = static_cast<size_t>(
        (static_cast<double>(num_bits_) / static_cast<double>(expected_keys)) * std::log(2.0));

    // Clamp hash functions to reasonable range
    if (num_hashes_ < 1)
        num_hashes_ = 1;
    if (num_hashes_ > 10)
        num_hashes_ = 10;

    // Allocate bit array (rounded up to bytes)
    bits_.resize((num_bits_ + 7) / 8, 0);
}

LSMBloomFilter::~LSMBloomFilter()
{
}

void LSMBloomFilter::add(const std::vector<uint8_t> &key)
{
    // Use multiple hash functions (double hashing with MurmurHash3-style)
    uint64_t hash1 = 0;
    uint64_t hash2 = 1;

    // Simple hash function (replace with MurmurHash3 in Phase 6)
    for (uint8_t byte : key)
    {
        hash1 = hash1 * 31 + byte;
        hash2 = hash2 * 37 + byte;
    }

    // Set bits using double hashing
    for (size_t i = 0; i < num_hashes_; ++i)
    {
        uint64_t combined = (hash1 + i * hash2) % num_bits_;
        size_t byte_idx = combined / 8;
        size_t bit_idx = combined % 8;
        bits_[byte_idx] |= (1 << bit_idx);
    }
}

bool LSMBloomFilter::mightContain(const std::vector<uint8_t> &key) const
{
    // Check if all bits are set
    uint64_t hash1 = 0;
    uint64_t hash2 = 1;

    for (uint8_t byte : key)
    {
        hash1 = hash1 * 31 + byte;
        hash2 = hash2 * 37 + byte;
    }

    for (size_t i = 0; i < num_hashes_; ++i)
    {
        uint64_t combined = (hash1 + i * hash2) % num_bits_;
        size_t byte_idx = combined / 8;
        size_t bit_idx = combined % 8;

        if (!(bits_[byte_idx] & (1 << bit_idx)))
        {
            return false; // Definitely not present
        }
    }

    return true; // Might be present (or false positive)
}

void LSMBloomFilter::serialize(std::vector<uint8_t> *output) const
{
    output->clear();

    // Write num_bits (8 bytes)
    const uint8_t *num_bits_ptr = reinterpret_cast<const uint8_t *>(&num_bits_);
    output->insert(output->end(), num_bits_ptr, num_bits_ptr + sizeof(num_bits_));

    // Write num_hashes (8 bytes)
    const uint8_t *num_hashes_ptr = reinterpret_cast<const uint8_t *>(&num_hashes_);
    output->insert(output->end(), num_hashes_ptr, num_hashes_ptr + sizeof(num_hashes_));

    // Write bit array
    output->insert(output->end(), bits_.begin(), bits_.end());
}

LSMBloomFilter *LSMBloomFilter::deserialize(const std::vector<uint8_t> &data)
{
    if (data.size() < 16)
    {
        return nullptr; // Invalid data
    }

    // Read num_bits
    size_t num_bits = *reinterpret_cast<const size_t *>(data.data());

    // Read num_hashes
    size_t num_hashes = *reinterpret_cast<const size_t *>(data.data() + sizeof(size_t));

    // Create bloom filter
    LSMBloomFilter *bf = new LSMBloomFilter(1, 0.01); // Dummy params
    bf->num_bits_ = num_bits;
    bf->num_hashes_ = num_hashes;

    // Read bit array
    size_t expected_bytes = (num_bits + 7) / 8;
    if (data.size() < 16 + expected_bytes)
    {
        delete bf;
        return nullptr; // Invalid data
    }

    bf->bits_.assign(data.begin() + 16, data.begin() + 16 + expected_bytes);

    return bf;
}

// ============================================================================
// SSTable Writer Implementation
// ============================================================================

SSTableWriter::SSTableWriter(const std::string &file_path, size_t block_size)
    : file_path_(file_path),
      block_size_(block_size),
      current_block_offset_(0),
      num_entries_(0),
      bloom_filter_(1000, 0.01) // Default: 1000 keys, 1% false positive
{
}

SSTableWriter::~SSTableWriter()
{
    if (file_.is_open())
    {
        file_.close();
    }
}

Status SSTableWriter::open(ErrorContext *ctx)
{
    // Open file in binary write mode
    file_.open(file_path_, std::ios::binary | std::ios::out | std::ios::trunc);

    if (!file_.is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Failed to open SSTable file for writing");
        return Status::FILE_NOT_FOUND;
    }

    current_block_offset_ = 0;
    num_entries_ = 0;
    current_block_.clear();
    index_entries_.clear();
    min_key_.clear();
    max_key_.clear();

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
    // Update min/max keys
    if (num_entries_ == 0)
    {
        min_key_ = key;
    }
    max_key_ = key;

    // Add to Bloom filter
    bloom_filter_.add(key);

    // Serialize entry: [key_len][key][value_len][value][seq][type][xmin][xmax]
    std::vector<uint8_t> serialized;

    // Key length (2 bytes)
    uint16_t key_len = static_cast<uint16_t>(key.size());
    const uint8_t *key_len_ptr = reinterpret_cast<const uint8_t *>(&key_len);
    serialized.insert(serialized.end(), key_len_ptr, key_len_ptr + sizeof(key_len));

    // Key data
    serialized.insert(serialized.end(), key.begin(), key.end());

    // Value length (4 bytes)
    uint32_t value_len = static_cast<uint32_t>(value.size());
    const uint8_t *value_len_ptr = reinterpret_cast<const uint8_t *>(&value_len);
    serialized.insert(serialized.end(), value_len_ptr, value_len_ptr + sizeof(value_len));

    // Value data
    serialized.insert(serialized.end(), value.begin(), value.end());

    // Sequence number (8 bytes)
    const uint8_t *seq_ptr = reinterpret_cast<const uint8_t *>(&sequence_number);
    serialized.insert(serialized.end(), seq_ptr, seq_ptr + sizeof(sequence_number));

    // Entry type (1 byte)
    serialized.push_back(entry_type);

    // MGA fields: xmin (8 bytes)
    const uint8_t *xmin_ptr = reinterpret_cast<const uint8_t *>(&xmin);
    serialized.insert(serialized.end(), xmin_ptr, xmin_ptr + sizeof(xmin));

    // MGA fields: xmax (8 bytes)
    const uint8_t *xmax_ptr = reinterpret_cast<const uint8_t *>(&xmax);
    serialized.insert(serialized.end(), xmax_ptr, xmax_ptr + sizeof(xmax));

    // Check if adding entry would exceed block size
    if (!current_block_.empty() && current_block_.size() + serialized.size() > block_size_)
    {
        // Flush current block
        Status status = flushBlock(ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    // If this is the first entry in a new block, record index entry
    if (current_block_.empty())
    {
        IndexEntry idx;
        idx.first_key = key;
        idx.block_offset = current_block_offset_;
        idx.block_size = 0; // Will be filled on flush
        index_entries_.push_back(idx);
    }

    // Add entry to current block
    current_block_.insert(current_block_.end(), serialized.begin(), serialized.end());
    num_entries_++;

    return Status::OK;
}

Status SSTableWriter::flushBlock(ErrorContext *ctx)
{
    if (current_block_.empty())
    {
        return Status::OK; // Nothing to flush
    }

    // Write current block to file
    file_.write(reinterpret_cast<const char *>(current_block_.data()), current_block_.size());

    if (!file_.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write data block");
        return Status::IO_ERROR;
    }

    // Update last index entry with block size
    if (!index_entries_.empty())
    {
        index_entries_.back().block_size = current_block_.size();
    }

    // Update offset for next block
    current_block_offset_ += current_block_.size();

    // Clear current block
    current_block_.clear();

    return Status::OK;
}

Status SSTableWriter::finish(ErrorContext *ctx)
{
    // Flush last block
    if (!current_block_.empty())
    {
        Status status = flushBlock(ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    // Write index block
    uint64_t index_offset = file_.tellp();

    for (const auto &idx : index_entries_)
    {
        // Serialize index entry: [key_len][key][offset][size]
        uint16_t key_len = static_cast<uint16_t>(idx.first_key.size());
        file_.write(reinterpret_cast<const char *>(&key_len), sizeof(key_len));
        file_.write(reinterpret_cast<const char *>(idx.first_key.data()), key_len);
        file_.write(reinterpret_cast<const char *>(&idx.block_offset), sizeof(idx.block_offset));
        file_.write(reinterpret_cast<const char *>(&idx.block_size), sizeof(idx.block_size));
    }

    if (!file_.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write index block");
        return Status::IO_ERROR;
    }

    // Write Bloom filter
    uint64_t bloom_offset = file_.tellp();
    std::vector<uint8_t> bloom_data;
    bloom_filter_.serialize(&bloom_data);
    file_.write(reinterpret_cast<const char *>(bloom_data.data()), bloom_data.size());

    if (!file_.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write bloom filter");
        return Status::IO_ERROR;
    }

    // Write footer
    SSTableFooter footer;
    footer.magic = SSTableFooter::MAGIC_NUMBER;
    footer.version = SSTableFooter::VERSION;
    footer.index_offset = index_offset;
    footer.bloom_offset = bloom_offset;
    footer.num_entries = num_entries_;

    // Min key
    footer.min_key_len = min_key_.size();
    size_t min_copy_len = std::min(min_key_.size(), SSTableFooter::MAX_KEY_SIZE);
    std::memcpy(footer.min_key, min_key_.data(), min_copy_len);
    std::memset(footer.min_key + min_copy_len, 0, SSTableFooter::MAX_KEY_SIZE - min_copy_len);

    // Max key
    footer.max_key_len = max_key_.size();
    size_t max_copy_len = std::min(max_key_.size(), SSTableFooter::MAX_KEY_SIZE);
    std::memcpy(footer.max_key, max_key_.data(), max_copy_len);
    std::memset(footer.max_key + max_copy_len, 0, SSTableFooter::MAX_KEY_SIZE - max_copy_len);

    // Calculate checksum (Phase 6: proper CRC32, for now use placeholder)
    footer.checksum = calculateChecksum();

    file_.write(reinterpret_cast<const char *>(&footer), sizeof(footer));

    if (!file_.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write footer");
        return Status::IO_ERROR;
    }

    file_.close();

    return Status::OK;
}

uint32_t SSTableWriter::calculateChecksum()
{
    // Phase 6: Implement proper CRC32
    // For now, return placeholder
    return 0xDEADBEEF;
}

// ============================================================================
// SSTable Reader Implementation
// ============================================================================

SSTableReader::SSTableReader(const std::string &file_path)
    : file_path_(file_path),
      is_open_(false),
      bloom_filter_(nullptr)
{
}

SSTableReader::~SSTableReader()
{
    if (file_.is_open())
    {
        file_.close();
    }
    if (bloom_filter_ != nullptr)
    {
        delete bloom_filter_;
    }
}

Status SSTableReader::open(ErrorContext *ctx)
{
    // Open file in binary read mode
    file_.open(file_path_, std::ios::binary | std::ios::in);

    if (!file_.is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Failed to open SSTable file for reading");
        return Status::FILE_NOT_FOUND;
    }

    // Get file size
    file_.seekg(0, std::ios::end);
    size_t file_size = file_.tellg();

    if (file_size < sizeof(SSTableFooter))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "SSTable file too small");
        file_.close();
        return Status::IO_ERROR;
    }

    // Read footer from end of file
    size_t footer_offset = file_size - sizeof(SSTableFooter);
    file_.seekg(footer_offset, std::ios::beg);
    file_.read(reinterpret_cast<char *>(&footer_), sizeof(footer_));

    if (!file_.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read footer");
        file_.close();
        return Status::IO_ERROR;
    }

    // Validate magic number
    if (footer_.magic != SSTableFooter::MAGIC_NUMBER)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Invalid SSTable magic number");
        file_.close();
        return Status::IO_ERROR;
    }

    // Validate version
    if (footer_.version != SSTableFooter::VERSION)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Unsupported SSTable version");
        file_.close();
        return Status::IO_ERROR;
    }

    // Read bloom filter
    size_t bloom_size = footer_offset - footer_.bloom_offset;
    std::vector<uint8_t> bloom_data(bloom_size);
    file_.seekg(footer_.bloom_offset, std::ios::beg);
    file_.read(reinterpret_cast<char *>(bloom_data.data()), bloom_size);

    if (!file_.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read bloom filter");
        file_.close();
        return Status::IO_ERROR;
    }

    bloom_filter_ = LSMBloomFilter::deserialize(bloom_data);
    if (bloom_filter_ == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to deserialize bloom filter");
        file_.close();
        return Status::IO_ERROR;
    }

    // Read index block
    file_.seekg(footer_.index_offset, std::ios::beg);

    while (file_.tellg() < static_cast<std::streampos>(footer_.bloom_offset))
    {
        // Read index entry: [key_len][key][offset][size]
        uint16_t key_len;
        file_.read(reinterpret_cast<char *>(&key_len), sizeof(key_len));

        if (!file_.good())
            break;

        IndexEntry idx;
        idx.first_key.resize(key_len);
        file_.read(reinterpret_cast<char *>(idx.first_key.data()), key_len);
        file_.read(reinterpret_cast<char *>(&idx.block_offset), sizeof(idx.block_offset));
        file_.read(reinterpret_cast<char *>(&idx.block_size), sizeof(idx.block_size));

        if (!file_.good())
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read index entry");
            delete bloom_filter_;
            bloom_filter_ = nullptr;
            file_.close();
            return Status::IO_ERROR;
        }

        index_entries_.push_back(idx);
    }

    is_open_ = true;
    return Status::OK;
}

std::vector<uint8_t> SSTableReader::getMinKey() const
{
    return std::vector<uint8_t>(footer_.min_key, footer_.min_key + footer_.min_key_len);
}

std::vector<uint8_t> SSTableReader::getMaxKey() const
{
    return std::vector<uint8_t>(footer_.max_key, footer_.max_key + footer_.max_key_len);
}

int SSTableReader::findBlockIndex(const std::vector<uint8_t> &key) const
{
    // Binary search to find block containing key
    // Returns index of block where key might be found

    if (index_entries_.empty())
    {
        return -1;
    }

    // Check if key is before first block
    if (key < index_entries_[0].first_key)
    {
        return -1;
    }

    // Binary search for last block where first_key <= key
    int left = 0;
    int right = index_entries_.size() - 1;
    int result = 0;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;

        if (index_entries_[mid].first_key <= key)
        {
            result = mid;
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }

    return result;
}

Status SSTableReader::readBlock(uint64_t block_offset,
                                 uint64_t block_size,
                                 std::vector<uint8_t> *block_data,
                                 ErrorContext *ctx)
{
    block_data->resize(block_size);
    file_.seekg(block_offset, std::ios::beg);
    file_.read(reinterpret_cast<char *>(block_data->data()), block_size);

    if (!file_.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read data block");
        return Status::IO_ERROR;
    }

    return Status::OK;
}

Status SSTableReader::parseBlockEntries(const std::vector<uint8_t> &block_data,
                                         std::vector<MemtableEntry> *entries,
                                         ErrorContext *ctx)
{
    entries->clear();

    size_t offset = 0;

    while (offset < block_data.size())
    {
        // Check if we have enough data for key_len
        if (offset + sizeof(uint16_t) > block_data.size())
            break;

        // Read key_len
        uint16_t key_len;
        std::memcpy(&key_len, &block_data[offset], sizeof(key_len));
        offset += sizeof(key_len);

        // Check if we have enough data for key
        if (offset + key_len > block_data.size())
            break;

        // Read key
        MemtableEntry entry;
        entry.key.assign(block_data.begin() + offset, block_data.begin() + offset + key_len);
        offset += key_len;

        // Read value_len
        if (offset + sizeof(uint32_t) > block_data.size())
            break;

        uint32_t value_len;
        std::memcpy(&value_len, &block_data[offset], sizeof(value_len));
        offset += sizeof(value_len);

        // Read value
        if (offset + value_len > block_data.size())
            break;

        entry.value.assign(block_data.begin() + offset, block_data.begin() + offset + value_len);
        offset += value_len;

        // Read sequence_number
        if (offset + sizeof(uint64_t) > block_data.size())
            break;

        std::memcpy(&entry.sequence_number, &block_data[offset], sizeof(entry.sequence_number));
        offset += sizeof(entry.sequence_number);

        // Read entry_type
        if (offset + sizeof(uint8_t) > block_data.size())
            break;

        entry.entry_type = block_data[offset];
        offset += sizeof(uint8_t);

        // Read xmin
        if (offset + sizeof(uint64_t) > block_data.size())
            break;

        std::memcpy(&entry.xmin, &block_data[offset], sizeof(entry.xmin));
        offset += sizeof(entry.xmin);

        // Read xmax
        if (offset + sizeof(uint64_t) > block_data.size())
            break;

        std::memcpy(&entry.xmax, &block_data[offset], sizeof(entry.xmax));
        offset += sizeof(entry.xmax);

        entries->push_back(entry);
    }

    return Status::OK;
}

bool SSTableReader::isEntryVisible(const MemtableEntry &entry,
                                    uint64_t current_xid,
                                    TransactionManager *txn_mgr) const
{
    // FIREBIRD MGA VISIBILITY RULES
    // See /MGA_RULES.md for complete specification

    // Rule 1: Own changes always visible
    if (entry.xmin == current_xid)
    {
        return entry.xmax == 0 || entry.xmax != current_xid;
    }

    // Rule 2: Check if creator transaction is committed
    // Use TIP-based visibility (NO PostgreSQL Snapshot!)
    TransactionState xmin_state;
    Status status = txn_mgr->getTransactionState(entry.xmin, xmin_state, nullptr);
    if (status != Status::OK || xmin_state != TransactionState::COMMITTED)
    {
        // Not committed = not visible
        return false;
    }

    // Rule 3: Creator must be older than current transaction
    if (entry.xmin >= current_xid)
    {
        // Future transaction = not visible
        return false;
    }

    // Rule 4: Check if deleted
    if (entry.xmax != 0)
    {
        // Entry was deleted - check if deletion is visible
        if (entry.xmax == current_xid)
        {
            // Deleted by us = not visible
            return false;
        }

        TransactionState xmax_state;
        status = txn_mgr->getTransactionState(entry.xmax, xmax_state, nullptr);
        if (status == Status::OK && xmax_state == TransactionState::COMMITTED && entry.xmax < current_xid)
        {
            // Deletion committed and older = not visible
            return false;
        }
    }

    // Entry is visible
    return true;
}

Status SSTableReader::get(const std::vector<uint8_t> &key,
                           uint64_t current_xid,
                           TransactionManager *txn_mgr,
                           std::vector<uint8_t> *value_out,
                           bool *found,
                           ErrorContext *ctx)
{
    *found = false;

    if (!is_open_)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "SSTable not open");
        return Status::IO_ERROR;
    }

    // Step 1: Check bloom filter
    if (!bloom_filter_->mightContain(key))
    {
        // Definitely not present
        return Status::OK;
    }

    // Step 2: Check if key is in range
    std::vector<uint8_t> min_key = getMinKey();
    std::vector<uint8_t> max_key = getMaxKey();

    if (key < min_key || key > max_key)
    {
        return Status::OK;
    }

    // Step 3: Binary search index to find data block
    int block_idx = findBlockIndex(key);
    if (block_idx < 0)
    {
        return Status::OK;
    }

    // Step 4: Read data block
    const IndexEntry &idx = index_entries_[block_idx];
    std::vector<uint8_t> block_data;
    Status status = readBlock(idx.block_offset, idx.block_size, &block_data, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Step 5: Parse entries from block
    std::vector<MemtableEntry> entries;
    status = parseBlockEntries(block_data, &entries, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Step 6: Search for key in entries (with MGA visibility)
    for (const auto &entry : entries)
    {
        if (entry.key == key)
        {
            // Check visibility
            if (isEntryVisible(entry, current_xid, txn_mgr))
            {
                if (entry.entry_type == ENTRY_TYPE_INSERT)
                {
                    *value_out = entry.value;
                    *found = true;
                    return Status::OK;
                }
                else if (entry.entry_type == ENTRY_TYPE_DELETE)
                {
                    // Tombstone
                    *found = false;
                    return Status::OK;
                }
            }
        }
    }

    return Status::OK;
}

Status SSTableReader::scan(const std::vector<uint8_t> &start_key,
                            const std::vector<uint8_t> &end_key,
                            uint64_t current_xid,
                            TransactionManager *txn_mgr,
                            std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> *results,
                            ErrorContext *ctx)
{
    results->clear();

    if (!is_open_)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "SSTable not open");
        return Status::IO_ERROR;
    }

    // Determine range of blocks to scan
    int start_block = 0;
    int end_block = index_entries_.size() - 1;

    if (!start_key.empty())
    {
        start_block = findBlockIndex(start_key);
        if (start_block < 0)
            start_block = 0;
    }

    // Scan blocks in range
    std::vector<uint8_t> last_key;

    for (int block_idx = start_block; block_idx <= end_block; ++block_idx)
    {
        const IndexEntry &idx = index_entries_[block_idx];

        // Read block
        std::vector<uint8_t> block_data;
        Status status = readBlock(idx.block_offset, idx.block_size, &block_data, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Parse entries
        std::vector<MemtableEntry> entries;
        status = parseBlockEntries(block_data, &entries, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Filter entries by range and visibility
        for (const auto &entry : entries)
        {
            // Check if in range
            if (!start_key.empty() && entry.key < start_key)
                continue;

            if (!end_key.empty() && entry.key >= end_key)
                return Status::OK; // Done

            // Skip duplicate keys (keep only newest visible version)
            if (entry.key == last_key)
                continue;

            // Check visibility
            if (isEntryVisible(entry, current_xid, txn_mgr))
            {
                if (entry.entry_type == ENTRY_TYPE_INSERT)
                {
                    results->push_back({entry.key, entry.value});
                    last_key = entry.key;
                }
                else if (entry.entry_type == ENTRY_TYPE_DELETE)
                {
                    // Tombstone - skip this key
                    last_key = entry.key;
                }
            }
        }
    }

    return Status::OK;
}

/**
 * Load all entries from SSTable (for compaction)
 *
 * Reads ALL entries without visibility filtering
 * Used by compaction to access invisible entries for garbage collection
 */
Status SSTableReader::loadAllEntries(std::vector<MemtableEntry> *entries, ErrorContext *ctx)
{
    if (!is_open_)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "SSTable not open");
        return Status::FILE_NOT_FOUND;
    }

    if (!entries)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "entries is nullptr");
        return Status::INVALID_ARGUMENT;
    }

    entries->clear();

    // Read all data blocks using the index
    for (const auto &idx : index_entries_)
    {
        // Read data block
        std::vector<uint8_t> block_data;
        Status status = readBlock(idx.block_offset, idx.block_size, &block_data, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Parse entries from block (WITHOUT visibility filtering)
        std::vector<MemtableEntry> block_entries;
        status = parseBlockEntries(block_data, &block_entries, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Append all entries
        entries->insert(entries->end(), block_entries.begin(), block_entries.end());
    }

    return Status::OK;
}

} // namespace core
} // namespace scratchbird
