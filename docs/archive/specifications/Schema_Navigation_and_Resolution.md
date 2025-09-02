# Schema Navigation and Resolution Specification

## Overview

ScratchBird implements a hierarchical schema system with filesystem-like navigation, search paths, and symbolic links (synonyms).

## Schema Path Resolution Algorithm

```cpp
namespace scratchbird::schema {

class SchemaResolver {
private:
    struct SchemaNode {
        UUID schema_id;
        string name;
        SchemaNode* parent;
        map<string, unique_ptr<SchemaNode>> children;
        SchemaType type;
        bool is_virtual;  // For remote schemas
        
        string get_full_path() const {
            if (!parent) return "[root]";
            return parent->get_full_path() + "." + "[" + name + "]";
        }
    };
    
    SchemaNode* root;
    SchemaNode* current_schema;
    vector<string> search_path;
    
public:
    // Resolve a schema reference to actual schema
    SchemaNode* resolve_schema(const string& reference) {
        if (is_absolute_path(reference)) {
            // Absolute path like [root].[sys].[tables]
            return resolve_absolute(reference);
        }
        else if (is_relative_path(reference)) {
            // Relative path like ..\..\common
            return resolve_relative(reference);
        }
        else {
            // Simple name - use search path
            return resolve_with_search_path(reference);
        }
    }
    
    SchemaNode* resolve_absolute(const string& path) {
        // Parse [root].[app].[crm] format
        vector<string> components = parse_path(path);
        
        SchemaNode* node = root;
        for (const auto& component : components) {
            if (component == "root") continue;
            
            auto it = node->children.find(component);
            if (it == node->children.end()) {
                return nullptr;  // Path not found
            }
            node = it->second.get();
        }
        
        return node;
    }
    
    SchemaNode* resolve_relative(const string& path) {
        SchemaNode* node = current_schema;
        
        // Process .. components
        size_t pos = 0;
        while (path.substr(pos, 2) == "..") {
            if (!node->parent) {
                throw schema_error("Cannot navigate above root");
            }
            node = node->parent;
            pos += 3;  // Skip ../
        }
        
        // Process remaining path
        if (pos < path.length()) {
            string remaining = path.substr(pos);
            return resolve_from_node(node, remaining);
        }
        
        return node;
    }
    
    SchemaNode* resolve_with_search_path(const string& name) {
        // Try each schema in search path
        for (const auto& search_schema : search_path) {
            SchemaNode* base = nullptr;
            
            if (search_schema == "[current]") {
                base = current_schema;
            } else {
                base = resolve_absolute(search_schema);
            }
            
            if (base) {
                // Look for object in this schema
                auto it = base->children.find(name);
                if (it != base->children.end()) {
                    return it->second.get();
                }
            }
        }
        
        return nullptr;  // Not found in any search path
    }
};

} // namespace scratchbird::schema
```

## Object Resolution with Schema Context

```cpp
class ObjectResolver {
private:
    SchemaResolver schema_resolver;
    SynonymManager synonym_manager;
    
public:
    // Resolve a table reference
    TableDescriptor* resolve_table(const string& table_ref) {
        // Parse schema.table or just table
        auto [schema_part, table_part] = split_qualified_name(table_ref);
        
        SchemaNode* schema = nullptr;
        if (!schema_part.empty()) {
            // Explicit schema reference
            schema = schema_resolver.resolve_schema(schema_part);
        } else {
            // Search for table in search path
            return find_table_in_search_path(table_part);
        }
        
        if (!schema) {
            throw not_found_error("Schema not found: " + schema_part);
        }
        
        // Check if it's a synonym
        if (synonym_manager.is_synonym(schema->get_full_path(), table_part)) {
            return resolve_synonym(schema->get_full_path(), table_part);
        }
        
        // Look up actual table
        return catalog.get_table(schema->schema_id, table_part);
    }
    
    TableDescriptor* find_table_in_search_path(const string& table_name) {
        for (const auto& search_schema : schema_resolver.get_search_path()) {
            SchemaNode* schema = schema_resolver.resolve_schema(search_schema);
            if (schema) {
                // Check for synonym first
                string full_path = schema->get_full_path();
                if (synonym_manager.is_synonym(full_path, table_name)) {
                    return resolve_synonym(full_path, table_name);
                }
                
                // Check for actual table
                TableDescriptor* table = catalog.get_table_if_exists(
                    schema->schema_id, table_name
                );
                if (table) return table;
            }
        }
        
        throw not_found_error("Table not found: " + table_name);
    }
    
    TableDescriptor* resolve_synonym(const string& schema_path, const string& synonym_name) {
        SynonymDef synonym = synonym_manager.get_synonym(schema_path, synonym_name);
        
        if (synonym.is_remote) {
            // Remote synonym - may need to establish connection
            return remote_manager.get_remote_table(synonym.target_path);
        } else {
            // Local synonym - recursive resolution
            return resolve_table(synonym.target_path);
        }
    }
};
```

## Schema Creation and Management

```sql
-- Creating hierarchical schemas
CREATE SCHEMA [root].[app].[new_app];
CREATE SCHEMA [root].[app].[new_app].[module1];
CREATE SCHEMA [root].[app].[new_app].[module2];

-- Setting current schema
SET SCHEMA = '[root].[app].[new_app]';
SET search_path = '[current], [root].[app].[common], [root].[sys]';

-- Creating objects in specific schemas
CREATE TABLE [root].[app].[new_app].[module1].customers (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100)
);

-- Or using current schema
SET SCHEMA = '[root].[app].[new_app].[module1]';
CREATE TABLE customers (  -- Creates in current schema
    id INTEGER PRIMARY KEY,
    name VARCHAR(100)
);
```

## Synonym Management

```cpp
class SynonymManager {
private:
    struct SynonymDef {
        UUID synonym_id;
        string schema_path;
        string synonym_name;
        string target_path;
        bool is_remote;
        bool is_recursive;  // Can point to another synonym
        int recursion_depth;  // Prevent infinite loops
    };
    
    // Schema path -> synonym name -> definition
    map<string, map<string, SynonymDef>> synonyms;
    
public:
    void create_synonym(
        const string& schema_path,
        const string& synonym_name,
        const string& target_path
    ) {
        // Validate target exists (unless it's remote)
        if (!is_remote_path(target_path)) {
            if (!object_exists(target_path)) {
                throw not_found_error("Target not found: " + target_path);
            }
        }
        
        // Check for circular references
        if (would_create_cycle(schema_path, synonym_name, target_path)) {
            throw schema_error("Synonym would create circular reference");
        }
        
        SynonymDef def{
            .synonym_id = generate_uuid(),
            .schema_path = schema_path,
            .synonym_name = synonym_name,
            .target_path = target_path,
            .is_remote = is_remote_path(target_path),
            .is_recursive = is_synonym(target_path)
        };
        
        synonyms[schema_path][synonym_name] = def;
        
        // Persist to system catalog
        catalog.store_synonym(def);
    }
    
    string resolve_synonym_chain(const string& schema_path, const string& synonym_name) {
        set<string> visited;  // Detect cycles
        string current_path = schema_path + "." + synonym_name;
        
        while (is_synonym_path(current_path)) {
            if (visited.count(current_path)) {
                throw schema_error("Circular synonym reference detected");
            }
            visited.insert(current_path);
            
            auto [schema, name] = split_path(current_path);
            SynonymDef def = synonyms[schema][name];
            current_path = def.target_path;
        }
        
        return current_path;
    }
};
```

## User Home Schemas

```cpp
class UserSchemaManager {
public:
    void create_user_with_home_schema(const string& username, const UserOptions& options) {
        // Create user
        UUID user_id = security.create_user(username, options);
        
        // Create home schema
        string home_path = "[root].[users].[" + username + "]";
        schema_manager.create_schema(home_path, SchemaType::USER);
        
        // Grant full permissions to user on their home schema
        security.grant_all_on_schema(home_path, user_id);
        
        // Set as default schema for user
        user_manager.set_default_schema(user_id, home_path);
        
        // Set default search path
        vector<string> default_path = {
            "[current]",
            home_path,
            "[root].[app].[common]",
            "[root].[sys]"
        };
        user_manager.set_search_path(user_id, default_path);
        
        // Create standard objects in home schema
        create_user_defaults(home_path);
    }
    
private:
    void create_user_defaults(const string& home_path) {
        // Create default tables/views in user's home
        execute_sql(format(R"(
            CREATE TABLE {}.preferences (
                key VARCHAR(255) PRIMARY KEY,
                value JSONB,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            
            CREATE VIEW {}.my_objects AS
            SELECT * FROM [root].[sys].all_objects
            WHERE owner_id = CURRENT_USER_ID();
        )", home_path, home_path));
    }
};
```

## Remote Schema Integration

```cpp
class RemoteSchemaManager {
private:
    struct RemoteServer {
        UUID server_id;
        string alias;
        DatabaseType type;
        ConnectionPool* pool;
        string local_schema_root;  // e.g., [root].[remote].[postgresql].[alias]
    };
    
    map<string, RemoteServer> servers;
    
public:
    void mount_remote_database(
        const string& server_alias,
        const string& remote_database,
        const string& local_mount_point
    ) {
        RemoteServer& server = servers[server_alias];
        
        // Create virtual schema tree
        string mount_path = local_mount_point.empty() 
            ? server.local_schema_root + ".[" + remote_database + "]"
            : local_mount_point;
            
        schema_manager.create_virtual_schema(mount_path);
        
        // Fetch remote schema structure
        auto remote_schemas = fetch_remote_schemas(server, remote_database);
        
        // Create local virtual schemas mirroring remote structure
        for (const auto& remote_schema : remote_schemas) {
            string local_schema_path = mount_path + ".[" + remote_schema.name + "]";
            schema_manager.create_virtual_schema(local_schema_path);
            
            // Register remote tables as virtual objects
            for (const auto& table : remote_schema.tables) {
                register_remote_table(local_schema_path, table, server);
            }
        }
    }
    
    ResultSet execute_remote_query(const string& remote_path, const string& query) {
        // Parse remote path to get server and database
        auto [server_alias, database, schema, object] = parse_remote_path(remote_path);
        
        RemoteServer& server = servers[server_alias];
        
        // Get connection from pool
        auto conn = server.pool->get_connection();
        
        // Translate query if needed
        string translated_query = translate_query(query, server.type);
        
        // Execute on remote
        return conn->execute(translated_query);
    }
};
```

## Schema Templates and Inheritance

```cpp
class SchemaTemplateManager {
private:
    struct SchemaTemplate {
        string template_name;
        vector<DDLStatement> ddl_statements;
        map<string, string> placeholders;
        SchemaOptions default_options;
    };
    
    map<string, SchemaTemplate> templates;
    
public:
    void create_schema_from_template(
        const string& schema_path,
        const string& template_name,
        const map<string, string>& parameters
    ) {
        SchemaTemplate& tmpl = templates[template_name];
        
        // Create base schema
        schema_manager.create_schema(schema_path);
        
        // Apply template DDL with parameter substitution
        for (const auto& ddl : tmpl.ddl_statements) {
            string sql = ddl.sql;
            
            // Replace placeholders
            for (const auto& [key, value] : parameters) {
                replace_all(sql, "${" + key + "}", value);
            }
            
            // Execute in context of new schema
            execute_in_schema(schema_path, sql);
        }
        
        // Set schema options
        schema_manager.set_options(schema_path, tmpl.default_options);
    }
    
    void create_inheriting_schema(
        const string& child_path,
        const string& parent_path,
        const InheritanceOptions& options
    ) {
        // Create child schema
        schema_manager.create_schema(child_path);
        
        // Set inheritance relationship
        schema_manager.set_parent(child_path, parent_path);
        
        if (options.inherit_permissions) {
            // Copy permissions from parent
            security.inherit_permissions(parent_path, child_path);
        }
        
        if (options.inherit_search_path) {
            // Prepend parent to search path
            auto parent_search_path = schema_manager.get_search_path(parent_path);
            schema_manager.set_search_path(child_path, parent_search_path);
        }
        
        if (options.inherit_objects) {
            // Create views pointing to parent objects
            create_inherited_views(parent_path, child_path);
        }
    }
};
```

## Performance Optimizations

```cpp
class SchemaCache {
private:
    // Cache resolved paths
    LRUCache<string, SchemaNode*> path_cache;
    
    // Cache search path resolutions
    LRUCache<pair<string, vector<string>>, TableDescriptor*> search_cache;
    
    // Cache synonym chains
    LRUCache<string, string> synonym_cache;
    
public:
    SchemaNode* resolve_cached(const string& path) {
        if (auto cached = path_cache.get(path)) {
            return *cached;
        }
        
        auto resolved = schema_resolver.resolve_schema(path);
        path_cache.put(path, resolved);
        return resolved;
    }
    
    void invalidate_schema(const string& schema_path) {
        // Remove all cached entries for this schema
        path_cache.remove_if([&](const auto& entry) {
            return entry.first.find(schema_path) != string::npos;
        });
        
        search_cache.remove_if([&](const auto& entry) {
            return entry.first.second.find(schema_path) != vector<string>::npos;
        });
    }
};
```

## Security Considerations

```cpp
class SchemaSecurityManager {
public:
    bool can_access_schema(const UUID& user_id, const string& schema_path) {
        // Check direct permissions
        if (has_permission(user_id, schema_path, Permission::USAGE)) {
            return true;
        }
        
        // Check role permissions
        for (const auto& role_id : get_user_roles(user_id)) {
            if (has_permission(role_id, schema_path, Permission::USAGE)) {
                return true;
            }
        }
        
        // Check if user owns the schema
        if (is_owner(user_id, schema_path)) {
            return true;
        }
        
        // Check if it's user's home schema
        if (is_user_home_schema(user_id, schema_path)) {
            return true;
        }
        
        // Check inherited permissions
        if (has_inherited_permission(user_id, schema_path)) {
            return true;
        }
        
        return false;
    }
    
    void validate_path_traversal(const string& path) {
        // Prevent directory traversal attacks
        if (path.find("....") != string::npos) {
            throw security_error("Invalid path traversal");
        }
        
        // Validate brackets are balanced
        if (!are_brackets_balanced(path)) {
            throw security_error("Malformed schema path");
        }
        
        // Check for injection attempts
        if (contains_sql_injection(path)) {
            throw security_error("Potential SQL injection in path");
        }
    }
};
```

This hierarchical schema system provides intuitive organization while maintaining security and performance!