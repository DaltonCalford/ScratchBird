#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/debug.h"
#include <cstring>
#include <algorithm>

namespace scratchbird::core
{

// Catalog page structures
#pragma pack(push, 1)

    // Root catalog page - points to system tables
    struct CatalogRootPage
    {
        PageHeader header;
        uint32_t schema_count;
        uint32_t table_count;
        uint32_t schemas_page;        // Page containing schemas table
        uint32_t tables_page;         // Page containing tables table
        uint32_t columns_page;        // Page containing columns table
        uint32_t indexes_page;        // Page containing indexes table
        uint32_t constraints_page;    // Page containing constraints table
        uint32_t sequences_page;      // Page containing sequences table
        uint32_t views_page;          // Page containing views table
        uint32_t triggers_page;       // Page containing triggers table
        uint32_t permissions_page;    // Page containing permissions table
        uint32_t statistics_page;     // Page containing statistics table
        uint32_t collations_page;     // Page containing collations table (legacy)
        uint32_t timezones_page;      // Page containing timezones table
        uint32_t charsets_page;       // Page containing character sets table (pg_charset)
        uint32_t collation_defs_page; // Page containing collation definitions table (pg_collation)
        uint8_t reserved[4024];       // Padding for 16KB page (adjusted for new fields)
    };

    // Schema record on disk
    struct SchemaRecord
    {
        ID schema_id;
        char schema_name[128];          // SQL standard: 128 characters
        char owner[128];                // SQL standard: 128 characters
        uint16_t default_tablespace_id; // Default tablespace for new tables
        uint16_t permissions;           // Bitmask of schema permissions
        uint16_t default_charset;       // CharacterSet enum (0 = inherit from database)
        uint16_t reserved;
        uint32_t default_collation_id; // Collation ID (0 = inherit from database)
        uint32_t acl_oid;              // TOAST reference for ACL (access control list)
        uint32_t search_path_oid;      // TOAST reference for search path
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid; // 1 if valid, 0 if deleted
        uint32_t padding;  // Alignment
    };

    // Table types
    enum class TableType : uint8_t
    {
        HEAP = 0,              // Regular heap table
        INDEX = 1,             // Index-organized table
        TEMPORARY = 2,         // Temporary table
        EXTERNAL = 3,          // External table
        MATERIALIZED_VIEW = 4, // Materialized view
        TOAST = 5              // TOAST table
    };

    // Table record on disk
    struct TableRecord
    {
        ID table_id;
        ID schema_id;
        char table_name[128]; // SQL standard: 128 characters
        uint32_t root_page;
        uint32_t column_count;
        uint64_t row_count;
        uint8_t table_type;            // TableType enum
        uint8_t has_toast;             // 1 if table has TOAST
        uint16_t tablespace_id;        // Tablespace ID (0 = default)
        uint16_t default_charset;      // CharacterSet enum (0 = inherit from schema)
        uint16_t reserved1;            // Reserved for future use
        uint32_t default_collation_id; // Collation ID (0 = inherit from schema)
        uint32_t storage_params_oid;   // TOAST reference for storage parameters
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding; // Alignment
    };

    // Column record on disk
    struct ColumnRecord
    {
        ID table_id;
        ID column_id;
        char column_name[128]; // SQL standard: 128 characters
        uint16_t ordinal;      // Column position in table
        uint16_t data_type;
        uint32_t type_precision; // For DECIMAL, VECTOR dimensions, VARCHAR length
        uint32_t type_scale;     // For DECIMAL scale
        uint32_t max_length;     // Legacy field, use type_precision instead
        uint8_t nullable;
        uint8_t has_default;
        uint8_t is_primary_key;
        uint8_t is_unique;
        uint8_t is_foreign_key;
        uint8_t is_generated;
        uint8_t storage_type;  // TOAST storage strategy
        uint8_t with_timezone; // For TIMESTAMP: 1 = WITH TIME ZONE, 0 = WITHOUT
        uint8_t reserved2;
        uint16_t charset;       // CharacterSet enum (0 = inherit from table)
        uint16_t timezone_hint; // Timezone ID for display (0 = use connection default)
        uint32_t collation_id;  // Collation ID (0 = inherit from table)
        char default_value[128];
        uint32_t default_value_oid; // TOAST reference for large defaults
        uint32_t check_expr_oid;    // TOAST reference for check expressions
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding; // Alignment
    };

    // Index types
    enum class IndexType : uint8_t
    {
        BTREE = 0,    // B-tree index (default)
        HASH = 1,     // Hash index
        VECTOR = 2,   // Vector similarity index (HNSW, IVF, etc.)
        FULLTEXT = 3, // Full-text search index
        GIN = 4,      // Generalized Inverted Index
        GIST = 5,     // Generalized Search Tree
        BRIN = 6      // Block Range Index
    };

    // Index record on disk
    struct IndexRecord
    {
        ID index_id;
        ID table_id;
        char index_name[128]; // SQL standard: 128 characters
        uint32_t root_page;
        uint8_t index_type; // IndexType enum
        uint8_t is_unique;
        uint16_t column_count;
        ID column_ids[16];         // Max 16 columns per index
        uint32_t index_params_oid; // TOAST reference for index parameters (HNSW config, etc.)
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding; // Alignment
    };

    // Timezone record on disk
    struct TimezoneRecord
    {
        uint16_t timezone_id;       // Unique timezone ID
        char name[64];              // Timezone name (e.g., "America/New_York")
        char abbreviation[16];      // Abbreviation (e.g., "EST", "PST")
        int32_t std_offset_minutes; // Standard offset from GMT in minutes
        uint8_t observes_dst;       // 1 if observes DST, 0 otherwise
        uint8_t reserved1;
        uint16_t reserved2;
        // DST transition rules (simplified - in production use IANA tzdata)
        uint8_t dst_start_month;    // Month DST starts (1-12, 0 = no DST)
        uint8_t dst_start_week;     // Week of month (1-5, 0 = last)
        uint8_t dst_start_day;      // Day of week (0-6, 0 = Sunday)
        uint8_t dst_start_hour;     // Hour DST starts (0-23)
        uint8_t dst_end_month;      // Month DST ends
        uint8_t dst_end_week;       // Week of month
        uint8_t dst_end_day;        // Day of week
        uint8_t dst_end_hour;       // Hour DST ends
        int32_t dst_offset_minutes; // Additional offset during DST
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid; // 1 if valid, 0 if deleted
        uint32_t padding;  // Alignment
    };

    // Character set record on disk (pg_charset)
    struct CharsetRecord
    {
        uint16_t charset_id;    // Character set ID (matches CharacterSet enum)
        char name[64];          // Character set name (e.g., "utf8", "latin1")
        char description[128];  // Human-readable description
        uint8_t min_bytes;      // Minimum bytes per character
        uint8_t max_bytes;      // Maximum bytes per character
        uint8_t variable_width; // 1 = variable width, 0 = fixed width
        uint8_t reserved1;
        uint32_t default_collation_id; // Default collation for this charset
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid; // 1 if valid, 0 if deleted
        uint32_t padding;  // Alignment
    };

    // Collation record on disk (pg_collation)
    struct CollationRecord
    {
        uint32_t collation_id;
        char name[128];         // Collation name (e.g., "utf8_general_ci")
        uint16_t charset_id;    // Associated character set ID
        uint8_t collation_type; // CollationType enum value
        uint8_t strength;       // CollationStrength enum value
        uint8_t pad_space;      // 1 = PAD SPACE, 0 = NO PAD
        uint8_t is_default;     // 1 = default for charset, 0 = not default
        uint16_t reserved;
        char locale[32]; // Locale string (e.g., "en_US")
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid; // 1 if valid, 0 if deleted
        uint32_t padding;  // Alignment
    };

    // Constraint types
    enum class ConstraintType : uint8_t
    {
        PRIMARY_KEY = 0, // Primary key constraint
        FOREIGN_KEY = 1, // Foreign key constraint
        UNIQUE = 2,      // Unique constraint
        CHECK = 3,       // Check constraint
        NOT_NULL = 4,    // Not null constraint
        DEFAULT = 5,     // Default value constraint
        EXCLUSION = 6    // Exclusion constraint
    };

    // Constraint record on disk
    struct ConstraintRecord
    {
        ID constraint_id;
        ID table_id;
        char constraint_name[128];  // SQL standard: 128 characters
        uint8_t constraint_type;    // ConstraintType enum
        uint8_t is_deferrable;      // Can be deferred to end of transaction
        uint8_t initially_deferred; // Initially deferred or immediate
        uint8_t reserved_flags;
        uint16_t column_count;  // Number of columns involved
        ID column_ids[16];      // Columns involved in constraint (max 16)
        ID referenced_table_id; // For foreign keys
        uint16_t referenced_column_count;
        ID referenced_column_ids[16]; // Referenced columns for FK
        uint32_t check_expr_oid;      // TOAST reference for check expression
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Sequence record on disk
    struct SequenceRecord
    {
        ID sequence_id;
        ID schema_id;
        char sequence_name[128]; // SQL standard: 128 characters
        int64_t current_value;
        int64_t increment_by;
        int64_t min_value;
        int64_t max_value;
        int64_t cache_size;
        uint8_t cycle;       // 1 if cycle, 0 if no cycle
        uint8_t reserved[7]; // Alignment
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // View record on disk
    struct ViewRecord
    {
        ID view_id;
        ID schema_id;
        char view_name[128];     // SQL standard: 128 characters
        uint32_t definition_oid; // TOAST reference for view definition SQL
        uint8_t is_materialized; // 1 if materialized view
        uint8_t reserved[3];
        uint64_t created_time;
        uint64_t last_refreshed; // For materialized views
        uint32_t is_valid;
        uint32_t padding;
    };

    // Trigger record on disk
    struct TriggerRecord
    {
        ID trigger_id;
        ID table_id;
        char trigger_name[128]; // SQL standard: 128 characters
        uint8_t trigger_timing; // 0=BEFORE, 1=AFTER, 2=INSTEAD OF
        uint8_t trigger_events; // Bitmask: 0x01=INSERT, 0x02=UPDATE, 0x04=DELETE
        uint8_t for_each_row;   // 1 if FOR EACH ROW, 0 if FOR EACH STATEMENT
        uint8_t enabled;        // 1 if enabled
        uint32_t condition_oid; // TOAST reference for WHEN condition
        uint32_t action_oid;    // TOAST reference for trigger action
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Permission record on disk
    struct PermissionRecord
    {
        ID permission_id;
        ID object_id;        // ID of schema, table, etc.
        char grantee[128];   // User/role granted the permission
        uint8_t object_type; // 0=SCHEMA, 1=TABLE, 2=VIEW, 3=SEQUENCE
        uint8_t reserved[3];
        uint32_t privileges;  // Bitmask of privileges
        uint8_t grant_option; // 1 if WITH GRANT OPTION
        uint8_t reserved2[3];
        char grantor[128]; // User who granted the permission
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Statistics record on disk
    struct StatisticsRecord
    {
        ID stats_id;
        ID table_id;
        ID column_id;
        int64_t n_distinct;            // Number of distinct values
        float null_frac;               // Fraction of null values
        float avg_width;               // Average width in bytes
        uint32_t most_common_vals_oid; // TOAST reference for MCVs
        uint32_t histogram_bounds_oid; // TOAST reference for histogram
        uint64_t last_analyzed;        // Timestamp of last ANALYZE
        uint32_t is_valid;
        uint32_t padding;
    };

    // Collation record on disk - see updated CollationRecord structure below at line ~194

#pragma pack(pop)

    CatalogManager::CatalogManager(Database *db) : db_(db)
    {
        DEBUG_LOG_DB("CatalogManager created");
    }

    CatalogManager::~CatalogManager()
    {
        DEBUG_LOG_DB("CatalogManager destroyed");
    }

    auto CatalogManager::initialize(ErrorContext *ctx) -> Status
    {
        // NOTE: Assumes mutex_ is already held by caller (load())
        DEBUG_LOG_DB("Initializing system catalog");

        // Write catalog root page
        Status status = writeCatalogRoot(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate pages for system tables
        PageManager *pm = db_->page_manager();
        if (pm == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "PageManager not available");
            return Status::INVALID_ARGUMENT;
        }

        DEBUG_LOG_DB("CatalogManager::initialize - allocating pages");

        // Allocate schema table page
        status = pm->allocatePage(schemas_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Initialize schemas table page
        auto page_buffer = std::make_unique<uint8_t[]>(db_->page_size());
        if (!page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate page buffer");
            return Status::OOM;
        }

        memset(page_buffer.get(), 0, db_->page_size());
        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer.get());

        heap->header.magic = K_MAGIC_SBRD;
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
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize tables page
        status = pm->allocatePage(tables_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        heap->header.page_id = tables_table_page_;
        status = db_->write_page(tables_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize columns page
        status = pm->allocatePage(columns_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        heap->header.page_id = columns_table_page_;
        status = db_->write_page(columns_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize indexes page
        status = pm->allocatePage(indexes_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        heap->header.page_id = indexes_table_page_;
        status = db_->write_page(indexes_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize constraints page
        status = pm->allocatePage(constraints_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = constraints_table_page_;
        status = db_->write_page(constraints_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize sequences page
        status = pm->allocatePage(sequences_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = sequences_table_page_;
        status = db_->write_page(sequences_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize views page
        status = pm->allocatePage(views_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = views_table_page_;
        status = db_->write_page(views_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize triggers page
        status = pm->allocatePage(triggers_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = triggers_table_page_;
        status = db_->write_page(triggers_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize permissions page
        status = pm->allocatePage(permissions_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = permissions_table_page_;
        status = db_->write_page(permissions_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize statistics page
        status = pm->allocatePage(statistics_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = statistics_table_page_;
        status = db_->write_page(statistics_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize collations page
        status = pm->allocatePage(collations_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = collations_table_page_;
        status = db_->write_page(collations_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize timezones page
        status = pm->allocatePage(timezones_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = timezones_table_page_;
        status = db_->write_page(timezones_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize charsets page (pg_charset)
        status = pm->allocatePage(charsets_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = charsets_table_page_;
        status = db_->write_page(charsets_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize collation definitions page (pg_collation)
        status = pm->allocatePage(collation_defs_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = collation_defs_table_page_;
        status = db_->write_page(collation_defs_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Update root page with table locations
        status = writeCatalogRoot(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create default schemas
        ID schema_id;
        status = createSchemaInternal("[sys]", "[root]", schema_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        DEBUG_LOG_DB("System catalog initialized with schemas page="
                     << schemas_table_page_ << ", tables page=" << tables_table_page_
                     << ", columns page=" << columns_table_page_);

        return Status::OK;
    }

    auto CatalogManager::load(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        DEBUG_LOG_DB("Loading system catalog");

        // Try to read catalog root
        Status status = readCatalogRoot(ctx);

        if (status == Status::PAGE_CORRUPT)
        {
            // Catalog not initialized yet, initialize it
            DEBUG_LOG_DB("Catalog not found, initializing");

            return initialize(ctx);
        }
        if (status != Status::OK)
        {
            return status;
        }

        // If we successfully read catalog root, load the data

        // Load schemas
        status = readSchemaRecords(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Load tables
        status = readTableRecords(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Load columns for each table
        for (const auto &[table_id, table_info] : table_cache_)
        {
            status = readColumnRecords(table_info.table_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        // Load indexes
        status = readIndexRecords(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        DEBUG_LOG_DB("Catalog loaded: " << schema_count_ << " schemas, " << table_count_
                                        << " tables");

        return Status::OK;
    }

    // Internal version without lock (assumes caller holds mutex_)
    auto CatalogManager::createSchemaInternal(const std::string &schema_name,
                                              const std::string &owner, ID &schema_id,
                                              ErrorContext *ctx) -> Status
    {
        // Check if schema already exists
        for (const auto &[id, info] : schema_cache_)
        {
            if (info.schema_name == schema_name)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  ("Schema already exists: " + schema_name).c_str());
                return Status::INVALID_ARGUMENT;
            }
        }

        // Create new schema
        SchemaInfo schema;
        schema.schema_id = generateUuidV7();
        schema.schema_name = schema_name;
        schema.owner = owner;
        schema.default_tablespace_id = 0; // Default tablespace
        schema.permissions = 0x0FFF;      // Default permissions (read, write, create)
        schema.acl_oid = 0;               // No ACL initially
        schema.search_path_oid = 0;       // No custom search path
        schema.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        schema.last_modified_time = schema.created_time;

        // Write to disk
        Status status = writeSchemaRecord(schema, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Update cache
        schema_cache_[schema.schema_id] = schema;
        schema_count_++;
        schema_id = schema.schema_id;

        // Update root page
        status = writeCatalogRoot(ctx);
        if (status == Status::OK)
        {
            // Sync to ensure persistence
            db_->sync(ctx);
        }

        DEBUG_LOG_DB("Created schema: " << schema_name << " (ID: " << schema_id.toString() << ")");

        return status;
    }

    auto CatalogManager::createSchema(const std::string &schema_name, const std::string &owner,
                                      ID &schema_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return createSchemaInternal(schema_name, owner, schema_id, ctx);
    }

    auto CatalogManager::getSchema(const ID &schema_id, SchemaInfo &info, ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = schema_cache_.find(schema_id);
        if (it == schema_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Schema not found: " + schema_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        info = it->second;
        return Status::OK;
    }

    auto CatalogManager::getSchema(const std::string &schema_name, SchemaInfo &info,
                                   ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[id, schema_info] : schema_cache_)
        {
            if (schema_info.schema_name == schema_name)
            {
                info = schema_info;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          ("Schema not found: " + schema_name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    auto CatalogManager::listSchemas(std::vector<SchemaInfo> &schemas, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        schemas.clear();
        schemas.reserve(schema_cache_.size());

        for (const auto &[id, info] : schema_cache_)
        {
            schemas.push_back(info);
        }

        // Sort by schema_id for consistent ordering
        std::sort(schemas.begin(), schemas.end(), [](const SchemaInfo &a, const SchemaInfo &b)
                  { return a.schema_id < b.schema_id; });

        return Status::OK;
    }

    auto CatalogManager::createTable(const ID &schema_id, const std::string &table_name,
                                     const std::vector<ColumnInfo> &columns, ID &table_id,
                                     ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Verify schema exists
        if (schema_cache_.find(schema_id) == schema_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Schema not found: " + schema_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Check if table already exists in schema
        for (const auto &[id, info] : table_cache_)
        {
            if (info.schema_id == schema_id && info.table_name == table_name)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  ("Table already exists: " + table_name).c_str());
                return Status::INVALID_ARGUMENT;
            }
        }

        // Allocate root page for table data
        PageManager *pm = db_->page_manager();
        uint32_t root_page;
        Status status = pm->allocatePage(root_page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create table info
        TableInfo table;
        table.table_id = generateUuidV7();
        table.schema_id = schema_id;
        table.table_name = table_name;
        table.root_page = root_page;
        table.column_count = columns.size();
        table.row_count = 0;
        table.table_type = TableType::HEAP; // Default to heap table
        table.has_toast = false;            // Will be set to true if needed
        table.tablespace_id = 0;            // Default tablespace
        table.storage_params_oid = 0;       // No custom storage parameters
        table.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
        table.last_modified_time = table.created_time;

        // Write table record
        status = writeTableRecord(table, ctx);
        if (status != Status::OK)
        {
            pm->freePage(root_page, ctx); // Free allocated page
            return status;
        }

        // Assign UUIDs and ordinals to columns before writing them
        std::vector<ColumnInfo> columns_with_ids = columns;
        uint16_t ordinal = 0;
        for (auto &col : columns_with_ids)
        {
            col.column_id = generateUuidV7();
            col.ordinal = ordinal++;
            col.created_time = table.created_time;
        }

        // Write column records
        status = writeColumnRecords(table.table_id, columns_with_ids, ctx);
        if (status != Status::OK)
        {
            // Rollback: mark table record as invalid (logical delete)
            deleteTableRecord(table.table_id, ctx);
            pm->freePage(root_page, ctx);
            return status;
        }

        // Update caches
        table_cache_[table.table_id] = table;
        column_cache_[table.table_id] = columns_with_ids;
        table_count_++;
        table_id = table.table_id;

        // Update root page
        status = writeCatalogRoot(ctx);
        if (status == Status::OK)
        {
            // Sync to ensure persistence
            db_->sync(ctx);
        }

        DEBUG_LOG_DB("Created table: " << table_name << " (ID: " << table_id.toString() << ") with "
                                       << columns.size() << " columns");

        return status;
    }

    auto CatalogManager::getTable(const ID &table_id, TableInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = table_cache_.find(table_id);
        if (it == table_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Table not found: " + table_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        info = it->second;
        return Status::OK;
    }

    auto CatalogManager::getTable(const ID &schema_id, const std::string &table_name,
                                  TableInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[id, table_info] : table_cache_)
        {
            if (table_info.schema_id == schema_id && table_info.table_name == table_name)
            {
                info = table_info;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          ("Table not found: " + table_name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    auto CatalogManager::listTables(const ID &schema_id, std::vector<TableInfo> &tables,
                                    ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tables.clear();

        for (const auto &[id, info] : table_cache_)
        {
            if (info.schema_id == schema_id)
            {
                tables.push_back(info);
            }
        }

        // Sort by table name for consistent ordering
        std::sort(tables.begin(), tables.end(), [](const TableInfo &a, const TableInfo &b)
                  { return a.table_name < b.table_name; });

        return Status::OK;
    }

    auto CatalogManager::getColumns(const ID &table_id, std::vector<ColumnInfo> &columns,
                                    ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = column_cache_.find(table_id);
        if (it == column_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Columns not found for table: " + table_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        columns = it->second;

        // Sort by column_id for consistent ordering
        std::sort(columns.begin(), columns.end(), [](const ColumnInfo &a, const ColumnInfo &b)
                  { return a.column_id < b.column_id; });

        return Status::OK;
    }

    auto CatalogManager::getColumn(const ID &table_id, const std::string &column_name,
                                   ColumnInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = column_cache_.find(table_id);
        if (it == column_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Table not found: " + table_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        for (const auto &col : it->second)
        {
            if (col.column_name == column_name)
            {
                info = col;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          ("Column not found: " + column_name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    auto CatalogManager::createIndex(const ID &table_id, const std::string &index_name,
                                     const std::vector<std::string> &column_names, ID &index_id,
                                     bool is_unique, IndexType index_type, ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Verify table exists
        if (table_cache_.find(table_id) == table_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Table not found: " + table_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Check if index already exists
        for (const auto &[id, info] : index_cache_)
        {
            if (info.table_id == table_id && info.index_name == index_name)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  ("Index already exists: " + index_name).c_str());
                return Status::INVALID_ARGUMENT;
            }
        }

        // Resolve column names to column IDs
        std::vector<ID> column_ids;
        for (const auto &col_name : column_names)
        {
            ColumnInfo col_info;
            Status status = getColumn(table_id, col_name, col_info, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            column_ids.push_back(col_info.column_id);
        }

        // Allocate root page for index data
        PageManager *pm = db_->page_manager();
        uint32_t root_page;
        Status status = pm->allocatePage(root_page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create index info
        IndexInfo index;
        index.index_id = generateUuidV7();
        index.table_id = table_id;
        index.index_name = index_name;
        index.root_page = root_page;
        index.index_type = index_type;
        index.is_unique = is_unique;
        index.column_ids = column_ids;
        index.index_params_oid = 0; // Will be set later when index params are added
        index.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();

        // Write index record
        status = writeIndexRecord(index, ctx);
        if (status != Status::OK)
        {
            pm->freePage(root_page, ctx);
            return status;
        }

        // Update cache
        index_cache_[index.index_id] = index;
        index_id = index.index_id;

        // TODO: Update root page with index count
        // status = writeCatalogRoot(ctx);
        // if (status == Status::OK) {
        //     db_->sync(ctx);
        // }

        DEBUG_LOG_DB("Created index: " << index_name << " (ID: " << index_id.toString() << ")");

        return status;
    }

    auto CatalogManager::getIndex(const ID &index_id, IndexInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_cache_.find(index_id);
        if (it == index_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Index not found: " + index_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        info = it->second;
        return Status::OK;
    }

    auto CatalogManager::getIndex(const ID &table_id, const std::string &index_name,
                                  IndexInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[id, index_info] : index_cache_)
        {
            if (index_info.table_id == table_id && index_info.index_name == index_name)
            {
                info = index_info;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          ("Index not found: " + index_name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    auto CatalogManager::listIndexesForTable(const ID &table_id, std::vector<IndexInfo> &indexes,
                                             ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        indexes.clear();

        for (const auto &[id, info] : index_cache_)
        {
            if (info.table_id == table_id)
            {
                indexes.push_back(info);
            }
        }

        std::sort(indexes.begin(), indexes.end(), [](const IndexInfo &a, const IndexInfo &b)
                  { return a.index_name < b.index_name; });

        return Status::OK;
    }

    auto CatalogManager::writeCatalogRoot(ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        if (bp == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        // Check if we need to allocate the catalog root page
        PageManager *pm = db_->page_manager();
        if ((pm != nullptr) && !pm->isAllocated(CATALOG_ROOT_PAGE))
        {
            uint32_t allocated_page;
            Status alloc_status = pm->allocatePage(allocated_page, ctx);

            if (alloc_status != Status::OK || allocated_page != CATALOG_ROOT_PAGE)
            {
                // We need page 3 specifically, if we can't get it there's a problem
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Could not allocate catalog root page");
                return Status::INVALID_ARGUMENT;
            }
        }

        void *page_buffer;
        Status status = bp->pinPage(CATALOG_ROOT_PAGE, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *root = reinterpret_cast<CatalogRootPage *>(page_buffer);

        // Initialize header if this is first write
        if (root->header.magic != K_MAGIC_SBRD)
        {
            memset(page_buffer, 0, db_->page_size());
            root->header.magic = K_MAGIC_SBRD;
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
        root->constraints_page = constraints_table_page_;
        root->sequences_page = sequences_table_page_;
        root->views_page = views_table_page_;
        root->triggers_page = triggers_table_page_;
        root->permissions_page = permissions_table_page_;
        root->statistics_page = statistics_table_page_;
        root->collations_page = collations_table_page_;
        root->timezones_page = timezones_table_page_;
        root->charsets_page = charsets_table_page_;
        root->collation_defs_page = collation_defs_table_page_;

        return bp->unpinPage(CATALOG_ROOT_PAGE, true, ctx);
    }

    auto CatalogManager::readCatalogRoot(ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        if (bp == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer;
        Status status = bp->pinPage(CATALOG_ROOT_PAGE, &page_buffer, ctx);
        if (status == Status::IO_ERROR)
        {
            // Page doesn't exist yet - catalog not initialized

            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Catalog root page not found");
            return Status::PAGE_CORRUPT;
        }
        if (status != Status::OK)
        {

            return status;
        }

        auto *root = reinterpret_cast<CatalogRootPage *>(page_buffer);

        // Validate catalog root
        if (root->header.page_type != PAGE_TYPE_CATALOG_ROOT)
        {
            bp->unpinPage(CATALOG_ROOT_PAGE, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid catalog root page");
            return Status::PAGE_CORRUPT;
        }

        schema_count_ = root->schema_count;
        table_count_ = root->table_count;
        schemas_table_page_ = root->schemas_page;
        tables_table_page_ = root->tables_page;
        columns_table_page_ = root->columns_page;
        indexes_table_page_ = root->indexes_page;
        constraints_table_page_ = root->constraints_page;
        sequences_table_page_ = root->sequences_page;
        views_table_page_ = root->views_page;
        triggers_table_page_ = root->triggers_page;
        permissions_table_page_ = root->permissions_page;
        statistics_table_page_ = root->statistics_page;
        collations_table_page_ = root->collations_page;
        timezones_table_page_ = root->timezones_page;
        charsets_table_page_ = root->charsets_page;
        collation_defs_table_page_ = root->collation_defs_page;

        return bp->unpinPage(CATALOG_ROOT_PAGE, false, ctx);
    }

    // Helper to write a record to a catalog heap page
    template <typename RecordType>
    auto CatalogManager::writeRecordToHeapPage(uint32_t page_id, const RecordType &record,
                                               ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        Status status = bp->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);

        // Check if we have space
        if (heap->free_offset + sizeof(RecordType) > db_->page_size())
        {
            bp->unpinPage(page_id, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "Catalog heap page full");
            return Status::INVALID_ARGUMENT;
        }

        // Write record
        auto *dest_record = reinterpret_cast<RecordType *>(
            reinterpret_cast<uint8_t *>(page_buffer) + heap->free_offset);
        memcpy(dest_record, &record, sizeof(RecordType));

        heap->record_count++;
        heap->free_offset += sizeof(RecordType);
        heap->header.free_space -= sizeof(RecordType);
        heap->header.generation++;

        return bp->unpinPage(page_id, true, ctx);
    }

    template <typename RecordType, typename InfoType>
    inline auto CatalogManager::readRecordsToVector(
        uint32_t page_id, std::vector<InfoType> &results,
        std::function<bool(const RecordType &)> filter,
        std::function<void(const RecordType &, InfoType &)> converter, ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        Status status = bp->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to read catalog heap page");
            return status;
        }

        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);

        results.clear();
        uint32_t offset = sizeof(CatalogHeapPage);

        for (uint32_t i = 0; i < heap->record_count; i++)
        {
            auto *record =
                reinterpret_cast<RecordType *>(reinterpret_cast<uint8_t *>(page_buffer) + offset);

            if (record->is_valid && filter(*record))
            {
                InfoType info;
                converter(*record, info);
                results.push_back(info);
            }

            offset += sizeof(RecordType);
        }

        return bp->unpinPage(page_id, false, ctx);
    }

    auto CatalogManager::writeSchemaRecord(const SchemaInfo &schema, ErrorContext *ctx) -> Status
    {
        SchemaRecord record;
        memset(&record, 0, sizeof(SchemaRecord)); // Initialize all fields to zero
        record.schema_id = schema.schema_id;
        strncpy(record.schema_name, schema.schema_name.c_str(), 127);
        record.schema_name[127] = '\0';
        strncpy(record.owner, schema.owner.c_str(), 127);
        record.owner[127] = '\0';
        record.default_tablespace_id = schema.default_tablespace_id;
        record.permissions = schema.permissions;
        record.default_charset = schema.default_charset;
        record.default_collation_id = schema.default_collation_id;
        record.acl_oid = schema.acl_oid;
        record.search_path_oid = schema.search_path_oid;
        record.created_time = schema.created_time;
        record.last_modified_time = schema.last_modified_time;
        record.is_valid = 1;

        return writeRecordToHeapPage(schemas_table_page_, record, ctx);
    }

    auto CatalogManager::readSchemaRecords(ErrorContext *ctx) -> Status
    {
        auto converter = [](const SchemaRecord &record, SchemaInfo &info)
        {
            info.schema_id = record.schema_id;
            info.schema_name = record.schema_name;
            info.owner = record.owner;
            info.default_tablespace_id = record.default_tablespace_id;
            info.permissions = record.permissions;
            info.default_charset = record.default_charset;
            info.default_collation_id = record.default_collation_id;
            info.acl_oid = record.acl_oid;
            info.search_path_oid = record.search_path_oid;
            info.created_time = record.created_time;
            info.last_modified_time = record.last_modified_time;
        };
        auto key_extractor = [](const SchemaInfo &info) { return info.schema_id; };
        return readRecordsFromHeapPage<SchemaRecord, SchemaInfo, ID>(
            schemas_table_page_, schema_cache_, converter, key_extractor, ctx);
    }

    auto CatalogManager::writeTableRecord(const TableInfo &table, ErrorContext *ctx) -> Status
    {
        TableRecord record;
        memset(&record, 0, sizeof(TableRecord)); // Initialize all fields to zero
        record.table_id = table.table_id;
        record.schema_id = table.schema_id;
        strncpy(record.table_name, table.table_name.c_str(), 127);
        record.table_name[127] = '\0';
        record.root_page = table.root_page;
        record.column_count = table.column_count;
        record.row_count = table.row_count;
        record.table_type = static_cast<uint8_t>(table.table_type);
        record.has_toast = table.has_toast ? 1 : 0;
        record.tablespace_id = table.tablespace_id;
        record.default_charset = table.default_charset;
        record.default_collation_id = table.default_collation_id;
        record.storage_params_oid = table.storage_params_oid;
        record.created_time = table.created_time;
        record.last_modified_time = table.last_modified_time;
        record.is_valid = 1;

        return writeRecordToHeapPage(tables_table_page_, record, ctx);
    }

    auto CatalogManager::deleteTableRecord(const ID &table_id, ErrorContext *ctx) -> Status
    {
        // Mark the table record as invalid (logical delete) by setting is_valid = 0
        // This is a simple implementation that scans for the table ID and marks it invalid

        BufferPool *bp = db_->buffer_pool();
        void *page_data;
        Status status = bp->pinPage(tables_table_page_, &page_data, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        HeapPage heap_page(static_cast<uint8_t *>(page_data), db_->page_size());

        // Scan through all records to find the matching table_id
        uint16_t item_count = heap_page.getItemCount();
        bool found = false;

        // Access page data directly as mutable (we own it via pinPage)
        auto *mutable_page_data = static_cast<uint8_t *>(page_data);

        for (uint16_t i = 0; i < item_count; ++i)
        {
            // Get tuple location using const API for bounds checking
            const uint8_t *tuple_data;
            uint32_t tuple_size;

            if (heap_page.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
            {
                if (tuple_size >= sizeof(TupleHeader) + sizeof(TableRecord))
                {
                    // Calculate mutable pointer to same location
                    // (getTuple validates bounds, but returns const pointer)
                    const ptrdiff_t offset = tuple_data - static_cast<const uint8_t *>(page_data);
                    uint8_t *mutable_tuple_data = mutable_page_data + offset;

                    // Now we can access the record as mutable (no const_cast needed)
                    auto *record =
                        reinterpret_cast<TableRecord *>(mutable_tuple_data + sizeof(TupleHeader));

                    if (record->table_id == table_id && record->is_valid == 1)
                    {
                        // Found the record - mark it as invalid
                        // Safe: we own the page (pinned with write intent)
                        record->is_valid = 0;
                        found = true;
                        break;
                    }
                }
            }
        }

        // Mark page as dirty if we found and updated the record
        bp->unpinPage(tables_table_page_, found, ctx);

        return found ? Status::OK : Status::NOT_FOUND;
    }

    auto CatalogManager::readTableRecords(ErrorContext *ctx) -> Status
    {
        auto converter = [](const TableRecord &record, TableInfo &info)
        {
            info.table_id = record.table_id;
            info.schema_id = record.schema_id;
            info.table_name = record.table_name;
            info.root_page = record.root_page;
            info.column_count = record.column_count;
            info.row_count = record.row_count;
            info.table_type = static_cast<TableType>(record.table_type);
            info.has_toast = record.has_toast != 0;
            info.tablespace_id = record.tablespace_id;
            info.default_charset = record.default_charset;
            info.default_collation_id = record.default_collation_id;
            info.storage_params_oid = record.storage_params_oid;
            info.created_time = record.created_time;
            info.last_modified_time = record.last_modified_time;
        };
        auto key_extractor = [](const TableInfo &info) { return info.table_id; };
        return readRecordsFromHeapPage<TableRecord, TableInfo, ID>(tables_table_page_, table_cache_,
                                                                   converter, key_extractor, ctx);
    }

    auto CatalogManager::writeColumnRecords(const ID &table_id,
                                            const std::vector<ColumnInfo> &columns,
                                            ErrorContext *ctx) -> Status
    {
        for (const auto &col : columns)
        {
            ColumnRecord record;
            memset(&record, 0, sizeof(ColumnRecord)); // Initialize all fields to zero
            record.table_id = table_id;
            record.column_id = col.column_id;
            strncpy(record.column_name, col.column_name.c_str(), 127);
            record.column_name[127] = '\0';
            record.ordinal = col.ordinal;
            record.data_type = col.data_type;
            record.type_precision = col.type_precision;
            record.type_scale = col.type_scale;
            record.max_length = col.max_length;
            record.nullable = col.nullable ? 1 : 0;
            record.has_default = col.has_default ? 1 : 0;
            record.is_primary_key = col.is_primary_key ? 1 : 0;
            record.is_unique = col.is_unique ? 1 : 0;
            record.is_foreign_key = col.is_foreign_key ? 1 : 0;
            record.is_generated = col.is_generated ? 1 : 0;
            record.storage_type = col.storage_type;
            record.with_timezone = col.with_timezone ? 1 : 0;
            record.charset = col.charset;
            record.timezone_hint = col.timezone_hint;
            record.collation_id = col.collation_id;
            strncpy(record.default_value, col.default_value.c_str(), 127);
            record.default_value[127] = '\0';
            record.default_value_oid = col.default_value_oid;
            record.check_expr_oid = col.check_expr_oid;
            record.created_time = col.created_time;
            record.is_valid = 1;

            Status status = writeRecordToHeapPage(columns_table_page_, record, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
        return Status::OK;
    }

    auto CatalogManager::readColumnRecords(const ID &table_id, ErrorContext *ctx) -> Status
    {
        auto filter = [table_id](const ColumnRecord &record)
        { return record.table_id == table_id; };

        auto converter = [](const ColumnRecord &record, ColumnInfo &info)
        {
            info.table_id = record.table_id;
            info.column_id = record.column_id;
            info.column_name = record.column_name;
            info.ordinal = record.ordinal;
            info.data_type = record.data_type;
            info.type_precision = record.type_precision;
            info.type_scale = record.type_scale;
            info.max_length = record.max_length;
            info.nullable = record.nullable != 0;
            info.has_default = record.has_default != 0;
            info.is_primary_key = record.is_primary_key != 0;
            info.is_unique = record.is_unique != 0;
            info.is_foreign_key = record.is_foreign_key != 0;
            info.is_generated = record.is_generated != 0;
            info.storage_type = record.storage_type;
            info.with_timezone = record.with_timezone != 0;
            info.charset = record.charset;
            info.timezone_hint = record.timezone_hint;
            info.collation_id = record.collation_id;
            info.default_value = record.default_value;
            info.default_value_oid = record.default_value_oid;
            info.check_expr_oid = record.check_expr_oid;
            info.created_time = record.created_time;
        };

        std::vector<ColumnInfo> columns;
        Status status = readRecordsToVector<ColumnRecord, ColumnInfo>(columns_table_page_, columns,
                                                                      filter, converter, ctx);

        if (status == Status::OK && !columns.empty())
        {
            column_cache_[table_id] = columns;
        }

        return status;
    }

    auto CatalogManager::writeIndexRecord(const IndexInfo &index, ErrorContext *ctx) -> Status
    {
        IndexRecord record;
        memset(&record, 0, sizeof(IndexRecord)); // Initialize all fields to zero
        record.index_id = index.index_id;
        record.table_id = index.table_id;
        strncpy(record.index_name, index.index_name.c_str(), 127);
        record.index_name[127] = '\0';
        record.root_page = index.root_page;
        record.index_type = static_cast<uint8_t>(index.index_type);
        record.is_unique = static_cast<uint8_t>(index.is_unique);
        record.column_count = index.column_ids.size();
        for (size_t i = 0; i < index.column_ids.size(); ++i)
        {
            record.column_ids[i] = index.column_ids[i];
        }
        record.index_params_oid = index.index_params_oid;
        record.created_time = index.created_time;
        record.is_valid = 1;

        return writeRecordToHeapPage(indexes_table_page_, record, ctx);
    }

    auto CatalogManager::readIndexRecords(ErrorContext *ctx) -> Status
    {
        auto converter = [](const IndexRecord &record, IndexInfo &info)
        {
            info.index_id = record.index_id;
            info.table_id = record.table_id;
            info.index_name = record.index_name;
            info.root_page = record.root_page;
            info.index_type = static_cast<IndexType>(record.index_type);
            info.is_unique = record.is_unique;
            info.column_ids.assign(record.column_ids, record.column_ids + record.column_count);
            info.index_params_oid = record.index_params_oid;
            info.created_time = record.created_time;
        };
        auto key_extractor = [](const IndexInfo &info) { return info.index_id; };
        return readRecordsFromHeapPage<IndexRecord, IndexInfo, ID>(
            indexes_table_page_, index_cache_, converter, key_extractor, ctx);
    }

    // ===== Timezone Operations =====

    auto CatalogManager::createTimezone(const TimezoneInfo &tz_info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Convert TimezoneInfo to TimezoneRecord
        TimezoneRecord record{};
        memset(&record, 0, sizeof(TimezoneRecord));

        record.timezone_id = tz_info.timezone_id;
        strncpy(record.name, tz_info.name.c_str(), 63);
        record.name[63] = '\0';
        strncpy(record.abbreviation, tz_info.abbreviation.c_str(), 15);
        record.abbreviation[15] = '\0';
        record.std_offset_minutes = tz_info.std_offset_minutes;
        record.observes_dst = tz_info.observes_dst ? 1 : 0;
        record.dst_start_month = tz_info.dst_start_month;
        record.dst_start_week = tz_info.dst_start_week;
        record.dst_start_day = tz_info.dst_start_day;
        record.dst_start_hour = tz_info.dst_start_hour;
        record.dst_end_month = tz_info.dst_end_month;
        record.dst_end_week = tz_info.dst_end_week;
        record.dst_end_day = tz_info.dst_end_day;
        record.dst_end_hour = tz_info.dst_end_hour;
        record.dst_offset_minutes = tz_info.dst_offset_minutes;
        record.created_time = tz_info.created_time;
        record.last_modified_time = tz_info.last_modified_time;
        record.is_valid = 1;

        return writeRecordToHeapPage<TimezoneRecord>(timezones_table_page_, record, ctx);
    }

    auto CatalogManager::updateTimezone(uint16_t timezone_id, const TimezoneInfo &tz_info,
                                        ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "updateTimezone not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getTimezone(uint16_t timezone_id, TimezoneInfo &info, ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto predicate = [timezone_id](const TimezoneRecord &rec)
        { return rec.timezone_id == timezone_id && rec.is_valid; };
        auto result = findRecordInHeapPage<TimezoneRecord>(timezones_table_page_, predicate, ctx);

        if (result.status != Status::OK)
        {
            return result.status;
        }

        // Convert to TimezoneInfo
        const auto &rec = result.record;
        info.timezone_id = rec.timezone_id;
        info.name = rec.name;
        info.abbreviation = rec.abbreviation;
        info.std_offset_minutes = rec.std_offset_minutes;
        info.observes_dst = rec.observes_dst != 0;
        info.dst_start_month = rec.dst_start_month;
        info.dst_start_week = rec.dst_start_week;
        info.dst_start_day = rec.dst_start_day;
        info.dst_start_hour = rec.dst_start_hour;
        info.dst_end_month = rec.dst_end_month;
        info.dst_end_week = rec.dst_end_week;
        info.dst_end_day = rec.dst_end_day;
        info.dst_end_hour = rec.dst_end_hour;
        info.dst_offset_minutes = rec.dst_offset_minutes;
        info.created_time = rec.created_time;
        info.last_modified_time = rec.last_modified_time;

        return Status::OK;
    }

    auto CatalogManager::getTimezoneByName(const std::string &name, TimezoneInfo &info,
                                           ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto predicate = [&name](const TimezoneRecord &rec)
        { return std::string(rec.name) == name && rec.is_valid; };
        auto result = findRecordInHeapPage<TimezoneRecord>(timezones_table_page_, predicate, ctx);

        if (result.status != Status::OK)
        {
            return result.status;
        }

        // Convert to TimezoneInfo
        const auto &rec = result.record;
        info.timezone_id = rec.timezone_id;
        info.name = rec.name;
        info.abbreviation = rec.abbreviation;
        info.std_offset_minutes = rec.std_offset_minutes;
        info.observes_dst = rec.observes_dst != 0;
        info.dst_start_month = rec.dst_start_month;
        info.dst_start_week = rec.dst_start_week;
        info.dst_start_day = rec.dst_start_day;
        info.dst_start_hour = rec.dst_start_hour;
        info.dst_end_month = rec.dst_end_month;
        info.dst_end_week = rec.dst_end_week;
        info.dst_end_day = rec.dst_end_day;
        info.dst_end_hour = rec.dst_end_hour;
        info.dst_offset_minutes = rec.dst_offset_minutes;
        info.created_time = rec.created_time;
        info.last_modified_time = rec.last_modified_time;

        return Status::OK;
    }

    auto CatalogManager::listTimezones(std::vector<TimezoneInfo> &timezones, ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto converter = [](const TimezoneRecord &rec, TimezoneInfo &info)
        {
            info.timezone_id = rec.timezone_id;
            info.name = rec.name;
            info.abbreviation = rec.abbreviation;
            info.std_offset_minutes = rec.std_offset_minutes;
            info.observes_dst = rec.observes_dst != 0;
            info.dst_start_month = rec.dst_start_month;
            info.dst_start_week = rec.dst_start_week;
            info.dst_start_day = rec.dst_start_day;
            info.dst_start_hour = rec.dst_start_hour;
            info.dst_end_month = rec.dst_end_month;
            info.dst_end_week = rec.dst_end_week;
            info.dst_end_day = rec.dst_end_day;
            info.dst_end_hour = rec.dst_end_hour;
            info.dst_offset_minutes = rec.dst_offset_minutes;
            info.created_time = rec.created_time;
            info.last_modified_time = rec.last_modified_time;
        };

        return scanHeapPage<TimezoneRecord, TimezoneInfo>(timezones_table_page_, timezones,
                                                          converter, ctx);
    }

    auto CatalogManager::deleteTimezone(uint16_t timezone_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto predicate = [timezone_id](const TimezoneRecord &rec)
        { return rec.timezone_id == timezone_id && rec.is_valid; };
        auto result = findRecordInHeapPage<TimezoneRecord>(timezones_table_page_, predicate, ctx);

        if (result.status != Status::OK)
        {
            return result.status;
        }

        // Mark as deleted
        TimezoneRecord record = result.record;
        record.is_valid = 0;

        return updateRecordInHeapPage<TimezoneRecord>(timezones_table_page_, result.slot_index,
                                                      record, ctx);
    }

    // ========== Character Set Operations ==========
    // NOTE: These implementations are stubs and need proper helper template functions
    //       (findRecordInHeapPage, updateRecordInHeapPage, scanHeapPage, etc.)
    //       Similar to timezone operations, they are marked for future implementation.

    auto CatalogManager::createCharset(const CharsetInfo &cs_info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        CharsetRecord rec;
        memset(&rec, 0, sizeof(rec));

        rec.charset_id = cs_info.charset_id;
        strncpy(rec.name, cs_info.name.c_str(), sizeof(rec.name) - 1);
        strncpy(rec.description, cs_info.description.c_str(), sizeof(rec.description) - 1);
        rec.min_bytes = cs_info.min_bytes;
        rec.max_bytes = cs_info.max_bytes;
        rec.variable_width = cs_info.variable_width;
        rec.default_collation_id = cs_info.default_collation_id;
        rec.created_time = static_cast<uint64_t>(std::time(nullptr));
        rec.last_modified_time = rec.created_time;
        rec.is_valid = 1;

        return writeRecordToHeapPage(charsets_table_page_, rec, ctx);
    }

    auto CatalogManager::updateCharset(uint16_t charset_id, const CharsetInfo &cs_info,
                                       ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "updateCharset not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getCharset(uint16_t charset_id, CharsetInfo &info, ErrorContext *ctx)
        -> Status
    {
        // TODO: Needs findRecordInHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "getCharset not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getCharsetByName(const std::string &name, CharsetInfo &info,
                                          ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "getCharsetByName not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::listCharsets(std::vector<CharsetInfo> &charsets, ErrorContext *ctx)
        -> Status
    {
        // TODO: Needs scanHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "listCharsets not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::deleteCharset(uint16_t charset_id, ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "deleteCharset not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    // ========== Collation Operations ==========
    // NOTE: These implementations are stubs and need proper helper template functions

    auto CatalogManager::createCollation(const CollationCatalogInfo &col_info, ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        CollationRecord rec;
        memset(&rec, 0, sizeof(rec));

        rec.collation_id = col_info.collation_id;
        strncpy(rec.name, col_info.name.c_str(), sizeof(rec.name) - 1);
        rec.charset_id = col_info.charset_id;
        rec.collation_type = col_info.collation_type;
        rec.strength = col_info.strength;
        rec.pad_space = col_info.pad_space;
        rec.is_default = col_info.is_default;
        strncpy(rec.locale, col_info.locale, sizeof(rec.locale) - 1);
        rec.created_time = static_cast<uint64_t>(std::time(nullptr));
        rec.last_modified_time = rec.created_time;
        rec.is_valid = 1;

        return writeRecordToHeapPage(collation_defs_table_page_, rec, ctx);
    }

    auto CatalogManager::updateCollation(uint32_t collation_id,
                                         const CollationCatalogInfo &col_info, ErrorContext *ctx)
        -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "updateCollation not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getCollation(uint32_t collation_id, CollationCatalogInfo &info,
                                      ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "getCollation not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getCollationByName(const std::string &name, CollationCatalogInfo &info,
                                            ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "getCollationByName not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::listCollations(std::vector<CollationCatalogInfo> &collations,
                                        ErrorContext *ctx) -> Status
    {
        // TODO: Needs scanHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "listCollations not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::listCollationsForCharset(uint16_t charset_id,
                                                  std::vector<CollationCatalogInfo> &collations,
                                                  ErrorContext *ctx) -> Status
    {
        // TODO: Needs scanHeapPageWithFilter helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "listCollationsForCharset not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::deleteCollation(uint32_t collation_id, ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "deleteCollation not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

} // namespace scratchbird::core
