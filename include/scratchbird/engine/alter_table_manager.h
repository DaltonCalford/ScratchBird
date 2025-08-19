#ifndef SCRATCHBIRD_ENGINE_ALTER_TABLE_MANAGER_H
#define SCRATCHBIRD_ENGINE_ALTER_TABLE_MANAGER_H

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/system_oids.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Comprehensive ALTER TABLE operation types
    enum class AlterTableOperation {
        // Column operations
        ADD_COLUMN,
        DROP_COLUMN,
        RENAME_COLUMN,
        ALTER_COLUMN_TYPE,
        ALTER_COLUMN_SET_DEFAULT,
        ALTER_COLUMN_DROP_DEFAULT,
        ALTER_COLUMN_SET_NOT_NULL,
        ALTER_COLUMN_DROP_NOT_NULL,

        // Constraint operations
        ADD_CONSTRAINT,
        DROP_CONSTRAINT,
        RENAME_CONSTRAINT,
        ALTER_CONSTRAINT,

        // Table operations
        RENAME_TABLE,
        SET_TABLESPACE,

        // Advanced operations
        ADD_GENERATED_COLUMN,
        ALTER_COLUMN_GENERATED,
        ENABLE_TRIGGER,
        DISABLE_TRIGGER,
        CLUSTER_ON,
        SET_WITHOUT_CLUSTER
    };

    // Column definition for ADD COLUMN
    struct ColumnDefinition {
        std::string name;
        std::string data_type;
        bool not_null{false};
        std::string default_value;
        bool has_default{false};
        std::string check_constraint;
        bool is_generated{false};
        std::string generated_expression;
        std::string collation;
        std::string comment;

        // Advanced properties
        bool is_identity{false};
        std::string identity_type; // ALWAYS or BY DEFAULT
        std::int64_t identity_start{1};
        std::int64_t identity_increment{1};

        // Validation
        bool is_valid() const;
        std::string to_sql() const;
    };

    // Constraint definition for ADD CONSTRAINT
    struct ConstraintDefinition {
        std::string name;
        std::string type; // PRIMARY KEY, UNIQUE, CHECK, FOREIGN KEY
        std::vector<std::string> columns;
        std::string check_expression;

        // Foreign key specific
        std::string referenced_table;
        std::vector<std::string> referenced_columns;
        std::string on_delete_action; // CASCADE, SET NULL, RESTRICT, etc.
        std::string on_update_action;

        // Constraint properties
        bool deferrable{false};
        std::string initially; // IMMEDIATE or DEFERRED

        // Validation
        bool is_valid() const;
        std::string to_sql() const;
    };

    // ALTER TABLE operation specification
    struct AlterTableSpec {
        AlterTableOperation operation;
        std::string schema_name;
        std::string table_name;

        // Operation-specific data
        std::string column_name;
        std::string new_name; // For rename operations
        std::string constraint_name;

        ColumnDefinition column_def;
        ConstraintDefinition constraint_def;

        // Additional properties
        std::string new_data_type;
        std::string new_default_value;
        bool new_not_null{false};

        // Validation and utility
        bool is_valid() const;
        std::string to_sql() const;
    };

    // ALTER TABLE execution engine
    class AlterTableManager
    {
      public:
        AlterTableManager(const std::string& database_path);
        ~AlterTableManager();

        // Main execution interface
        bool execute_alter_table(const AlterTableSpec& spec);
        bool execute_alter_table_batch(const std::vector<AlterTableSpec>& specs);

        // Validation and planning
        bool validate_alter_table(const AlterTableSpec& spec) const;
        std::vector<std::string> get_execution_plan(const AlterTableSpec& spec) const;

        // Column operations
        bool add_column(const std::string& schema, const std::string& table,
                        const ColumnDefinition& column_def);
        bool drop_column(const std::string& schema, const std::string& table,
                         const std::string& column_name);
        bool rename_column(const std::string& schema, const std::string& table,
                           const std::string& old_name, const std::string& new_name);
        bool alter_column_type(const std::string& schema, const std::string& table,
                               const std::string& column_name, const std::string& new_type);
        bool alter_column_default(const std::string& schema, const std::string& table,
                                  const std::string& column_name, const std::string& default_value,
                                  bool set_default);
        bool alter_column_null(const std::string& schema, const std::string& table,
                               const std::string& column_name, bool set_not_null);

        // Constraint operations
        bool add_constraint(const std::string& schema, const std::string& table,
                            const ConstraintDefinition& constraint_def);
        bool drop_constraint(const std::string& schema, const std::string& table,
                             const std::string& constraint_name);
        bool rename_constraint(const std::string& schema, const std::string& table,
                               const std::string& old_name, const std::string& new_name);

        // Table operations
        bool rename_table(const std::string& schema, const std::string& old_name,
                          const std::string& new_name);

        // Advanced operations
        bool add_generated_column(const std::string& schema, const std::string& table,
                                  const std::string& column_name, const std::string& expression);
        bool enable_disable_trigger(const std::string& schema, const std::string& table,
                                    const std::string& trigger_name, bool enable);

        // Utility and maintenance
        bool rebuild_table_structure(const std::string& schema, const std::string& table);
        bool analyze_alter_impact(const AlterTableSpec& spec) const;

      private:
        std::string database_path_;
        std::unique_ptr<CatalogManager> catalog_;

        // Internal execution helpers
        bool execute_add_column_internal(const std::string& schema, const std::string& table,
                                         const ColumnDefinition& column_def);
        bool execute_drop_column_internal(const std::string& schema, const std::string& table,
                                          const std::string& column_name);
        bool execute_rename_column_internal(const std::string& schema, const std::string& table,
                                            const std::string& old_name,
                                            const std::string& new_name);
        bool execute_alter_column_type_internal(const std::string& schema, const std::string& table,
                                                const std::string& column_name,
                                                const std::string& new_type);

        // Data migration helpers
        bool migrate_column_data(const std::string& schema, const std::string& table,
                                 const std::string& column_name, const std::string& old_type,
                                 const std::string& new_type);
        bool populate_new_column(const std::string& schema, const std::string& table,
                                 const std::string& column_name, const std::string& default_value);
        bool remove_column_data(const std::string& schema, const std::string& table,
                                const std::string& column_name);

        // Validation helpers
        bool column_exists(const std::string& schema, const std::string& table,
                           const std::string& column_name) const;
        bool constraint_exists(const std::string& schema, const std::string& table,
                               const std::string& constraint_name) const;
        bool table_exists(const std::string& schema, const std::string& table) const;
        bool is_valid_data_type(const std::string& data_type) const;
        bool is_compatible_type_conversion(const std::string& from_type,
                                           const std::string& to_type) const;

        // Dependency checking
        std::vector<std::string> get_column_dependencies(const std::string& schema,
                                                         const std::string& table,
                                                         const std::string& column_name) const;
        std::vector<std::string>
        get_constraint_dependencies(const std::string& schema, const std::string& table,
                                    const std::string& constraint_name) const;

        // Catalog update helpers
        bool update_column_metadata(const std::string& schema, const std::string& table,
                                    const std::string& column_name,
                                    const ColumnDefinition& new_def);
        bool update_constraint_metadata(const std::string& schema, const std::string& table,
                                        const std::string& constraint_name,
                                        const ConstraintDefinition& new_def);
        bool update_table_metadata(const std::string& schema, const std::string& old_name,
                                   const std::string& new_name);
    };

    // High-level ALTER TABLE utilities

    // Parse ALTER TABLE SQL into AlterTableSpec
    AlterTableSpec parse_alter_table_sql(const std::string& sql);

    // Generate ALTER TABLE SQL from spec
    std::string generate_alter_table_sql(const AlterTableSpec& spec);

    // Batch ALTER TABLE operations
    std::vector<AlterTableSpec>
    optimize_alter_table_batch(const std::vector<AlterTableSpec>& specs);

    // Schema evolution utilities
    bool apply_schema_migration(const std::string& database_path,
                                const std::vector<AlterTableSpec>& migration_specs);

    // Generate migration scripts
    std::vector<std::string> generate_migration_script(const std::string& from_schema,
                                                       const std::string& to_schema);

    // Validate schema compatibility
    bool validate_schema_migration(const std::vector<AlterTableSpec>& migration_specs);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_ALTER_TABLE_MANAGER_H
