#ifndef SCRATCHBIRD_ENGINE_CATALOG_MANAGER_H
#define SCRATCHBIRD_ENGINE_CATALOG_MANAGER_H

#include "scratchbird/engine/system_oids.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    struct CatalogVersion {
        std::uint16_t major{0};
        std::uint16_t minor{0};
    };

    // Minimal Phase 4 scaffolding: header-based bootstrap detection and reporting
    class CatalogManager
    {
      public:
        // path is database base path (without .seg0); e.g., /path/to/db
        explicit CatalogManager(std::string db_path);

        // Returns true if header reports a catalog version
        bool is_bootstrapped() const;

        // Reads version from header clumplet
        CatalogVersion current_version() const;

        // Idempotent: if not bootstrapped, emit bootstrap SQL sidecar with fixed UUIDs already
        // handled by create_database, else no-op.
        void bootstrap_if_needed() const;

        // --- Minimal CRUD for Phase 4 ---
        // Create a schema with provided UUID and attributes. Returns true on success.
        bool create_schema(const UuidBytes& oid, const std::string& name,
                           const std::optional<UuidBytes>& parent, const std::string& kind) const;

        // Lookup schema OID by name; returns empty if not found
        std::optional<UuidBytes> lookup_schema_oid_by_name(const std::string& name) const;

        // List schemas (oid, name)
        std::vector<std::pair<UuidBytes, std::string>> list_schemas() const;

        // --- SDB$OBJECT minimal CRUD ---
        // Create object row (oid, type, schema_oid, name)
        bool create_object(const UuidBytes& oid, const std::string& type,
                           const std::optional<UuidBytes>& schema_oid,
                           const std::string& name) const;

        // Lookup object OID by (schema_oid, type, name)
        std::optional<UuidBytes> lookup_object_oid(const std::optional<UuidBytes>& schema_oid,
                                                   const std::string& type,
                                                   const std::string& name) const;

        // List objects in a schema (returns (oid, type, name))
        std::vector<std::tuple<UuidBytes, std::string, std::string>>
        list_objects_in_schema(const std::optional<UuidBytes>& schema_oid) const;

        // Create relation and its columns; returns relation OID
        UuidBytes create_relation(const UuidBytes& schema_oid, const std::string& name,
                                  const std::vector<std::string>& column_names) const;

        // List relations in schema (oid, name)
        std::vector<std::pair<UuidBytes, std::string>>
        list_relations(const std::optional<UuidBytes>& schema_oid) const;

        // List columns for a relation (position, name)
        std::vector<std::pair<std::int64_t, std::string>>
        list_columns(const UuidBytes& relation_oid) const;

        // Create SDB$COLUMN rows for a relation with NOT NULL flags and defaults
        bool create_columns(
            const UuidBytes& relation_oid,
            const std::vector<std::pair<std::int64_t, std::string>>& columns,
            const std::vector<std::string>& not_null_columns,
            const std::unordered_map<std::string, std::string>& column_defaults = {}) const;

        // Phase 5 helpers: resolve relation heap root page and list columns by name
        std::optional<std::uint32_t> get_relation_root_page(const UuidBytes& relation_oid) const;
        std::optional<std::uint32_t>
        get_relation_root_page_by_name(const std::optional<UuidBytes>& schema_oid,
                                       const std::string& relation_name) const;
        std::vector<std::string>
        list_column_names_by_name(const std::optional<UuidBytes>& schema_oid,
                                  const std::string& relation_name) const;

        struct IndexCatalogInfo {
            std::string name;
            std::string method;
            bool unique{false};
            std::vector<std::pair<std::string, std::string>> keys; // (column_or_expr, direction)
            std::vector<std::string> exprs;                        // expression texts, if any
        };
        // List indexes defined on a given relation
        std::vector<IndexCatalogInfo>
        list_relation_indexes_by_name(const std::optional<UuidBytes>& schema_oid,
                                      const std::string& relation_name) const;

        // Index helpers: persist/retrieve index root page via SDB$STATS JSON {"root":N}
        bool set_index_root(const std::optional<UuidBytes>& schema_oid,
                            const std::string& index_name, std::uint32_t root_page);
        std::optional<std::uint32_t> get_index_root(const std::optional<UuidBytes>& schema_oid,
                                                    const std::string& index_name) const;

        // Constraints
        struct ConstraintInfo {
            std::string name;
            std::string type; // CHECK|NOT_NULL|UNIQUE|PRIMARY_KEY|FOREIGN_KEY
            bool deferrable{false};
            bool initially_deferred{false};
            std::string check_expr;               // for CHECK
            std::string on_delete;                // for FK
            std::string on_update;                // for FK
            std::vector<std::string> columns;     // local columns
            std::vector<std::string> ref_columns; // referenced columns for FK
            std::string ref_relation;             // referenced relation name
        };
        std::vector<ConstraintInfo>
        list_relation_constraints_by_name(const std::optional<UuidBytes>& schema_oid,
                                          const std::string& relation_name) const;

        // Create a constraint catalog entry and its key rows
        bool create_constraint_catalog(const std::optional<UuidBytes>& schema_oid,
                                       const UuidBytes& relation_oid, const std::string& name,
                                       const std::string& type, bool deferrable,
                                       bool initially_deferred, const std::string& check_expr,
                                       const std::vector<std::string>& columns,
                                       const std::vector<std::string>& ref_columns,
                                       const std::string& ref_relation,
                                       const std::string& on_update,
                                       const std::string& on_delete) const;

        // Drop a named constraint on a relation
        bool drop_constraint_by_name(const std::optional<UuidBytes>& schema_oid,
                                     const std::string& relation_name,
                                     const std::string& constraint_name) const;

        // Alter constraint deferrability flags by name
        bool alter_constraint_deferral(const std::optional<UuidBytes>& schema_oid,
                                       const std::string& relation_name,
                                       const std::string& constraint_name, bool deferrable,
                                       bool initially_deferred) const;

        // Triggers (basic listing)
        struct TriggerInfo {
            UuidBytes oid{};
            std::string name;
            std::string timing; // BEFORE|AFTER
            std::string events; // textual list
            int position{0};
            std::string for_each; // ROW|STATEMENT
            bool active{true};
            std::vector<std::string> update_of_cols; // only for UPDATE triggers
        };
        std::vector<TriggerInfo>
        list_relation_triggers_by_name(const std::optional<UuidBytes>& schema_oid,
                                       const std::string& relation_name) const;

        // Create trigger catalog rows (OBJECT, TRIGGER, SOURCE)
        bool create_trigger_catalog(const std::optional<UuidBytes>& schema_oid,
                                    const UuidBytes& relation_oid, const std::string& name,
                                    const std::string& timing, const std::string& events_json,
                                    int position, const std::string& for_each, bool active,
                                    const std::string& update_of_json,
                                    const std::string& body_source) const;

        bool drop_trigger_by_name(const std::optional<UuidBytes>& schema_oid,
                                  const std::string& relation_name,
                                  const std::string& trigger_name) const;

        // ALTER TRIGGER name ACTIVE|INACTIVE (toggle)
        bool alter_trigger_active(const std::optional<UuidBytes>& schema_oid,
                                  const std::string& trigger_name, bool active) const;

        // Fetch source text for an object from SDB$SOURCE (empty if not found)
        std::string get_source_for_object(const UuidBytes& object_oid) const;

        // Inbound FK discovery for parent-side enforcement
        struct ForeignKeyInfo {
            UuidBytes child_relation_oid{};
            std::string child_relation_name;
            std::string constraint_name;
            std::vector<std::string> child_columns;
            std::vector<std::string> parent_columns; // ref_columns
            std::string on_delete;
            std::string on_update;
            bool deferrable{false};
            bool initially_deferred{false};
        };
        std::vector<ForeignKeyInfo>
        list_inbound_foreign_keys_by_parent(const UuidBytes& parent_relation_oid) const;
        std::vector<ForeignKeyInfo>
        list_inbound_foreign_keys_by_name(const std::optional<UuidBytes>& schema_oid,
                                          const std::string& parent_relation_name) const;

        // Column defaults (effective: column default overrides domain default)
        std::unordered_map<std::string, std::string>
        get_effective_column_defaults_by_name(const std::optional<UuidBytes>& schema_oid,
                                              const std::string& relation_name) const;

        // --- SDB$DOMAIN ---
        struct DomainSpec {
            std::string oid_hex;   // 32-hex or empty to auto-generate
            std::string base_type; // e.g., VARCHAR(128) raw simplified
            std::int64_t length{0};
            std::int64_t precision{0};
            std::int64_t scale{0};
            std::string charset; // optional
            std::string collate; // optional
            bool not_null{false};
            std::string default_expr; // optional
            std::string check_expr;   // optional
        };

        bool create_domain(const DomainSpec& spec) const;
        std::vector<std::tuple<std::string, std::string, std::int64_t, std::int64_t, std::int64_t>>
        list_domains() const; // name, base_type, length, precision, scale

        // --- VIEWS ---
        bool create_view(const UuidBytes& schema_oid, const std::string& name,
                         const std::string& definition_sql) const;
        std::vector<std::pair<UuidBytes, std::string>>
        list_views(const std::optional<UuidBytes>& schema_oid) const;

        // Return view definition text or empty if not found
        std::string get_view_definition(const std::optional<UuidBytes>& schema_oid,
                                        const std::string& name) const;

        // --- Index catalog stubs (no physical index build in Phase 4) ---
        bool create_index_catalog(const std::optional<UuidBytes>& schema_oid,
                                  const UuidBytes& relation_oid, const std::string& name,
                                  const std::string& method,
                                  const std::vector<std::pair<std::string, std::string>>& keys,
                                  bool unique) const; // keys: (column_or_expr, direction)
        std::vector<std::tuple<std::string, std::string, bool>>
        list_indexes(const std::optional<UuidBytes>& schema_oid) const; // name, method, unique

        // DROP INDEX support
        bool drop_index_by_name(const std::optional<UuidBytes>& schema_oid,
                                const std::string& index_name) const;

        // Stats catalog helpers
        bool set_stats(const UuidBytes& object_oid, const std::string& json);
        std::optional<std::string> get_stats(const UuidBytes& object_oid) const;

        // ALTER TABLE column operations
        bool add_column(const std::optional<UuidBytes>& schema_oid,
                        const std::string& relation_name,
                        const std::string& column_definition) const;
        bool drop_column(const std::optional<UuidBytes>& schema_oid,
                         const std::string& relation_name, const std::string& column_name) const;
        bool alter_column_type(const std::optional<UuidBytes>& schema_oid,
                               const std::string& relation_name, const std::string& column_name,
                               const std::string& new_type) const;
        bool alter_column_default(const std::optional<UuidBytes>& schema_oid,
                                  const std::string& relation_name, const std::string& column_name,
                                  const std::string& default_value) const;
        bool alter_column_not_null(const std::optional<UuidBytes>& schema_oid,
                                   const std::string& relation_name, const std::string& column_name,
                                   bool not_null) const;
        bool rename_column(const std::optional<UuidBytes>& schema_oid,
                           const std::string& relation_name, const std::string& old_name,
                           const std::string& new_name) const;

        // Stored procedures and functions
        struct RoutineInfo {
            UuidBytes oid{};
            std::string name;
            std::string kind;       // PROCEDURE or FUNCTION
            std::string language;   // PSQL
            std::string security;   // INVOKER or DEFINER
            std::string volatility; // VOLATILE, STABLE, IMMUTABLE
            bool leakproof{false};
            bool returns_set{false};
            std::string source_code; // from SDB$SOURCE
        };

        struct RoutineParamInfo {
            std::string name;
            std::string mode;      // IN, OUT, INOUT
            std::string type_json; // serialized type info
            int position{0};
        };

        // Create stored procedure/function in catalog
        bool create_routine(const std::optional<UuidBytes>& schema_oid, const std::string& name,
                            const std::string& kind, const std::string& language,
                            const std::string& security, const std::string& volatility,
                            bool leakproof, bool returns_set,
                            const std::vector<RoutineParamInfo>& params,
                            const std::string& source_code) const;

        // List routines in a schema
        std::vector<RoutineInfo> list_routines(const std::optional<UuidBytes>& schema_oid) const;

        // Get routine by name
        std::optional<RoutineInfo> get_routine_by_name(const std::optional<UuidBytes>& schema_oid,
                                                       const std::string& name) const;

        // Get routine parameters
        std::vector<RoutineParamInfo> get_routine_params(const UuidBytes& routine_oid) const;

        // Drop routine by name
        bool drop_routine_by_name(const std::optional<UuidBytes>& schema_oid,
                                  const std::string& name) const;

        // Package management
        struct PackageInfo {
            std::string name;
            std::string schema_name;
            std::string header_source;
            std::string body_source;
            bool has_header{false};
            bool has_body{false};
        };

        // Create package header
        bool create_package_header(const std::optional<UuidBytes>& schema_oid,
                                   const std::string& name, const std::string& header_source) const;

        // Create package body
        bool create_package_body(const std::optional<UuidBytes>& schema_oid,
                                 const std::string& name, const std::string& body_source) const;

        // Get package by name
        std::optional<PackageInfo> get_package_by_name(const std::optional<UuidBytes>& schema_oid,
                                                       const std::string& name) const;

        // List packages in a schema
        std::vector<PackageInfo> list_packages(const std::optional<UuidBytes>& schema_oid) const;

        // Drop package by name
        bool drop_package_by_name(const std::optional<UuidBytes>& schema_oid,
                                  const std::string& name) const;

      private:
        std::string db_path_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_CATALOG_MANAGER_H
