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

#include "scratchbird/core/lsm_tree.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

LSMTreeIndex::LSMTreeIndex(const std::string &index_path,
                           TransactionManager *txn_mgr,
                           size_t memtable_size_mb)
    : index_path_(index_path),
      txn_mgr_(txn_mgr),
      memtable_max_size_(memtable_size_mb * 1024 * 1024),
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
    // NOT IMPLEMENTED IN PHASE 6
    // Future: K-way merge across memtable + all SSTables
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "Range scan not yet implemented");
    return Status::NOT_IMPLEMENTED;
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
    SSTableWriter writer(sstable_path, 4096);
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
        auto reader = std::make_unique<SSTableReader>(sstable_path);
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
                    // TODO: Add proper logging
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
            auto reader = std::make_unique<SSTableReader>(path);
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

} // namespace core
} // namespace scratchbird
