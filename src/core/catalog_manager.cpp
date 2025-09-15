#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/debug.h"
#include <cstring>
#include <algorithm>

namespace scratchbird {
namespace core {

// Catalog page structures
#pragma pack(push, 1)

// Root catalog page - points to system tables
struct CatalogRootPage {
    PageHeader header;
    uint32_t schema_count;
    uint32_t table_count;
    uint32_t schemas_page;    // Page containing schemas table
    uint32_t tables_page;     // Page containing tables table  
    uint32_t columns_page;    // Page containing columns table
    uint32_t indexes_page;    // Page containing indexes table
    uint8_t reserved[4064];   // Padding for 16KB page
};

// Schema record on disk
struct SchemaRecord {
    ID schema_id;
    char schema_name[64];
    char owner[64];
    uint64_t created_time;
    uint32_t is_valid;       // 1 if valid, 0 if deleted
};

// Table record on disk
struct TableRecord {
    ID table_id;
    ID schema_id;
    char table_name[64];
    uint32_t root_page;
    uint32_t column_count;
    uint64_t row_count;
    uint64_t created_time;
    uint32_t is_valid;
};

// Column record on disk
struct ColumnRecord {
    ID table_id;
    uint16_t column_id;
    char column_name[64];
    uint16_t data_type;
    uint32_t max_length;
    uint8_t nullable;
    uint8_t has_default;
    char default_value[128];
    uint32_t is_valid;
};

// Index record on disk
struct IndexRecord {
    ID index_id;
    ID table_id;
    char index_name[64];
    uint32_t root_page;
    uint8_t is_unique;
    uint16_t column_count;
    uint16_t column_ids[16]; // Max 16 columns per index
    uint64_t created_time;
    uint32_t is_valid;
};

#pragma pack(pop)

CatalogManager::CatalogManager(Database* db) : db_(db) {
    DEBUG_LOG_DB("CatalogManager created");
}

CatalogManager::~CatalogManager() {
    DEBUG_LOG_DB("CatalogManager destroyed");

}

Status CatalogManager::initialize(ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    DEBUG_LOG_DB("Initializing system catalog");
    
    // Write catalog root page
    Status status = write_catalog_root(ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Allocate pages for system tables
    PageManager* pm = db_->page_manager();
    if (!pm) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "PageManager not available");
        return Status::InvalidArgument;
    }
    
    DEBUG_LOG_DB("CatalogManager::initialize - allocating pages");
    
    // Allocate schema table page
    status = pm->allocate_page(schemas_table_page_, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Initialize schemas table page
    auto page_buffer = std::make_unique<uint8_t[]>(db_->page_size());
    if (!page_buffer) {
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate page buffer");
        return Status::OOM;
    }
    
    memset(page_buffer.get(), 0, db_->page_size());
    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer.get());
    
    heap->header.magic = kMagicSBRD;
    heap->header.version = 1;
    heap->header.page_type = PAGE_TYPE_HEAP;
    heap->header.page_size = db_->page_size();
    heap->header.page_id = schemas_table_page_;
    heap->header.flags = 0;
    memcpy(heap->header.database_uuid, db_->uuid().bytes.data(), 16);
    heap->header.generation = 1;
    heap->record_count = 0;
    heap->free_offset = sizeof(CatalogHeapPage);
    heap->header.free_space = db_->page_size() - sizeof(CatalogHeapPage);
    heap->header.item_count = 0;
    heap->header.free_offset = sizeof(CatalogHeapPage);
    
    status = db_->write_page(schemas_table_page_, page_buffer.get(), ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Allocate and initialize tables page
    status = pm->allocate_page(tables_table_page_, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    heap->header.page_id = tables_table_page_;
    status = db_->write_page(tables_table_page_, page_buffer.get(), ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Allocate and initialize columns page
    status = pm->allocate_page(columns_table_page_, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    heap->header.page_id = columns_table_page_;
    status = db_->write_page(columns_table_page_, page_buffer.get(), ctx);
    if (status != Status::Ok) {
        return status;
    }

    // Allocate and initialize indexes page
    status = pm->allocate_page(indexes_table_page_, ctx);
    if (status != Status::Ok) {
        return status;
    }

    heap->header.page_id = indexes_table_page_;
    status = db_->write_page(indexes_table_page_, page_buffer.get(), ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Update root page with table locations
    status = write_catalog_root(ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Create default schemas
    ID schema_id;
    status = create_schema("[sys]", "[root]", schema_id, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    DEBUG_LOG_DB("System catalog initialized with schemas page=" << schemas_table_page_
                 << ", tables page=" << tables_table_page_ 
                 << ", columns page=" << columns_table_page_);
    
    return Status::Ok;
}

Status CatalogManager::load(ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    DEBUG_LOG_DB("Loading system catalog");

    
    // Try to read catalog root
    Status status = read_catalog_root(ctx);

    if (status == Status::PageCorrupt) {
        // Catalog not initialized yet, initialize it
        DEBUG_LOG_DB("Catalog not found, initializing");

        return initialize(ctx);
    } else if (status != Status::Ok) {
        return status;
    }
    
    // If we successfully read catalog root, load the data

    
    // Load schemas
    status = read_schema_records(ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Load tables
    status = read_table_records(ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Load columns for each table
    for (const auto& [table_id, table_info] : table_cache_) {
        status = read_column_records(table_info.table_id, ctx);
        if (status != Status::Ok) {
            return status;
        }
    }
    
    // Load indexes
    status = read_index_records(ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    DEBUG_LOG_DB("Catalog loaded: " << schema_count_ << " schemas, " 
                 << table_count_ << " tables");
    
    return Status::Ok;
}

Status CatalogManager::create_schema(const std::string& schema_name,
                                   const std::string& owner,
                                   ID& schema_id,
                                   ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Check if schema already exists
    for (const auto& [id, info] : schema_cache_) {
        if (info.schema_name == schema_name) {
            SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                            ("Schema already exists: " + schema_name).c_str());
            return Status::InvalidArgument;
        }
    }
    
    // Create new schema
    SchemaInfo schema;
    schema.schema_id = generate_uuid_v7();
    schema.schema_name = schema_name;
    schema.owner = owner;
    schema.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Write to disk
    Status status = write_schema_record(schema, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Update cache
    schema_cache_[schema.schema_id] = schema;
    schema_count_++;
    schema_id = schema.schema_id;
    
    // Update root page
    status = write_catalog_root(ctx);
    if (status == Status::Ok) {
        // Sync to ensure persistence
        db_->sync(ctx);
    }
    
    DEBUG_LOG_DB("Created schema: " << schema_name << " (ID: " << schema_id.to_string() << ")");
    
    return status;
}

Status CatalogManager::get_schema(const ID& schema_id, SchemaInfo& info,
                                ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = schema_cache_.find(schema_id);
    if (it == schema_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Schema not found: " + schema_id.to_string()).c_str());
        return Status::InvalidArgument;
    }
    
    info = it->second;
    return Status::Ok;
}

Status CatalogManager::get_schema(const std::string& schema_name, SchemaInfo& info,
                                ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, schema_info] : schema_cache_) {
        if (schema_info.schema_name == schema_name) {
            info = schema_info;
            return Status::Ok;
        }
    }
    
    SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                     ("Schema not found: " + schema_name).c_str());
    return Status::InvalidArgument;
}

Status CatalogManager::list_schemas(std::vector<SchemaInfo>& schemas,
                                  ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    schemas.clear();
    schemas.reserve(schema_cache_.size());
    
    for (const auto& [id, info] : schema_cache_) {
        schemas.push_back(info);
    }
    
    // Sort by schema_id for consistent ordering
    std::sort(schemas.begin(), schemas.end(),
              [](const SchemaInfo& a, const SchemaInfo& b) {
                  return a.schema_id < b.schema_id;
              });
    
    return Status::Ok;
}

Status CatalogManager::create_table(const ID& schema_id,
                                  const std::string& table_name,
                                  const std::vector<ColumnInfo>& columns,
                                  ID& table_id,
                                  ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Verify schema exists
    if (schema_cache_.find(schema_id) == schema_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Schema not found: " + schema_id.to_string()).c_str());
        return Status::InvalidArgument;
    }
    
    // Check if table already exists in schema
    for (const auto& [id, info] : table_cache_) {
        if (info.schema_id == schema_id && info.table_name == table_name) {
            SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                            ("Table already exists: " + table_name).c_str());
            return Status::InvalidArgument;
        }
    }
    
    // Allocate root page for table data
    PageManager* pm = db_->page_manager();
    uint32_t root_page;
    Status status = pm->allocate_page(root_page, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Create table info
    TableInfo table;
    table.table_id = generate_uuid_v7();
    table.schema_id = schema_id;
    table.table_name = table_name;
    table.root_page = root_page;
    table.column_count = columns.size();
    table.row_count = 0;
    table.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Write table record
    status = write_table_record(table, ctx);
    if (status != Status::Ok) {
        pm->free_page(root_page, ctx);  // Free allocated page
        return status;
    }
    
    // Write column records
    status = write_column_records(table.table_id, columns, ctx);
    if (status != Status::Ok) {
        // TODO: Rollback table record
        pm->free_page(root_page, ctx);
        return status;
    }
    
    // Update caches
    table_cache_[table.table_id] = table;
    column_cache_[table.table_id] = columns;
    table_count_++;
    table_id = table.table_id;
    
    // Update root page
    status = write_catalog_root(ctx);
    if (status == Status::Ok) {
        // Sync to ensure persistence
        db_->sync(ctx);
    }
    
    DEBUG_LOG_DB("Created table: " << table_name << " (ID: " << table_id.to_string() 
                 << ") with " << columns.size() << " columns");
    
    return status;
}

Status CatalogManager::get_table(const ID& table_id, TableInfo& info,
                               ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = table_cache_.find(table_id);
    if (it == table_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Table not found: " + table_id.to_string()).c_str());
        return Status::InvalidArgument;
    }
    
    info = it->second;
    return Status::Ok;
}

Status CatalogManager::get_table(const ID& schema_id, const std::string& table_name,
                               TableInfo& info, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, table_info] : table_cache_) {
        if (table_info.schema_id == schema_id && table_info.table_name == table_name) {
            info = table_info;
            return Status::Ok;
        }
    }
    
    SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                     ("Table not found: " + table_name).c_str());
    return Status::InvalidArgument;
}

Status CatalogManager::list_tables(const ID& schema_id, std::vector<TableInfo>& tables,
                                 ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    tables.clear();
    
    for (const auto& [id, info] : table_cache_) {
        if (info.schema_id == schema_id) {
            tables.push_back(info);
        }
    }
    
    // Sort by table name for consistent ordering
    std::sort(tables.begin(), tables.end(),
              [](const TableInfo& a, const TableInfo& b) {
                  return a.table_name < b.table_name;
              });
    
    return Status::Ok;
}

Status CatalogManager::get_columns(const ID& table_id, std::vector<ColumnInfo>& columns,
                                 ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = column_cache_.find(table_id);
    if (it == column_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Columns not found for table: " + table_id.to_string()).c_str());
        return Status::InvalidArgument;
    }
    
    columns = it->second;
    
    // Sort by column_id for consistent ordering
    std::sort(columns.begin(), columns.end(),
              [](const ColumnInfo& a, const ColumnInfo& b) {
                  return a.column_id < b.column_id;
              });
    
    return Status::Ok;
}

Status CatalogManager::get_column(const ID& table_id, const std::string& column_name,
                                ColumnInfo& info, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = column_cache_.find(table_id);
    if (it == column_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Table not found: " + table_id.to_string()).c_str());
        return Status::InvalidArgument;
    }
    
    for (const auto& col : it->second) {
        if (col.column_name == column_name) {
            info = col;
            return Status::Ok;
        }
    }
    
    SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                     ("Column not found: " + column_name).c_str());
    return Status::InvalidArgument;
}

Status CatalogManager::create_index(const ID& table_id,
                                  const std::string& index_name,
                                  const std::vector<std::string>& column_names,
                                  ID& index_id,
                                  bool is_unique,
                                  ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Verify table exists
    if (table_cache_.find(table_id) == table_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument,
                         ("Table not found: " + table_id.to_string()).c_str());
        return Status::InvalidArgument;
    }

    // Check if index already exists
    for (const auto& [id, info] : index_cache_) {
        if (info.table_id == table_id && info.index_name == index_name) {
            SET_ERROR_CONTEXT(ctx, Status::InvalidArgument,
                            ("Index already exists: " + index_name).c_str());
            return Status::InvalidArgument;
        }
    }

    // Resolve column names to column IDs
    std::vector<uint16_t> column_ids;
    for (const auto& col_name : column_names) {
        ColumnInfo col_info;
        Status status = get_column(table_id, col_name, col_info, ctx);
        if (status != Status::Ok) {
            return status;
        }
        column_ids.push_back(col_info.column_id);
    }

    // Allocate root page for index data
    PageManager* pm = db_->page_manager();
    uint32_t root_page;
    Status status = pm->allocate_page(root_page, ctx);
    if (status != Status::Ok) {
        return status;
    }

    // Create index info
    IndexInfo index;
    index.index_id = generate_uuid_v7();
    index.table_id = table_id;
    index.index_name = index_name;
    index.root_page = root_page;
    index.is_unique = is_unique;
    index.column_ids = column_ids;
    index.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Write index record
    status = write_index_record(index, ctx);
    if (status != Status::Ok) {
        pm->free_page(root_page, ctx);
        return status;
    }

    // Update cache
    index_cache_[index.index_id] = index;
    index_id = index.index_id;

    // TODO: Update root page with index count
    // status = write_catalog_root(ctx);
    // if (status == Status::Ok) {
    //     db_->sync(ctx);
    // }

    DEBUG_LOG_DB("Created index: " << index_name << " (ID: " << index_id.to_string() << ")");

    return status;
}

Status CatalogManager::get_index(const ID& index_id, IndexInfo& info,
                               ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_cache_.find(index_id);
    if (it == index_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument,
                         ("Index not found: " + index_id.to_string()).c_str());
        return Status::InvalidArgument;
    }

    info = it->second;
    return Status::Ok;
}

Status CatalogManager::get_index(const ID& table_id, const std::string& index_name,
                               IndexInfo& info, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, index_info] : index_cache_) {
        if (index_info.table_id == table_id && index_info.index_name == index_name) {
            info = index_info;
            return Status::Ok;
        }
    }

    SET_ERROR_CONTEXT(ctx, Status::InvalidArgument,
                     ("Index not found: " + index_name).c_str());
    return Status::InvalidArgument;
}

Status CatalogManager::list_indexes_for_table(const ID& table_id,
                                              std::vector<IndexInfo>& indexes,
                                              ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    indexes.clear();

    for (const auto& [id, info] : index_cache_) {
        if (info.table_id == table_id) {
            indexes.push_back(info);
        }
    }

    std::sort(indexes.begin(), indexes.end(),
              [](const IndexInfo& a, const IndexInfo& b) {
                  return a.index_name < b.index_name;
              });

    return Status::Ok;
}

Status CatalogManager::write_catalog_root(ErrorContext* ctx) {
    BufferPool* bp = db_->buffer_pool();
    if (!bp) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "BufferPool not available");
        return Status::InvalidArgument;
    }
    
    // Check if we need to allocate the catalog root page
    PageManager* pm = db_->page_manager();
    if (pm && !pm->is_allocated(CATALOG_ROOT_PAGE)) {
        uint32_t allocated_page;
        Status alloc_status = pm->allocate_page(allocated_page, ctx);

        if (alloc_status != Status::Ok || allocated_page != CATALOG_ROOT_PAGE) {
            // We need page 3 specifically, if we can't get it there's a problem
            SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Could not allocate catalog root page");
            return Status::InvalidArgument;
        }
    }
    
    void* page_buffer;
    Status status = bp->pin_page(CATALOG_ROOT_PAGE, &page_buffer, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    CatalogRootPage* root = reinterpret_cast<CatalogRootPage*>(page_buffer);
    
    // Initialize header if this is first write
    if (root->header.magic != kMagicSBRD) {
        memset(page_buffer, 0, db_->page_size());
        root->header.magic = kMagicSBRD;
        root->header.version = 1;
        root->header.page_size = db_->page_size();
        root->header.page_id = CATALOG_ROOT_PAGE;
        memcpy(root->header.database_uuid, db_->uuid().bytes.data(), 16);
    }
    
    // Always ensure page type is correct
    root->header.page_type = PAGE_TYPE_CATALOG_ROOT;
    root->header.generation++;
    root->schema_count = schema_count_;
    root->table_count = table_count_;
    
    root->schemas_page = schemas_table_page_;
    root->tables_page = tables_table_page_;
    root->columns_page = columns_table_page_;
    root->indexes_page = indexes_table_page_;
    

    
    return bp->unpin_page(CATALOG_ROOT_PAGE, true, ctx);
}

Status CatalogManager::read_catalog_root(ErrorContext* ctx) {
    BufferPool* bp = db_->buffer_pool();
    if (!bp) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "BufferPool not available");
        return Status::InvalidArgument;
    }
    
    void* page_buffer;
    Status status = bp->pin_page(CATALOG_ROOT_PAGE, &page_buffer, ctx);
    if (status == Status::IoError) {
        // Page doesn't exist yet - catalog not initialized

        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Catalog root page not found");
        return Status::PageCorrupt;
    } else if (status != Status::Ok) {

        return status;
    }
    
    CatalogRootPage* root = reinterpret_cast<CatalogRootPage*>(page_buffer);
    

    
    // Validate catalog root
    if (root->header.page_type != PAGE_TYPE_CATALOG_ROOT) {
        bp->unpin_page(CATALOG_ROOT_PAGE, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid catalog root page");
        return Status::PageCorrupt;
    }
    
    schema_count_ = root->schema_count;
    table_count_ = root->table_count;
    schemas_table_page_ = root->schemas_page;
    tables_table_page_ = root->tables_page;
    columns_table_page_ = root->columns_page;
    indexes_table_page_ = root->indexes_page;
    
    return bp->unpin_page(CATALOG_ROOT_PAGE, false, ctx);
}

// Helper to write a record to a catalog heap page
template<typename RecordType>
Status CatalogManager::write_record_to_heap_page(uint32_t page_id, const RecordType& record, ErrorContext* ctx) {
    BufferPool* bp = db_->buffer_pool();
    void* page_buffer;
    Status status = bp->pin_page(page_id, &page_buffer, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);
    
    // Check if we have space
    if (heap->free_offset + sizeof(RecordType) > db_->page_size()) {
        bp->unpin_page(page_id, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::PageFull, "Catalog heap page full");
        return Status::InvalidArgument;
    }
    
    // Write record
    RecordType* dest_record = reinterpret_cast<RecordType*>(
        reinterpret_cast<uint8_t*>(page_buffer) + heap->free_offset);
    memcpy(dest_record, &record, sizeof(RecordType));
    
    heap->record_count++;
    heap->free_offset += sizeof(RecordType);
    heap->header.free_space -= sizeof(RecordType);
    heap->header.generation++;
    
    return bp->unpin_page(page_id, true, ctx);
}

template<typename RecordType, typename InfoType>
Status CatalogManager::read_records_to_vector(uint32_t page_id, std::vector<InfoType>& results,
                                              std::function<bool(const RecordType&)> filter,
                                              std::function<void(const RecordType&, InfoType&)> converter,
                                              ErrorContext* ctx) {
    BufferPool* bp = db_->buffer_pool();
    void* page_buffer;
    Status status = bp->pin_page(page_id, &page_buffer, ctx);
    if (status != Status::Ok) {
        SET_ERROR_CONTEXT(ctx, status, "Failed to read catalog heap page");
        return status;
    }

    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);

    results.clear();
    uint32_t offset = sizeof(CatalogHeapPage);

    for (uint32_t i = 0; i < heap->record_count; i++) {
        RecordType* record = reinterpret_cast<RecordType*>(
            reinterpret_cast<uint8_t*>(page_buffer) + offset);

        if (record->is_valid && filter(*record)) {
            InfoType info;
            converter(*record, info);
            results.push_back(info);
        }

        offset += sizeof(RecordType);
    }

    return bp->unpin_page(page_id, false, ctx);
}

Status CatalogManager::write_schema_record(const SchemaInfo& schema, ErrorContext* ctx) {
    SchemaRecord record;
    record.schema_id = schema.schema_id;
    strncpy(record.schema_name, schema.schema_name.c_str(), 63);
    record.schema_name[63] = '\0';
    strncpy(record.owner, schema.owner.c_str(), 63);
    record.owner[63] = '\0';
    record.created_time = schema.created_time;
    record.is_valid = 1;
    
    return write_record_to_heap_page(schemas_table_page_, record, ctx);
}

Status CatalogManager::read_schema_records(ErrorContext* ctx) {
    auto converter = [](const SchemaRecord& record, SchemaInfo& info) {
        info.schema_id = record.schema_id;
        info.schema_name = record.schema_name;
        info.owner = record.owner;
        info.created_time = record.created_time;
    };
    auto key_extractor = [](const SchemaInfo& info) { return info.schema_id; };
    return read_records_from_heap_page<SchemaRecord, SchemaInfo, ID>(schemas_table_page_, schema_cache_, converter, key_extractor, ctx);
}

Status CatalogManager::write_table_record(const TableInfo& table, ErrorContext* ctx) {
    TableRecord record;
    record.table_id = table.table_id;
    record.schema_id = table.schema_id;
    strncpy(record.table_name, table.table_name.c_str(), 63);
    record.table_name[63] = '\0';
    record.root_page = table.root_page;
    record.column_count = table.column_count;
    record.row_count = table.row_count;
    record.created_time = table.created_time;
    record.is_valid = 1;
    
    return write_record_to_heap_page(tables_table_page_, record, ctx);
}

Status CatalogManager::read_table_records(ErrorContext* ctx) {
    auto converter = [](const TableRecord& record, TableInfo& info) {
        info.table_id = record.table_id;
        info.schema_id = record.schema_id;
        info.table_name = record.table_name;
        info.root_page = record.root_page;
        info.column_count = record.column_count;
        info.row_count = record.row_count;
        info.created_time = record.created_time;
    };
    auto key_extractor = [](const TableInfo& info) { return info.table_id; };
    return read_records_from_heap_page<TableRecord, TableInfo, ID>(tables_table_page_, table_cache_, converter, key_extractor, ctx);
}

Status CatalogManager::write_column_records(const ID& table_id,
                                          const std::vector<ColumnInfo>& columns,
                                          ErrorContext* ctx) {
    for (const auto& col : columns) {
        ColumnRecord record;
        record.table_id = table_id;
        record.column_id = col.column_id;
        strncpy(record.column_name, col.column_name.c_str(), 63);
        record.column_name[63] = '\0';
        record.data_type = col.data_type;
        record.max_length = col.max_length;
        record.nullable = col.nullable ? 1 : 0;
        record.has_default = col.has_default ? 1 : 0;
        strncpy(record.default_value, col.default_value.c_str(), 127);
        record.default_value[127] = '\0';
        record.is_valid = 1;
        
        Status status = write_record_to_heap_page(columns_table_page_, record, ctx);
        if (status != Status::Ok) {
            return status;
        }
    }
    return Status::Ok;
}

Status CatalogManager::read_column_records(const ID& table_id, ErrorContext* ctx) {
    auto filter = [table_id](const ColumnRecord& record) {
        return record.table_id == table_id;
    };

    auto converter = [](const ColumnRecord& record, ColumnInfo& info) {
        info.table_id = record.table_id;
        info.column_id = record.column_id;
        info.column_name = record.column_name;
        info.data_type = record.data_type;
        info.max_length = record.max_length;
        info.nullable = record.nullable != 0;
        info.has_default = record.has_default != 0;
        info.default_value = record.default_value;
    };

    std::vector<ColumnInfo> columns;
    Status status = read_records_to_vector<ColumnRecord, ColumnInfo>(
        columns_table_page_, columns, filter, converter, ctx);

    if (status == Status::Ok && !columns.empty()) {
        column_cache_[table_id] = columns;
    }

    return status;
}

Status CatalogManager::write_index_record(const IndexInfo& index, ErrorContext* ctx) {
    IndexRecord record;
    record.index_id = index.index_id;
    record.table_id = index.table_id;
    strncpy(record.index_name, index.index_name.c_str(), 63);
    record.index_name[63] = '\0';
    record.root_page = index.root_page;
    record.is_unique = index.is_unique;
    record.column_count = index.column_ids.size();
    for (size_t i = 0; i < index.column_ids.size(); ++i) {
        record.column_ids[i] = index.column_ids[i];
    }
    record.created_time = index.created_time;
    record.is_valid = 1;

    return write_record_to_heap_page(indexes_table_page_, record, ctx);
}

Status CatalogManager::read_index_records(ErrorContext* ctx) {
    auto converter = [](const IndexRecord& record, IndexInfo& info) {
        info.index_id = record.index_id;
        info.table_id = record.table_id;
        info.index_name = record.index_name;
        info.root_page = record.root_page;
        info.is_unique = record.is_unique;
        info.column_ids.assign(record.column_ids, record.column_ids + record.column_count);
        info.created_time = record.created_time;
    };
    auto key_extractor = [](const IndexInfo& info) { return info.index_id; };
    return read_records_from_heap_page<IndexRecord, IndexInfo, ID>(indexes_table_page_, index_cache_, converter, key_extractor, ctx);
}

} // namespace core
} // namespace scratchbird
