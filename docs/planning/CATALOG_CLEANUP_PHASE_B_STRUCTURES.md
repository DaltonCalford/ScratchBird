# Catalog Cleanup Phase B: Add Missing Structures

**Created:** November 26, 2025
**Priority:** HIGH
**Estimated Effort:** 30-40 hours
**Prerequisites:** None (can run parallel to Phase A)
**Status:** NOT STARTED

---

## Overview

This phase adds new catalog structures required for Alpha Phase 2 features. These structures support Foreign Data Wrappers (FDW), Distributed MVCC, and the UDR plugin system.

---

## Task List

### B-1: Foreign Data Wrapper Structures (10-12 hours)

Add FDW structures for wire protocol integration. Reference: `05-Wire-Protocol-Integration-Specification.md`

**Location:** Add after EmulatedDatabaseInfo in catalog_manager.h (~line 986)

**Structures to Add:**
```cpp
// Foreign Data Wrapper structures (Phase 2 - Wire Protocol Integration)

// Foreign server represents a connection to an external data source
struct ForeignServerInfo
{
    ID server_id;                    // UUID v7
    std::string server_name;         // Unique server name
    std::string server_type;         // "postgresql", "mysql", "mssql", "firebird"
    std::string host;                // Server hostname
    uint16_t port = 0;               // Server port
    std::string connection_options;  // JSON connection parameters (stored in TOAST)
    ID owner_id;                     // Owner UUID
    bool is_active = true;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};

// Foreign table maps to a table on the foreign server
struct ForeignTableInfo
{
    ID foreign_table_id;             // UUID v7
    ID schema_id;                    // Local schema
    std::string table_name;          // Local table name
    ID foreign_server_id;            // References ForeignServerInfo
    std::string remote_schema;       // Remote schema name
    std::string remote_table;        // Remote table name
    std::string column_mapping;      // JSON column mapping (stored in TOAST)
    ID owner_id;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};

// User mapping for authentication to foreign server
struct UserMappingInfo
{
    ID mapping_id;                   // UUID v7
    ID user_id;                      // Local user
    ID foreign_server_id;            // Foreign server
    std::string remote_user;         // Username on remote server
    std::string remote_credentials;  // Encrypted credentials (stored in TOAST)
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

**CRUD Methods to Add:**
```cpp
// Foreign Server operations
auto createForeignServer(const std::string& server_name, const std::string& server_type,
                         const std::string& host, uint16_t port,
                         const std::string& connection_options,
                         ID& server_id_out, ErrorContext* ctx = nullptr) -> Status;
auto getForeignServer(const ID& server_id, ForeignServerInfo& server_out,
                      ErrorContext* ctx = nullptr) -> Status;
auto getForeignServerByName(const std::string& server_name, ForeignServerInfo& server_out,
                            ErrorContext* ctx = nullptr) -> Status;
auto updateForeignServer(const ID& server_id, const std::string& connection_options,
                         bool is_active, ErrorContext* ctx = nullptr) -> Status;
auto dropForeignServer(const ID& server_id, bool cascade, ErrorContext* ctx = nullptr) -> Status;
auto listForeignServers(std::vector<ForeignServerInfo>& servers_out,
                        ErrorContext* ctx = nullptr) -> Status;

// Foreign Table operations
auto createForeignTable(const ID& schema_id, const std::string& table_name,
                        const ID& foreign_server_id, const std::string& remote_schema,
                        const std::string& remote_table, const std::string& column_mapping,
                        ID& table_id_out, ErrorContext* ctx = nullptr) -> Status;
auto getForeignTable(const ID& foreign_table_id, ForeignTableInfo& table_out,
                     ErrorContext* ctx = nullptr) -> Status;
auto dropForeignTable(const ID& foreign_table_id, ErrorContext* ctx = nullptr) -> Status;
auto listForeignTables(const ID& schema_id, std::vector<ForeignTableInfo>& tables_out,
                       ErrorContext* ctx = nullptr) -> Status;

// User Mapping operations
auto createUserMapping(const ID& user_id, const ID& foreign_server_id,
                       const std::string& remote_user, const std::string& remote_credentials,
                       ID& mapping_id_out, ErrorContext* ctx = nullptr) -> Status;
auto getUserMapping(const ID& user_id, const ID& foreign_server_id,
                    UserMappingInfo& mapping_out, ErrorContext* ctx = nullptr) -> Status;
auto dropUserMapping(const ID& mapping_id, ErrorContext* ctx = nullptr) -> Status;
```

---

### B-2: Server Registry Structure (8-10 hours)

Add server registry for distributed MVCC. Reference: `03-Distributed-MVCC-Specification.md`

**Location:** Add after FDW structures

**Structure to Add:**
```cpp
// Server Registry (Phase 2 - Distributed MVCC)
// Tracks all nodes in a distributed database cluster

enum class ServerRole : uint8_t
{
    PRIMARY = 0,      // Primary/master node
    REPLICA = 1,      // Read replica
    STANDBY = 2,      // Hot standby
    WITNESS = 3       // Witness node (for quorum)
};

enum class ServerState : uint8_t
{
    ONLINE = 0,       // Server is online and healthy
    OFFLINE = 1,      // Server is offline
    SYNCING = 2,      // Server is syncing/catching up
    MAINTENANCE = 3,  // Server in maintenance mode
    FAILED = 4        // Server has failed
};

struct ServerRegistryInfo
{
    ID server_id;                    // UUID v7 (unique per server instance)
    std::string server_name;         // Human-readable name
    std::string host;                // Hostname or IP
    uint16_t port = 0;               // Listening port
    ServerRole role = ServerRole::PRIMARY;
    ServerState state = ServerState::ONLINE;
    uint64_t last_heartbeat = 0;     // Timestamp of last heartbeat
    uint64_t last_xid = 0;           // Last transaction ID processed
    uint64_t replication_lag_ms = 0; // Replication lag in milliseconds
    std::string cluster_id;          // Cluster this server belongs to
    std::string server_version;      // ScratchBird version
    std::string metadata;            // JSON metadata (stored in TOAST)
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

**CRUD Methods to Add:**
```cpp
// Server Registry operations (Distributed MVCC)
auto registerServer(const std::string& server_name, const std::string& host,
                    uint16_t port, ServerRole role, const std::string& cluster_id,
                    ID& server_id_out, ErrorContext* ctx = nullptr) -> Status;
auto getServer(const ID& server_id, ServerRegistryInfo& server_out,
               ErrorContext* ctx = nullptr) -> Status;
auto getServerByName(const std::string& server_name, ServerRegistryInfo& server_out,
                     ErrorContext* ctx = nullptr) -> Status;
auto updateServerState(const ID& server_id, ServerState state,
                       ErrorContext* ctx = nullptr) -> Status;
auto updateServerHeartbeat(const ID& server_id, uint64_t last_xid,
                           ErrorContext* ctx = nullptr) -> Status;
auto deregisterServer(const ID& server_id, ErrorContext* ctx = nullptr) -> Status;
auto listServers(const std::string& cluster_id, std::vector<ServerRegistryInfo>& servers_out,
                 ErrorContext* ctx = nullptr) -> Status;
auto listServersWithState(ServerState state, std::vector<ServerRegistryInfo>& servers_out,
                          ErrorContext* ctx = nullptr) -> Status;
auto getPrimaryServer(const std::string& cluster_id, ServerRegistryInfo& server_out,
                      ErrorContext* ctx = nullptr) -> Status;
```

---

### B-3: UDR Module and Engine Structures (8-10 hours)

Add UDR plugin system structures. Reference: `10-UDR-System-Specification.md`

**Location:** Add after ServerRegistryInfo

**Structures to Add:**
```cpp
// UDR Engine information (Phase 2 - UDR Plugin System)
// Represents a language runtime that can execute UDR code

enum class UDREngineType : uint8_t
{
    NATIVE = 0,       // C/C++ native code
    JAVA = 1,         // Java (JVM)
    PYTHON = 2,       // Python
    JAVASCRIPT = 3,   // JavaScript (V8/Node)
    DOTNET = 4,       // .NET (CLR)
    LUA = 5,          // Lua
    WASM = 6          // WebAssembly
};

struct UDREngineInfo
{
    ID engine_id;                    // UUID v7
    std::string engine_name;         // "native", "java", "python", etc.
    UDREngineType engine_type;
    std::string plugin_path;         // Path to engine plugin library
    std::string config;              // JSON configuration (stored in TOAST)
    bool is_active = true;
    bool is_default = false;         // Default engine for its type
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};

// UDR Module information (Phase 2 - UDR Plugin System)
// Represents a loadable module containing UDR implementations

struct UDRModuleInfo
{
    ID module_id;                    // UUID v7
    std::string module_name;         // Module identifier
    ID engine_id;                    // Engine that runs this module
    std::string library_path;        // Path to module library/archive
    std::string checksum;            // SHA-256 checksum for verification
    std::string entry_point;         // Main entry point function
    std::string dependencies;        // JSON list of dependencies
    bool is_loaded = false;          // Currently loaded in memory
    bool is_validated = false;       // Passed security validation
    uint64_t loaded_count = 0;       // Number of times loaded
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

**CRUD Methods to Add:**
```cpp
// UDR Engine operations
auto registerUDREngine(const std::string& engine_name, UDREngineType engine_type,
                       const std::string& plugin_path, const std::string& config,
                       ID& engine_id_out, ErrorContext* ctx = nullptr) -> Status;
auto getUDREngine(const ID& engine_id, UDREngineInfo& engine_out,
                  ErrorContext* ctx = nullptr) -> Status;
auto getUDREngineByName(const std::string& engine_name, UDREngineInfo& engine_out,
                        ErrorContext* ctx = nullptr) -> Status;
auto updateUDREngine(const ID& engine_id, const std::string& config,
                     bool is_active, ErrorContext* ctx = nullptr) -> Status;
auto dropUDREngine(const ID& engine_id, ErrorContext* ctx = nullptr) -> Status;
auto listUDREngines(std::vector<UDREngineInfo>& engines_out,
                    ErrorContext* ctx = nullptr) -> Status;
auto getDefaultUDREngine(UDREngineType type, UDREngineInfo& engine_out,
                         ErrorContext* ctx = nullptr) -> Status;

// UDR Module operations
auto registerUDRModule(const std::string& module_name, const ID& engine_id,
                       const std::string& library_path, const std::string& entry_point,
                       ID& module_id_out, ErrorContext* ctx = nullptr) -> Status;
auto getUDRModule(const ID& module_id, UDRModuleInfo& module_out,
                  ErrorContext* ctx = nullptr) -> Status;
auto getUDRModuleByName(const std::string& module_name, UDRModuleInfo& module_out,
                        ErrorContext* ctx = nullptr) -> Status;
auto validateUDRModule(const ID& module_id, ErrorContext* ctx = nullptr) -> Status;
auto setUDRModuleLoaded(const ID& module_id, bool is_loaded,
                        ErrorContext* ctx = nullptr) -> Status;
auto dropUDRModule(const ID& module_id, ErrorContext* ctx = nullptr) -> Status;
auto listUDRModules(const ID& engine_id, std::vector<UDRModuleInfo>& modules_out,
                    ErrorContext* ctx = nullptr) -> Status;
```

---

### B-4: Extend ObjectType Enum (2-3 hours)

Add missing values to ObjectType enum for new structures.

**Location:** `catalog_manager.h:484-517`

**Values to Add:**
```cpp
enum class ObjectType : uint8_t
{
    // ... existing values ...
    FOREIGN_SERVER = 30,    // Already exists
    FOREIGN_TABLE = 31,     // Already exists
    USER_MAPPING = 32,      // NEW
    SERVER_REGISTRY = 33,   // NEW
    UDR_ENGINE = 34,        // NEW
    UDR_MODULE = 35,        // NEW
    CLUSTER = 36            // NEW (for distributed MVCC)
};
```

---

### B-5: Add Private Member Caches (2-3 hours)

Add cache structures for new catalog objects.

**Location:** Private section of CatalogManager (~line 2246)

**Caches to Add:**
```cpp
// FDW caches
std::unordered_map<ID, ForeignServerInfo> foreign_server_cache_;
std::unordered_map<std::string, ID> foreign_server_name_to_id_;
std::unordered_map<ID, ForeignTableInfo> foreign_table_cache_;
std::unordered_map<ID, UserMappingInfo> user_mapping_cache_;

// Server Registry cache
std::unordered_map<ID, ServerRegistryInfo> server_registry_cache_;
std::unordered_map<std::string, ID> server_name_to_id_;

// UDR Engine/Module caches
std::unordered_map<ID, UDREngineInfo> udr_engine_cache_;
std::unordered_map<std::string, ID> udr_engine_name_to_id_;
std::unordered_map<ID, UDRModuleInfo> udr_module_cache_;
std::unordered_map<std::string, ID> udr_module_name_to_id_;

// Mutexes for new caches
mutable std::mutex foreign_server_mutex_;
mutable std::mutex server_registry_mutex_;
mutable std::mutex udr_engine_mutex_;
mutable std::mutex udr_module_mutex_;
```

---

### B-6: Add System Table Page Variables (1-2 hours)

Add page ID variables for new system tables.

**Location:** Private section (~line 2300)

**Variables to Add:**
```cpp
// FDW system table pages
uint32_t foreign_servers_table_page_ = 0;
uint32_t foreign_tables_table_page_ = 0;
uint32_t user_mappings_table_page_ = 0;

// Server Registry page
uint32_t server_registry_table_page_ = 0;

// UDR system table pages
uint32_t udr_engines_table_page_ = 0;
uint32_t udr_modules_table_page_ = 0;
```

---

## Alignment with Phase 2 Specifications

| Structure | Spec Document | Section |
|-----------|---------------|---------|
| ForeignServerInfo | 05-Wire-Protocol-Integration | Foreign Data Access |
| ForeignTableInfo | 05-Wire-Protocol-Integration | Foreign Tables |
| UserMappingInfo | 05-Wire-Protocol-Integration | Authentication |
| ServerRegistryInfo | 03-Distributed-MVCC | Node Management |
| UDREngineInfo | 10-UDR-System | Engine Plugin |
| UDRModuleInfo | 10-UDR-System | Module Loading |

---

## Checklist

### Implementation
- [ ] B-1: FDW structures (ForeignServerInfo, ForeignTableInfo, UserMappingInfo)
- [ ] B-2: ServerRegistryInfo structure
- [ ] B-3: UDR structures (UDREngineInfo, UDRModuleInfo)
- [ ] B-4: ObjectType enum extensions
- [ ] B-5: Private member caches
- [ ] B-6: System table page variables

### Testing
- [ ] Compile with new structures
- [ ] Verify no breaking changes

### Documentation
- [ ] Update catalog_manager.h header comments

---

## Effort Summary

| Task | Est. Hours |
|------|-----------|
| B-1: FDW Structures | 10-12 |
| B-2: Server Registry | 8-10 |
| B-3: UDR Engine/Module | 8-10 |
| B-4: ObjectType Enum | 2-3 |
| B-5: Private Caches | 2-3 |
| B-6: Page Variables | 1-2 |
| **Total** | **31-40 hours** |

---

**Document Version:** 1.0
**Last Updated:** November 26, 2025
