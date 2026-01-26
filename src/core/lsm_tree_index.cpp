/**
 * LSMTreeIndex Implementation
 *
 * Orchestrates all LSM-Tree components:
 * - Memtable (in-memory writes)
 * - SSTable Writer/Reader (disk persistence)
 * - Compaction Manager (background merging)
 * - Bloom Filters (read optimization)
 *
 * Write path: memtable → flush → Level 0 SSTable → compact → Level 1-3
 * Read path: memtable → immutable memtable → Level 0-3 SSTables
 */

#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/logger.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <queue>
#include <cstring>
#include <filesystem>

namespace scratchbird
{
namespace core
{

namespace {
bool updateLsmValueForMapping(const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                              std::vector<uint8_t> *value)
{
    if (!value || value->size() != (sizeof(uint64_t) + sizeof(uint16_t)))
    {
        return false;
    }

    uint64_t gpid = 0;
    for (size_t i = 0; i < sizeof(uint64_t); ++i)
    {
        gpid |= (static_cast<uint64_t>((*value)[i]) << (i * 8));
    }

    auto it = tid_mapping.find(gpid);
    if (it == tid_mapping.end())
    {
        return false;
    }

    uint64_t new_gpid = it->second;
    for (size_t i = 0; i < sizeof(uint64_t); ++i)
    {
        (*value)[i] = static_cast<uint8_t>((new_gpid >> (i * 8)) & 0xFF);
    }

    return true;
}
} // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

LSMTreeIndex::LSMTreeIndex(Database *db,
                           const std::string &index_path,
                           TransactionManager *txn_mgr,
                           size_t memtable_size_mb)
    : db_(db),
      index_path_(index_path),
      txn_mgr_(txn_mgr),
      memtable_max_size_(memtable_size_mb * 1024 * 1024),
      block_size_(db ? db->page_size() : 4096),  // Use DB page size, fallback to 4KB
      compaction_shutdown_(false)
{
    // Initialize 4 levels
    sstables_.resize(4);
}

LSMTreeIndex::~LSMTreeIndex()
{
    // Close will handle cleanup
    close(nullptr);
}

// ============================================================================
// Lifecycle Methods
// ============================================================================

Status LSMTreeIndex::create(ErrorContext *ctx)
{
    // Create index directory
    if (mkdir(index_path_.c_str(), 0755) != 0 && errno != EEXIST)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                         ("Failed to create index directory: " + index_path_).c_str());
        return Status::IO_ERROR;
    }

    // Create level subdirectories
    for (uint32_t level = 0; level < 4; level++)
    {
        std::string level_path = index_path_ + "/level" + std::to_string(level);
        if (mkdir(level_path.c_str(), 0755) != 0 && errno != EEXIST)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to create level directory: " + level_path).c_str());
            return Status::IO_ERROR;
        }
    }

    // Initialize compaction manager
    compaction_mgr_ = std::make_unique<LSMCompactionManager>(txn_mgr_);
    Status status = compaction_mgr_->initialize(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Create active memtable
    active_memtable_ = std::make_unique<Memtable>(memtable_max_size_);

    return Status::OK;
}

Status LSMTreeIndex::open(ErrorContext *ctx)
{
    // Check if index directory exists
    struct stat st;
    if (stat(index_path_.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND,
                         ("Index directory not found: " + index_path_).c_str());
        return Status::FILE_NOT_FOUND;
    }

    // Initialize compaction manager
    compaction_mgr_ = std::make_unique<LSMCompactionManager>(txn_mgr_);
    Status status = compaction_mgr_->initialize(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Load existing SSTables
    status = loadExistingSSTables(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Create active memtable
    active_memtable_ = std::make_unique<Memtable>(memtable_max_size_);

    // Start background compaction thread
    compaction_shutdown_.store(false);
    compaction_thread_ = std::thread(&LSMTreeIndex::compactionThreadFunc, this);

    return Status::OK;
}

std::unique_ptr<LSMTreeIndex> LSMTreeIndex::open(Database* db,
                                                   const ID& index_uuid,
                                                   uint32_t root_page,
                                                   ErrorContext* ctx)
{
    (void)root_page;  // Unused parameter - LSM-Tree is file-based, not page-based

    if (!db)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database pointer");
        return nullptr;
    }

    // Construct LSM tree path: <db_directory>/indexes/lsm/<index_uuid>
    // For now, use a temporary directory pattern since Database doesn't expose its directory
    // This should be updated when Database provides a getIndexDirectory() method
    std::string index_path = "/tmp/scratchbird/indexes/lsm/" + index_uuid.toString();

    // Get transaction manager from database
    TransactionManager* txn_mgr = db->transaction_manager();
    if (!txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Transaction manager not available");
        return nullptr;
    }

    // Create LSM tree instance (default 4MB memtable size)
    auto index = std::make_unique<LSMTreeIndex>(db, index_path, txn_mgr, 4);

    // Open the existing index
    Status status = index->open(ctx);
    if (status != Status::OK)
    {
        return nullptr;
    }

    return index;
}

Status LSMTreeIndex::close(ErrorContext *ctx)
{
    // Signal compaction thread to stop
    compaction_shutdown_.store(true);
    if (compaction_thread_.joinable())
    {
        compaction_thread_.join();
    }

    // Flush active memtable if it has data
    if (active_memtable_ && active_memtable_->getNumEntries() > 0)
    {
        flush(ctx);
    }

    // Clear all SSTables
    std::lock_guard<std::mutex> lock(sstables_mutex_);
    for (auto &level : sstables_)
    {
        level.clear();
    }

    return Status::OK;
}

// ============================================================================
// Put / Get / Remove Operations
// ============================================================================

Status LSMTreeIndex::put(const std::vector<uint8_t> &key,
                         const std::vector<uint8_t> &value,
                         uint64_t xid,
                         ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(memtable_mutex_);

    // Try to insert into active memtable
    Status status = active_memtable_->put(key, value, xid, ctx);

    // If memtable is full, flush and retry
    if (status == Status::OOM)
    {
        // Mark active memtable as immutable
        immutable_memtable_ = std::move(active_memtable_);

        // Create new active memtable
        active_memtable_ = std::make_unique<Memtable>(memtable_max_size_);

        // Flush immutable memtable in background
        // For now, flush synchronously (async can be added later)
        status = flushImmutableMemtable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Retry insert in new memtable
        status = active_memtable_->put(key, value, xid, ctx);
    }

    return status;
}

Status LSMTreeIndex::get(const std::vector<uint8_t> &key,
                         uint64_t xid,
                         std::vector<uint8_t> *value_out,
                         bool *found,
                         ErrorContext *ctx)
{
    *found = false;

    // 1. Check active memtable
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);
        if (active_memtable_)
        {
            Status status = active_memtable_->get(key, xid, txn_mgr_, value_out, found, ctx);
            if (status != Status::OK || *found)
            {
                return status;
            }
        }
    }

    // 2. Check immutable memtable
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);
        if (immutable_memtable_)
        {
            Status status = immutable_memtable_->get(key, xid, txn_mgr_, value_out, found, ctx);
            if (status != Status::OK || *found)
            {
                return status;
            }
        }
    }

    // 3. Check SSTables (Level 0-3, newest first)
    {
        std::lock_guard<std::mutex> lock(sstables_mutex_);

        // Level 0: Check all files (unsorted, newest first)
        for (auto it = sstables_[0].rbegin(); it != sstables_[0].rend(); ++it)
        {
            SSTableReader *reader = it->get();
            Status status = reader->get(key, xid, txn_mgr_, value_out, found, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            if (*found)
            {
                return Status::OK;
            }
        }

        // Levels 1-3: Check files (use Bloom filters)
        for (uint32_t level = 1; level < 4; level++)
        {
            for (auto &reader_ptr : sstables_[level])
            {
                SSTableReader *reader = reader_ptr.get();
                Status status = reader->get(key, xid, txn_mgr_, value_out, found, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                if (*found)
                {
                    return Status::OK;
                }
            }
        }
    }

    // Not found in any level
    return Status::OK;
}

Status LSMTreeIndex::remove(const std::vector<uint8_t> &key,
                            uint64_t xid,
                            ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(memtable_mutex_);

    // Insert tombstone (DELETE entry) into memtable
    Status status = active_memtable_->remove(key, xid, ctx);

    // If memtable is full, flush and retry
    if (status == Status::OOM)
    {
        // Mark active memtable as immutable
        immutable_memtable_ = std::move(active_memtable_);

        // Create new active memtable
        active_memtable_ = std::make_unique<Memtable>(memtable_max_size_);

        // Flush immutable memtable
        status = flushImmutableMemtable(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Retry insert in new memtable
        status = active_memtable_->remove(key, xid, ctx);
    }

    return status;
}

Status LSMTreeIndex::scan(const std::vector<uint8_t> &start_key,
                          const std::vector<uint8_t> &end_key,
                          uint64_t xid,
                          std::vector<MemtableEntry> *entries_out,
                          ErrorContext *ctx)
{
    if (!entries_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "entries_out cannot be null");
        return Status::INVALID_ARGUMENT;
    }

    entries_out->clear();

    // Helper structure for k-way merge
    struct ScanSource
    {
        enum class Type
        {
            ACTIVE_MEMTABLE,
            IMMUTABLE_MEMTABLE,
            SSTABLE
        };

        Type type;
        uint32_t level;        // For SSTables: 0-3
        uint32_t sstable_idx;  // For SSTables: index within level

        // Current scan results from this source
        std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> entries;
        size_t current_pos;    // Current position in entries vector

        ScanSource() : type(Type::ACTIVE_MEMTABLE), level(0), sstable_idx(0), current_pos(0) {}

        // Get current entry (nullptr if exhausted)
        const std::pair<std::vector<uint8_t>, std::vector<uint8_t>> *getCurrentEntry() const
        {
            if (current_pos >= entries.size())
            {
                return nullptr;
            }
            return &entries[current_pos];
        }

        // Advance to next entry
        void advance()
        {
            if (current_pos < entries.size())
            {
                current_pos++;
            }
        }

        // Check if exhausted
        bool isExhausted() const
        {
            return current_pos >= entries.size();
        }
    };

    // Priority queue entry for k-way merge
    struct MergeEntry
    {
        std::vector<uint8_t> key;
        std::vector<uint8_t> value;
        size_t source_id;  // Index in sources array

        // For min-heap: smallest key comes first
        bool operator>(const MergeEntry &other) const
        {
            return key > other.key;
        }
    };

    // Vector of scan sources (memtables + SSTables)
    std::vector<ScanSource> sources;

    // ========================================================================
    // STEP 1: Scan Active Memtable
    // ========================================================================
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);

        if (active_memtable_)
        {
            ScanSource source;
            source.type = ScanSource::Type::ACTIVE_MEMTABLE;

            const std::vector<uint8_t> *start_ptr = start_key.empty() ? nullptr : &start_key;
            const std::vector<uint8_t> *end_ptr = end_key.empty() ? nullptr : &end_key;

            Status status = active_memtable_->scan(start_ptr, end_ptr, xid, txn_mgr_,
                                                   &source.entries, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            if (!source.entries.empty())
            {
                sources.push_back(std::move(source));
            }
        }

        // ========================================================================
        // STEP 2: Scan Immutable Memtable
        // ========================================================================
        if (immutable_memtable_)
        {
            ScanSource source;
            source.type = ScanSource::Type::IMMUTABLE_MEMTABLE;

            const std::vector<uint8_t> *start_ptr = start_key.empty() ? nullptr : &start_key;
            const std::vector<uint8_t> *end_ptr = end_key.empty() ? nullptr : &end_key;

            Status status = immutable_memtable_->scan(start_ptr, end_ptr, xid, txn_mgr_,
                                                      &source.entries, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            if (!source.entries.empty())
            {
                sources.push_back(std::move(source));
            }
        }
    }

    // ========================================================================
    // STEP 3: Scan SSTables (All Levels)
    // ========================================================================
    {
        std::lock_guard<std::mutex> lock(sstables_mutex_);

        for (uint32_t level = 0; level < 4; level++)
        {
            const auto &level_sstables = sstables_[level];

            for (uint32_t sstable_idx = 0; sstable_idx < level_sstables.size(); sstable_idx++)
            {
                const auto &sstable = level_sstables[sstable_idx];

                if (!sstable || !sstable->isOpen())
                {
                    continue;
                }

                // Quick range check: skip SSTable if range doesn't overlap
                std::vector<uint8_t> sstable_min = sstable->getMinKey();
                std::vector<uint8_t> sstable_max = sstable->getMaxKey();

                // Skip if: SSTable max < start_key OR SSTable min > end_key
                if (!start_key.empty() && sstable_max < start_key)
                {
                    continue;  // SSTable is entirely before start_key
                }
                if (!end_key.empty() && sstable_min > end_key)
                {
                    continue;  // SSTable is entirely after end_key
                }

                // Scan this SSTable
                ScanSource source;
                source.type = ScanSource::Type::SSTABLE;
                source.level = level;
                source.sstable_idx = sstable_idx;

                Status status = sstable->scan(start_key, end_key, xid, txn_mgr_,
                                             &source.entries, ctx);
                if (status != Status::OK)
                {
                    // Log error but continue with other SSTables
                    // SSTable read errors shouldn't fail entire scan
                    continue;
                }

                if (!source.entries.empty())
                {
                    sources.push_back(std::move(source));
                }
            }
        }
    }

    // ========================================================================
    // STEP 4: K-way Merge
    // ========================================================================

    // Special case: no sources
    if (sources.empty())
    {
        return Status::OK;
    }

    // Special case: only one source (no merge needed)
    if (sources.size() == 1)
    {
        for (const auto &kv_pair : sources[0].entries)
        {
            MemtableEntry entry;
            entry.key = kv_pair.first;
            entry.value = kv_pair.second;
            entry.sequence_number = 0;  // Not used for output
            entry.entry_type = ENTRY_TYPE_INSERT;
            entry.xmin = 0;  // Already filtered by visibility
            entry.xmax = 0;
            entries_out->push_back(entry);
        }
        return Status::OK;
    }

    // K-way merge using priority queue
    std::priority_queue<MergeEntry, std::vector<MergeEntry>, std::greater<MergeEntry>> pq;

    // Initialize priority queue with first entry from each source
    for (size_t source_id = 0; source_id < sources.size(); source_id++)
    {
        const auto *entry = sources[source_id].getCurrentEntry();
        if (entry)
        {
            MergeEntry merge_entry;
            merge_entry.key = entry->first;
            merge_entry.value = entry->second;
            merge_entry.source_id = source_id;
            pq.push(merge_entry);
        }
    }

    // Track last key to avoid duplicates
    std::vector<uint8_t> last_key;
    bool first_entry = true;

    // Merge loop
    while (!pq.empty())
    {
        // Pop entry with smallest key
        MergeEntry current = pq.top();
        pq.pop();

        // Check if this is a duplicate key
        if (!first_entry && current.key == last_key)
        {
            // Skip duplicate (we already processed newest version)
            // Just advance the source and continue
            sources[current.source_id].advance();

            const auto *next_entry = sources[current.source_id].getCurrentEntry();
            if (next_entry)
            {
                MergeEntry next_merge;
                next_merge.key = next_entry->first;
                next_merge.value = next_entry->second;
                next_merge.source_id = current.source_id;
                pq.push(next_merge);
            }
            continue;
        }

        // This is the newest version of this key - add to results
        MemtableEntry result_entry;
        result_entry.key = current.key;
        result_entry.value = current.value;
        result_entry.sequence_number = 0;  // Not used for scan results
        result_entry.entry_type = ENTRY_TYPE_INSERT;
        result_entry.xmin = 0;  // Already visibility filtered
        result_entry.xmax = 0;

        entries_out->push_back(result_entry);

        // Update last key
        last_key = current.key;
        first_entry = false;

        // Advance source and add next entry to priority queue
        sources[current.source_id].advance();

        const auto *next_entry = sources[current.source_id].getCurrentEntry();
        if (next_entry)
        {
            MergeEntry next_merge;
            next_merge.key = next_entry->first;
            next_merge.value = next_entry->second;
            next_merge.source_id = current.source_id;
            pq.push(next_merge);
        }
    }

    return Status::OK;
}

// ============================================================================
// Flush Operations
// ============================================================================

Status LSMTreeIndex::flush(ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(memtable_mutex_);

    // Mark active memtable as immutable
    if (!active_memtable_ || active_memtable_->getNumEntries() == 0)
    {
        return Status::OK;  // Nothing to flush
    }

    immutable_memtable_ = std::move(active_memtable_);

    // Create new active memtable
    active_memtable_ = std::make_unique<Memtable>(memtable_max_size_);

    // Flush immutable memtable
    return flushImmutableMemtable(ctx);
}

Status LSMTreeIndex::flushImmutableMemtable(ErrorContext *ctx)
{
    if (!immutable_memtable_ || immutable_memtable_->getNumEntries() == 0)
    {
        return Status::OK;
    }

    // Generate SSTable path
    std::string sstable_path = generateSSTablePath(0);

    // Create SSTable writer
    SSTableWriter writer(sstable_path, block_size_);
    Status status = writer.open(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Get all entries from immutable memtable
    std::vector<MemtableEntry> entries;
    status = immutable_memtable_->getAllEntries(&entries, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Write entries to SSTable
    for (const auto &entry : entries)
    {
        status = writer.addEntry(entry.key, entry.value, entry.sequence_number,
                                entry.entry_type, entry.xmin, entry.xmax, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    // Finish writing SSTable
    status = writer.finish(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Get file size
    struct stat st;
    uint64_t file_size = 0;
    if (stat(sstable_path.c_str(), &st) == 0)
    {
        file_size = st.st_size;
    }

    // Add SSTable to compaction manager
    status = compaction_mgr_->addSSTable(0, sstable_path, file_size, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Open SSTable reader and add to level 0
    {
        std::lock_guard<std::mutex> lock(sstables_mutex_);
        auto reader = std::make_unique<SSTableReader>(sstable_path, block_size_);
        status = reader->open(ctx);
        if (status != Status::OK)
        {
            return status;
        }
        sstables_[0].push_back(std::move(reader));
    }

    // Clear immutable memtable
    immutable_memtable_.reset();

    return Status::OK;
}

// ============================================================================
// Background Compaction Thread
// ============================================================================

void LSMTreeIndex::compactionThreadFunc()
{
    while (!compaction_shutdown_.load())
    {
        // Check if compaction is needed
        if (compaction_mgr_->needsCompaction())
        {
            // Select compaction task
            CompactionTask task;
            ErrorContext ctx;
            Status status = compaction_mgr_->selectCompactionTask(&task, &ctx);
            if (status == Status::OK)
            {
                // Execute compaction
                status = compaction_mgr_->executeCompaction(task, &ctx);
                if (status != Status::OK)
                {
                    // Log error but continue (compaction failure is not fatal)
                    LOG_WARNING(STORAGE, "LSM-Tree: Compaction failed with status %d (non-fatal, will retry)",
                               static_cast<int>(status));
                }
            }
        }

        // Sleep for 1 second before checking again
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

std::string LSMTreeIndex::generateSSTablePath(uint32_t level)
{
    // Generate unique filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

    std::ostringstream oss;
    oss << index_path_ << "/level" << level << "/sstable_"
        << std::setfill('0') << std::setw(16) << now_ms.count()
        << ".sst";

    return oss.str();
}

Status LSMTreeIndex::loadExistingSSTables(ErrorContext *ctx)
{
    // Load SSTables from each level
    for (uint32_t level = 0; level < 4; level++)
    {
        std::string level_path = index_path_ + "/level" + std::to_string(level);

        // Open directory
        DIR *dir = opendir(level_path.c_str());
        if (!dir)
        {
            // Level directory doesn't exist (OK for new index)
            continue;
        }

        // Read all .sst files
        std::vector<std::string> sstable_paths;
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string filename(entry->d_name);
            if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".sst")
            {
                sstable_paths.push_back(level_path + "/" + filename);
            }
        }
        closedir(dir);

        // Sort paths (oldest first)
        std::sort(sstable_paths.begin(), sstable_paths.end());

        // Open each SSTable and add to level
        for (const auto &path : sstable_paths)
        {
            // Open reader
            auto reader = std::make_unique<SSTableReader>(path, block_size_);
            Status status = reader->open(ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status,
                                 ("Failed to open SSTable: " + path).c_str());
                return status;
            }

            // Get file size
            struct stat st;
            uint64_t file_size = 0;
            if (stat(path.c_str(), &st) == 0)
            {
                file_size = st.st_size;
            }

            // Add to compaction manager
            status = compaction_mgr_->addSSTable(level, path, file_size, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Add to level
            sstables_[level].push_back(std::move(reader));
        }
    }

    return Status::OK;
}

Status LSMTreeIndex::getStatistics(Statistics *stats_out, ErrorContext *ctx)
{
    if (!stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "stats_out is null");
        return Status::INVALID_ARGUMENT;
    }

    std::memset(stats_out, 0, sizeof(Statistics));

    // Get memtable stats
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);
        if (active_memtable_)
        {
            stats_out->active_memtable_entries = active_memtable_->getNumEntries();
            stats_out->active_memtable_size = active_memtable_->getSize();
        }
        if (immutable_memtable_)
        {
            stats_out->immutable_memtable_entries = immutable_memtable_->getNumEntries();
            stats_out->immutable_memtable_size = immutable_memtable_->getSize();
        }
    }

    // Get SSTable stats
    {
        std::lock_guard<std::mutex> lock(sstables_mutex_);
        stats_out->level0_sstables = sstables_[0].size();
        stats_out->level1_sstables = sstables_[1].size();
        stats_out->level2_sstables = sstables_[2].size();
        stats_out->level3_sstables = sstables_[3].size();
    }

    // Get total size
    uint64_t total_sstables, total_size;
    compaction_mgr_->getStatistics(&total_sstables, &total_size);
    stats_out->total_size_bytes = total_size;

    return Status::OK;
}

Status LSMTreeIndex::updateTIDsAfterMigration(const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                                              uint64_t *tids_updated_out,
                                              uint64_t *files_modified_out,
                                              ErrorContext *ctx)
{
    if (tids_updated_out != nullptr)
    {
        *tids_updated_out = 0;
    }
    if (files_modified_out != nullptr)
    {
        *files_modified_out = 0;
    }

    if (tid_mapping.empty())
    {
        return Status::OK;
    }

    uint64_t total_updated = 0;
    uint64_t total_files_modified = 0;

    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);
        if (active_memtable_)
        {
            uint64_t updated = 0;
            Status status = active_memtable_->updateTIDsAfterMigration(tid_mapping, &updated, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            total_updated += updated;
        }
        if (immutable_memtable_)
        {
            uint64_t updated = 0;
            Status status = immutable_memtable_->updateTIDsAfterMigration(tid_mapping, &updated, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            total_updated += updated;
        }
    }

    {
        std::lock_guard<std::mutex> lock(sstables_mutex_);
        for (auto &level : sstables_)
        {
            for (auto &reader : level)
            {
                if (!reader)
                {
                    continue;
                }

                if (!reader->isOpen())
                {
                    Status status = reader->open(ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                }

                auto iter = reader->createIterator();
                if (!iter)
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to create SSTable iterator");
                    return Status::IO_ERROR;
                }

                std::string src_path = reader->getFilePath();
                std::string tmp_path = src_path + ".tmp";

                SSTableWriter writer(tmp_path, block_size_, reader->compressionType());
                Status status = writer.open(ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                bool file_updated = false;
                for (; iter->isValid(); iter->next())
                {
                    std::vector<uint8_t> value = iter->value();
                    if (updateLsmValueForMapping(tid_mapping, &value))
                    {
                        total_updated++;
                        file_updated = true;
                    }

                    Status add_status = writer.addEntry(iter->key(), value,
                                                        iter->sequenceNumber(),
                                                        iter->entryType(),
                                                        iter->xmin(),
                                                        iter->xmax(),
                                                        ctx);
                    if (add_status != Status::OK)
                    {
                        writer.close(nullptr);
                        return add_status;
                    }
                }

                Status finish_status = writer.finish(ctx);
                if (finish_status != Status::OK)
                {
                    writer.close(nullptr);
                    return finish_status;
                }
                writer.close(nullptr);

                if (!file_updated)
                {
                    std::filesystem::remove(tmp_path);
                    continue;
                }

                reader->close(nullptr);

                std::error_code ec;
                std::filesystem::rename(tmp_path, src_path, ec);
                if (ec)
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                                     ("Failed to replace SSTable: " + src_path).c_str());
                    return Status::IO_ERROR;
                }

                auto new_reader = std::make_unique<SSTableReader>(src_path, block_size_);
                status = new_reader->open(ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                reader = std::move(new_reader);
                total_files_modified++;
            }
        }
    }

    if (tids_updated_out != nullptr)
    {
        *tids_updated_out = total_updated;
    }
    if (files_modified_out != nullptr)
    {
        *files_modified_out = total_files_modified;
    }

    return Status::OK;
}

} // namespace core
} // namespace scratchbird
