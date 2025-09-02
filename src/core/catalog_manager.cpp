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
    uint32_t next_schema_id;
    uint32_t next_table_id;
    uint32_t schemas_page;    // Page containing schemas table
    uint32_t tables_page;     // Page containing tables table  
    uint32_t columns_page;    // Page containing columns table
    uint8_t reserved[4060];   // Padding for 16KB page
};

// Schema record on disk
struct SchemaRecord {
    uint32_t schema_id;
    char schema_name[64];
    char owner[64];
    uint64_t created_time;
    uint32_t is_valid;       // 1 if valid, 0 if deleted
};

// Table record on disk
struct TableRecord {
    uint32_t table_id;
    uint32_t schema_id;
    char table_name[64];
    uint32_t root_page;
    uint32_t column_count;
    uint64_t row_count;
    uint64_t created_time;
    uint32_t is_valid;
};

// Column record on disk
struct ColumnRecord {
    uint32_t table_id;
    uint16_t column_id;
    char column_name[64];
    uint16_t data_type;
    uint32_t max_length;
    uint8_t nullable;
    uint8_t has_default;
    char default_value[128];
    uint32_t is_valid;
};

#pragma pack(pop)

// Simple heap page for catalog tables
struct CatalogHeapPage {
    PageHeader header;
    uint32_t record_count;
    uint32_t free_offset;
    uint8_t data[];  // Variable length records
};

CatalogManager::CatalogManager(Database* db) : db_(db) {
    DEBUG_LOG_DB("CatalogManager created");
}

CatalogManager::~CatalogManager() {
    DEBUG_LOG_DB("CatalogManager destroyed");

}

Status CatalogManager::initialize(ErrorContext* ctx) {
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
    uint8_t* page_buffer = new(std::nothrow) uint8_t[db_->page_size()];
    if (!page_buffer) {
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate page buffer");
        return Status::OOM;
    }
    
    memset(page_buffer, 0, db_->page_size());
    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);
    
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
    
    status = db_->write_page(schemas_table_page_, page_buffer, ctx);
    if (status != Status::Ok) {
        delete[] page_buffer;
        return status;
    }
    
    // Allocate and initialize tables page
    status = pm->allocate_page(tables_table_page_, ctx);
    if (status != Status::Ok) {
        delete[] page_buffer;
        return status;
    }
    
    heap->header.page_id = tables_table_page_;
    status = db_->write_page(tables_table_page_, page_buffer, ctx);
    if (status != Status::Ok) {
        delete[] page_buffer;
        return status;
    }
    
    // Allocate and initialize columns page
    status = pm->allocate_page(columns_table_page_, ctx);
    if (status != Status::Ok) {
        delete[] page_buffer;
        return status;
    }
    
    heap->header.page_id = columns_table_page_;
    status = db_->write_page(columns_table_page_, page_buffer, ctx);
    if (status != Status::Ok) {
        delete[] page_buffer;
        return status;
    }
    
    delete[] page_buffer;
    
    // Update root page with table locations
    status = write_catalog_root(ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Create default schemas
    uint32_t schema_id;
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
        status = read_column_records(table_id, ctx);
        if (status != Status::Ok) {
            return status;
        }
    }
    
    DEBUG_LOG_DB("Catalog loaded: " << schema_count_ << " schemas, " 
                 << table_count_ << " tables");
    
    return Status::Ok;
}

Status CatalogManager::create_schema(const std::string& schema_name,
                                   const std::string& owner,
                                   uint32_t& schema_id,
                                   ErrorContext* ctx) {
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
    schema.schema_id = next_schema_id_++;
    schema.schema_name = schema_name;
    schema.owner = owner;
    schema.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Write to disk
    Status status = write_schema_record(schema, ctx);
    if (status != Status::Ok) {
        next_schema_id_--;  // Rollback ID
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
    
    DEBUG_LOG_DB("Created schema: " << schema_name << " (ID: " << schema_id << ")");
    
    return status;
}

Status CatalogManager::get_schema(uint32_t schema_id, SchemaInfo& info,
                                ErrorContext* ctx) {
    auto it = schema_cache_.find(schema_id);
    if (it == schema_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Schema not found: " + std::to_string(schema_id)).c_str());
        return Status::InvalidArgument;
    }
    
    info = it->second;
    return Status::Ok;
}

Status CatalogManager::get_schema(const std::string& schema_name, SchemaInfo& info,
                                ErrorContext* ctx) {
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

Status CatalogManager::create_table(uint32_t schema_id,
                                  const std::string& table_name,
                                  const std::vector<ColumnInfo>& columns,
                                  uint32_t& table_id,
                                  ErrorContext* ctx) {
    // Verify schema exists
    if (schema_cache_.find(schema_id) == schema_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Schema not found: " + std::to_string(schema_id)).c_str());
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
    table.table_id = next_table_id_++;
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
        next_table_id_--;  // Rollback
        pm->free_page(root_page, ctx);  // Free allocated page
        return status;
    }
    
    // Write column records
    status = write_column_records(table.table_id, columns, ctx);
    if (status != Status::Ok) {
        // TODO: Rollback table record
        next_table_id_--;
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
    
    DEBUG_LOG_DB("Created table: " << table_name << " (ID: " << table_id 
                 << ") with " << columns.size() << " columns");
    
    return status;
}

Status CatalogManager::get_table(uint32_t table_id, TableInfo& info,
                               ErrorContext* ctx) {
    auto it = table_cache_.find(table_id);
    if (it == table_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Table not found: " + std::to_string(table_id)).c_str());
        return Status::InvalidArgument;
    }
    
    info = it->second;
    return Status::Ok;
}

Status CatalogManager::get_table(uint32_t schema_id, const std::string& table_name,
                               TableInfo& info, ErrorContext* ctx) {
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

Status CatalogManager::list_tables(uint32_t schema_id, std::vector<TableInfo>& tables,
                                 ErrorContext* ctx) {
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

Status CatalogManager::get_columns(uint32_t table_id, std::vector<ColumnInfo>& columns,
                                 ErrorContext* ctx) {
    auto it = column_cache_.find(table_id);
    if (it == column_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Columns not found for table: " + std::to_string(table_id)).c_str());
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

Status CatalogManager::get_column(uint32_t table_id, const std::string& column_name,
                                ColumnInfo& info, ErrorContext* ctx) {
    auto it = column_cache_.find(table_id);
    if (it == column_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         ("Table not found: " + std::to_string(table_id)).c_str());
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
    root->next_schema_id = next_schema_id_;
    root->next_table_id = next_table_id_;
    
    root->schemas_page = schemas_table_page_;
    root->tables_page = tables_table_page_;
    root->columns_page = columns_table_page_;
    

    
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
    next_schema_id_ = root->next_schema_id;
    next_table_id_ = root->next_table_id;
    schemas_table_page_ = root->schemas_page;
    tables_table_page_ = root->tables_page;
    columns_table_page_ = root->columns_page;
    
    return bp->unpin_page(CATALOG_ROOT_PAGE, false, ctx);
}

Status CatalogManager::write_schema_record(const SchemaInfo& schema, ErrorContext* ctx) {
    // For now, append to schemas heap page
    // TODO: Implement proper heap page management
    
    BufferPool* bp = db_->buffer_pool();
    void* page_buffer;
    Status status = bp->pin_page(schemas_table_page_, &page_buffer, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);
    
    // Check if we have space
    if (heap->free_offset + sizeof(SchemaRecord) > db_->page_size()) {
        bp->unpin_page(schemas_table_page_, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Schema table full");
        return Status::InvalidArgument;
    }
    
    // Write record
    SchemaRecord* record = reinterpret_cast<SchemaRecord*>(
        reinterpret_cast<uint8_t*>(page_buffer) + heap->free_offset);
    
    record->schema_id = schema.schema_id;
    strncpy(record->schema_name, schema.schema_name.c_str(), 63);
    record->schema_name[63] = '\0';
    strncpy(record->owner, schema.owner.c_str(), 63);
    record->owner[63] = '\0';
    record->created_time = schema.created_time;
    record->is_valid = 1;
    
    heap->record_count++;
    heap->free_offset += sizeof(SchemaRecord);
    heap->header.free_space -= sizeof(SchemaRecord);
    heap->header.generation++;
    

    
    return bp->unpin_page(schemas_table_page_, true, ctx);
}

Status CatalogManager::read_schema_records(ErrorContext* ctx) {
    BufferPool* bp = db_->buffer_pool();
    void* page_buffer;
    Status status = bp->pin_page(schemas_table_page_, &page_buffer, ctx);
    if (status != Status::Ok) {
        SET_ERROR_CONTEXT(ctx, status, "Failed to read schemas page");
        return status;
    }
    
    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);
    
    schema_cache_.clear();
    uint32_t offset = sizeof(CatalogHeapPage);
    

    
    for (uint32_t i = 0; i < heap->record_count; i++) {
        SchemaRecord* record = reinterpret_cast<SchemaRecord*>(
            reinterpret_cast<uint8_t*>(page_buffer) + offset);
        
        if (record->is_valid) {
            SchemaInfo info;
            info.schema_id = record->schema_id;
            info.schema_name = record->schema_name;
            info.owner = record->owner;
            info.created_time = record->created_time;
            
            schema_cache_[info.schema_id] = info;
        }
        
        offset += sizeof(SchemaRecord);
    }
    
    return bp->unpin_page(schemas_table_page_, false, ctx);
}

Status CatalogManager::write_table_record(const TableInfo& table, ErrorContext* ctx) {
    BufferPool* bp = db_->buffer_pool();
    void* page_buffer;
    Status status = bp->pin_page(tables_table_page_, &page_buffer, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);
    
    // Check if we have space
    if (heap->free_offset + sizeof(TableRecord) > db_->page_size()) {
        bp->unpin_page(tables_table_page_, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Tables table full");
        return Status::InvalidArgument;
    }
    
    // Write record
    TableRecord* record = reinterpret_cast<TableRecord*>(
        reinterpret_cast<uint8_t*>(page_buffer) + heap->free_offset);
    
    record->table_id = table.table_id;
    record->schema_id = table.schema_id;
    strncpy(record->table_name, table.table_name.c_str(), 63);
    record->table_name[63] = '\0';
    record->root_page = table.root_page;
    record->column_count = table.column_count;
    record->row_count = table.row_count;
    record->created_time = table.created_time;
    record->is_valid = 1;
    
    heap->record_count++;
    heap->free_offset += sizeof(TableRecord);
    heap->header.free_space -= sizeof(TableRecord);
    heap->header.generation++;
    
    return bp->unpin_page(tables_table_page_, true, ctx);
}

Status CatalogManager::read_table_records(ErrorContext* ctx) {
    BufferPool* bp = db_->buffer_pool();
    void* page_buffer;
    Status status = bp->pin_page(tables_table_page_, &page_buffer, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);
    
    table_cache_.clear();
    uint32_t offset = sizeof(CatalogHeapPage);
    
    for (uint32_t i = 0; i < heap->record_count; i++) {
        TableRecord* record = reinterpret_cast<TableRecord*>(
            reinterpret_cast<uint8_t*>(page_buffer) + offset);
        
        if (record->is_valid) {
            TableInfo info;
            info.table_id = record->table_id;
            info.schema_id = record->schema_id;
            info.table_name = record->table_name;
            info.root_page = record->root_page;
            info.column_count = record->column_count;
            info.row_count = record->row_count;
            info.created_time = record->created_time;
            
            table_cache_[info.table_id] = info;
        }
        
        offset += sizeof(TableRecord);
    }
    
    return bp->unpin_page(tables_table_page_, false, ctx);
}

Status CatalogManager::write_column_records(uint32_t table_id,
                                          const std::vector<ColumnInfo>& columns,
                                          ErrorContext* ctx) {
    BufferPool* bp = db_->buffer_pool();
    void* page_buffer;
    Status status = bp->pin_page(columns_table_page_, &page_buffer, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);
    
    // Check if we have space for all columns
    size_t needed_space = columns.size() * sizeof(ColumnRecord);
    if (heap->free_offset + needed_space > db_->page_size()) {
        bp->unpin_page(columns_table_page_, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Columns table full");
        return Status::InvalidArgument;
    }
    
    // Write all column records
    for (const auto& col : columns) {
        ColumnRecord* record = reinterpret_cast<ColumnRecord*>(
            reinterpret_cast<uint8_t*>(page_buffer) + heap->free_offset);
        
        record->table_id = table_id;
        record->column_id = col.column_id;
        strncpy(record->column_name, col.column_name.c_str(), 63);
        record->column_name[63] = '\0';
        record->data_type = col.data_type;
        record->max_length = col.max_length;
        record->nullable = col.nullable ? 1 : 0;
        record->has_default = col.has_default ? 1 : 0;
        strncpy(record->default_value, col.default_value.c_str(), 127);
        record->default_value[127] = '\0';
        record->is_valid = 1;
        
        heap->record_count++;
        heap->free_offset += sizeof(ColumnRecord);
        heap->header.free_space -= sizeof(ColumnRecord);
    }
    
    heap->header.generation++;
    
    return bp->unpin_page(columns_table_page_, true, ctx);
}

Status CatalogManager::read_column_records(uint32_t table_id, ErrorContext* ctx) {
    BufferPool* bp = db_->buffer_pool();
    void* page_buffer;
    Status status = bp->pin_page(columns_table_page_, &page_buffer, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);
    
    std::vector<ColumnInfo> columns;
    uint32_t offset = sizeof(CatalogHeapPage);
    
    for (uint32_t i = 0; i < heap->record_count; i++) {
        ColumnRecord* record = reinterpret_cast<ColumnRecord*>(
            reinterpret_cast<uint8_t*>(page_buffer) + offset);
        
        if (record->is_valid && record->table_id == table_id) {
            ColumnInfo info;
            info.table_id = record->table_id;
            info.column_id = record->column_id;
            info.column_name = record->column_name;
            info.data_type = record->data_type;
            info.max_length = record->max_length;
            info.nullable = record->nullable != 0;
            info.has_default = record->has_default != 0;
            info.default_value = record->default_value;
            
            columns.push_back(info);
        }
        
        offset += sizeof(ColumnRecord);
    }
    
    if (!columns.empty()) {
        column_cache_[table_id] = columns;
    }
    
    return bp->unpin_page(columns_table_page_, false, ctx);
}

} // namespace core
} // namespace scratchbird