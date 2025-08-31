# Metadata Version Control System Specification

## Overview

ScratchBird implements integrated version control for all database metadata objects, providing Git-like capabilities with automatic change tracking, branching, and rollback support.

## Core Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    DDL Statement Execution                   │
│            (CREATE, ALTER, DROP, COMMENT, etc.)             │
├─────────────────────────────────────────────────────────────┤
│                  Metadata Change Interceptor                 │
│        (Captures all schema modifications)                   │
├─────────────────────────────────────────────────────────────┤
│                  Version Control Engine                      │
│    (Creates snapshots, diffs, manages history)              │
├─────────────────────────────────────────────────────────────┤
│                    Change Log Storage                        │
│      (Immutable log with full object definitions)           │
├─────────────────────────────────────────────────────────────┤
│                     Git Sync Agent                          │
│        (Optional: Syncs to external Git repo)               │
└─────────────────────────────────────────────────────────────┘
```

## Version Control Tables in [root].[sys]

```sql
-- Main version control log
CREATE TABLE [root].[sys].version_control_log (
    change_id UUID PRIMARY KEY,
    change_number BIGINT GENERATED ALWAYS AS IDENTITY,
    change_timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    change_type ENUM('CREATE', 'ALTER', 'DROP', 'RENAME', 'COMMENT', 'GRANT', 'REVOKE'),
    object_type ENUM('TABLE', 'VIEW', 'PROCEDURE', 'FUNCTION', 'TRIGGER', 'INDEX', 'SCHEMA', 'TYPE', 'DOMAIN'),
    object_uuid UUID NOT NULL,
    object_path VARCHAR(1000),  -- Full hierarchical path at time of change
    object_name VARCHAR(255),    -- Just the object name
    schema_uuid UUID,
    
    -- Version information
    version_before INTEGER,      -- Version number before change
    version_after INTEGER,       -- Version number after change
    
    -- Change details
    change_user UUID REFERENCES [root].[sec].users(user_id),
    change_application VARCHAR(255),
    change_host INET,
    change_transaction_id BIGINT,
    
    -- SQL and BLR
    ddl_statement TEXT,          -- Original DDL statement
    ddl_normalized TEXT,         -- Normalized/formatted DDL
    blr_before BYTEA,           -- BLR before change (for ALTER)
    blr_after BYTEA,            -- BLR after change
    
    -- Full object definitions
    object_definition_before TEXT,  -- Complete object DDL before
    object_definition_after TEXT,   -- Complete object DDL after
    
    -- Metadata
    change_metadata JSONB,       -- Additional context
    change_comment TEXT,         -- User-provided change description
    change_tags TEXT[],          -- Tags for grouping changes
    
    -- Git integration
    git_commit_hash VARCHAR(40),
    git_branch VARCHAR(255),
    git_pushed BOOLEAN DEFAULT FALSE,
    
    INDEX idx_object_uuid (object_uuid),
    INDEX idx_change_timestamp (change_timestamp),
    INDEX idx_change_user (change_user),
    INDEX idx_git_commit (git_commit_hash)
);

-- Object version tracking
CREATE TABLE [root].[sys].object_versions (
    object_uuid UUID,
    version_number INTEGER,
    valid_from TIMESTAMP WITH TIME ZONE,
    valid_to TIMESTAMP WITH TIME ZONE,  -- NULL for current version
    is_current BOOLEAN DEFAULT FALSE,
    definition_hash VARCHAR(64),  -- SHA-256 of object definition
    full_definition TEXT,
    dependencies UUID[],  -- Other objects this depends on
    dependents UUID[],    -- Objects that depend on this
    
    PRIMARY KEY (object_uuid, version_number),
    INDEX idx_current (object_uuid, is_current) WHERE is_current = TRUE
);

-- Schema snapshots (point-in-time)
CREATE TABLE [root].[sys].schema_snapshots (
    snapshot_id UUID PRIMARY KEY,
    snapshot_name VARCHAR(255),
    snapshot_timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    snapshot_type ENUM('MANUAL', 'AUTO', 'BEFORE_MIGRATION', 'RELEASE'),
    created_by UUID REFERENCES [root].[sec].users(user_id),
    description TEXT,
    
    -- Snapshot data
    schemas_included TEXT[],  -- Which schemas are in snapshot
    object_count INTEGER,
    total_size_bytes BIGINT,
    
    -- Git integration
    git_tag VARCHAR(255),
    git_commit VARCHAR(40),
    
    -- Restore information
    is_restorable BOOLEAN DEFAULT TRUE,
    restore_point_lsn BIGINT,  -- Log sequence number for restore
    
    INDEX idx_snapshot_timestamp (snapshot_timestamp),
    INDEX idx_snapshot_name (snapshot_name)
);

-- Snapshot objects (what was in each snapshot)
CREATE TABLE [root].[sys].snapshot_objects (
    snapshot_id UUID REFERENCES schema_snapshots(snapshot_id),
    object_uuid UUID,
    object_type VARCHAR(50),
    object_path VARCHAR(1000),
    object_version INTEGER,
    object_definition TEXT,
    object_hash VARCHAR(64),
    
    PRIMARY KEY (snapshot_id, object_uuid)
);

-- Change sets (group related changes)
CREATE TABLE [root].[sys].change_sets (
    changeset_id UUID PRIMARY KEY,
    changeset_name VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    created_by UUID REFERENCES [root].[sec].users(user_id),
    description TEXT,
    status ENUM('DRAFT', 'REVIEWING', 'APPROVED', 'APPLIED', 'ROLLED_BACK'),
    
    -- Changes in this set
    change_ids UUID[],
    
    -- Review/approval
    reviewed_by UUID REFERENCES [root].[sec].users(user_id),
    reviewed_at TIMESTAMP WITH TIME ZONE,
    approved_by UUID REFERENCES [root].[sec].users(user_id),
    approved_at TIMESTAMP WITH TIME ZONE,
    
    -- Application
    applied_at TIMESTAMP WITH TIME ZONE,
    applied_by UUID REFERENCES [root].[sec].users(user_id),
    rollback_at TIMESTAMP WITH TIME ZONE,
    rollback_by UUID REFERENCES [root].[sec].users(user_id),
    
    -- Git integration
    pull_request_url TEXT,
    git_branch VARCHAR(255)
);
```

## Automatic Change Capture

```cpp
namespace scratchbird::version_control {

class MetadataVersionControl {
private:
    struct ChangeContext {
        UUID change_id;
        string ddl_statement;
        ObjectType object_type;
        UUID object_uuid;
        string object_path;
        ObjectDefinition before_state;
        ObjectDefinition after_state;
    };
    
public:
    // Intercept all DDL operations
    void capture_ddl_change(const DDLStatement& stmt, const ExecutionContext& ctx) {
        ChangeContext change;
        change.change_id = generate_uuid();
        change.ddl_statement = stmt.original_sql;
        
        // Determine change type and object
        auto [change_type, object_info] = analyze_ddl(stmt);
        
        // Capture before state for ALTER/DROP
        if (change_type == ChangeType::ALTER || change_type == ChangeType::DROP) {
            change.before_state = capture_object_state(object_info.uuid);
        }
        
        // Execute the DDL
        execute_ddl(stmt);
        
        // Capture after state for CREATE/ALTER
        if (change_type == ChangeType::CREATE || change_type == ChangeType::ALTER) {
            change.after_state = capture_object_state(object_info.uuid);
        }
        
        // Log the change
        log_change(change, ctx);
        
        // Trigger Git sync if enabled
        if (git_sync_enabled) {
            git_agent.queue_change(change);
        }
    }
    
private:
    ObjectDefinition capture_object_state(UUID object_uuid) {
        ObjectDefinition def;
        
        // Get complete DDL for object
        def.ddl = generate_create_statement(object_uuid);
        
        // Get dependencies
        def.dependencies = get_object_dependencies(object_uuid);
        
        // Get permissions
        def.permissions = get_object_permissions(object_uuid);
        
        // Get comments
        def.comments = get_object_comments(object_uuid);
        
        // Calculate hash for comparison
        def.hash = sha256(def.ddl);
        
        // Get BLR if applicable
        if (has_blr(object_uuid)) {
            def.blr = get_object_blr(object_uuid);
        }
        
        return def;
    }
    
    void log_change(const ChangeContext& change, const ExecutionContext& ctx) {
        // Insert into version control log
        execute_internal(R"(
            INSERT INTO [root].[sys].version_control_log (
                change_id,
                change_type,
                object_type,
                object_uuid,
                object_path,
                change_user,
                change_application,
                change_host,
                ddl_statement,
                object_definition_before,
                object_definition_after,
                blr_before,
                blr_after
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )", change.change_id, change.type, change.object_type,
            change.object_uuid, change.object_path,
            ctx.user_id, ctx.application_name, ctx.client_address,
            change.ddl_statement,
            change.before_state.ddl, change.after_state.ddl,
            change.before_state.blr, change.after_state.blr);
        
        // Update object versions table
        update_object_version(change);
    }
};

} // namespace scratchbird::version_control
```

## Git Integration Agent

```cpp
namespace scratchbird::agents {

class GitSyncAgent : public Agent {
private:
    struct GitConfig {
        string repo_path;
        string remote_url;
        string branch;
        string author_name;
        string author_email;
        bool auto_commit;
        bool auto_push;
        duration sync_interval;
    };
    
    GitConfig config;
    git_repository* repo;
    queue<ChangeContext> pending_changes;
    
public:
    void initialize() override {
        // Initialize or clone repository
        if (filesystem::exists(config.repo_path)) {
            git_repository_open(&repo, config.repo_path.c_str());
        } else {
            git_clone(&repo, config.remote_url.c_str(), config.repo_path.c_str(), nullptr);
        }
    }
    
    void process_change(const ChangeContext& change) {
        // Create directory structure matching schema hierarchy
        string file_path = create_schema_path(change.object_path);
        
        // Write object definition to file
        write_object_file(file_path, change);
        
        // Stage the change
        git_index* index;
        git_repository_index(&index, repo);
        git_index_add_bypath(index, file_path.c_str());
        git_index_write(index);
        
        if (config.auto_commit) {
            commit_change(change);
        }
        
        if (config.auto_push) {
            push_to_remote();
        }
    }
    
private:
    string create_schema_path(const string& object_path) {
        // Convert [root].[app].[crm].customers
        // To: root/app/crm/tables/customers.sql
        
        vector<string> parts = parse_schema_path(object_path);
        string rel_path = boost::algorithm::join(parts, "/");
        
        // Add object type subdirectory
        rel_path += "/" + get_object_type_dir(object_type) + "/";
        rel_path += object_name + ".sql";
        
        return config.repo_path + "/" + rel_path;
    }
    
    void write_object_file(const string& file_path, const ChangeContext& change) {
        ofstream file(file_path);
        
        // Write metadata header
        file << "-- ScratchBird Schema Object\n";
        file << "-- UUID: " << change.object_uuid << "\n";
        file << "-- Version: " << change.version << "\n";
        file << "-- Modified: " << change.timestamp << "\n";
        file << "-- Modified By: " << change.user_name << "\n";
        file << "-- Change ID: " << change.change_id << "\n";
        file << "\n";
        
        // Write object definition
        file << change.after_state.ddl << "\n";
        
        // Write dependencies as comments
        if (!change.after_state.dependencies.empty()) {
            file << "\n-- Dependencies:\n";
            for (const auto& dep : change.after_state.dependencies) {
                file << "-- " << dep << "\n";
            }
        }
        
        // Write permissions
        if (!change.after_state.permissions.empty()) {
            file << "\n-- Permissions:\n";
            for (const auto& perm : change.after_state.permissions) {
                file << perm.to_sql() << ";\n";
            }
        }
    }
    
    void commit_change(const ChangeContext& change) {
        // Create commit message
        stringstream msg;
        msg << change.change_type << " " << change.object_type << ": ";
        msg << change.object_name << "\n\n";
        
        if (!change.change_comment.empty()) {
            msg << change.change_comment << "\n\n";
        }
        
        msg << "Change-Id: " << change.change_id << "\n";
        msg << "Object-UUID: " << change.object_uuid << "\n";
        
        // Create commit
        git_signature* sig;
        git_signature_now(&sig, config.author_name.c_str(), config.author_email.c_str());
        
        git_oid tree_id, commit_id;
        git_tree* tree;
        git_index* index;
        
        git_repository_index(&index, repo);
        git_index_write_tree(&tree_id, index);
        git_tree_lookup(&tree, repo, &tree_id);
        
        git_reference* head_ref;
        git_repository_head(&head_ref, repo);
        
        git_commit_create_v(
            &commit_id, repo, "HEAD", sig, sig,
            nullptr, msg.str().c_str(), tree, 1,
            git_reference_target(head_ref)
        );
        
        // Update version control log with commit hash
        update_git_commit_hash(change.change_id, git_oid_tostr_s(&commit_id));
    }
};

} // namespace scratchbird::agents
```

## Schema Diff and Comparison

```sql
-- Compare two schemas or snapshots
CREATE FUNCTION [root].[sys].compare_schemas(
    source_schema VARCHAR(1000),
    target_schema VARCHAR(1000)
) RETURNS TABLE (
    object_path VARCHAR(1000),
    change_type VARCHAR(20),  -- ADDED, REMOVED, MODIFIED
    source_definition TEXT,
    target_definition TEXT,
    diff_details JSONB
) AS BEGIN
    -- Implementation compares all objects
END;

-- Get schema history
CREATE VIEW [root].[sys].schema_history AS
SELECT 
    object_path,
    object_type,
    change_type,
    change_timestamp,
    change_user,
    change_comment,
    version_before,
    version_after
FROM version_control_log
ORDER BY change_timestamp DESC;

-- Show object evolution
CREATE FUNCTION [root].[sys].show_object_history(
    p_object_path VARCHAR(1000)
) RETURNS TABLE (
    version INTEGER,
    change_date TIMESTAMP,
    change_type VARCHAR(20),
    changed_by VARCHAR(255),
    definition TEXT
) AS BEGIN
    SELECT 
        version_after as version,
        change_timestamp as change_date,
        change_type,
        u.username as changed_by,
        object_definition_after as definition
    FROM version_control_log v
    JOIN [root].[sec].users u ON v.change_user = u.user_id
    WHERE object_path = p_object_path
    ORDER BY version_after;
END;
```

## Rollback and Time Travel

```sql
-- Rollback single object to previous version
CREATE PROCEDURE [root].[sys].rollback_object(
    p_object_uuid UUID,
    p_target_version INTEGER DEFAULT NULL  -- NULL means previous version
) AS BEGIN
    DECLARE v_definition TEXT;
    DECLARE v_object_type VARCHAR(50);
    
    -- Get target definition
    IF p_target_version IS NULL THEN
        -- Get previous version
        SELECT object_definition_before INTO v_definition
        FROM version_control_log
        WHERE object_uuid = p_object_uuid
        ORDER BY change_timestamp DESC
        LIMIT 1;
    ELSE
        -- Get specific version
        SELECT full_definition INTO v_definition
        FROM object_versions
        WHERE object_uuid = p_object_uuid
        AND version_number = p_target_version;
    END IF;
    
    -- Execute rollback DDL
    EXECUTE IMMEDIATE v_definition;
    
    -- Log the rollback
    INSERT INTO version_control_log (change_type, change_comment, ...)
    VALUES ('ROLLBACK', 'Rolled back to version ' || p_target_version, ...);
END;

-- Create snapshot
CREATE PROCEDURE [root].[sys].create_schema_snapshot(
    p_snapshot_name VARCHAR(255),
    p_schemas TEXT[] DEFAULT NULL,  -- NULL means all schemas
    p_description TEXT DEFAULT NULL
) AS BEGIN
    DECLARE v_snapshot_id UUID = gen_random_uuid();
    
    -- Create snapshot record
    INSERT INTO schema_snapshots (
        snapshot_id, snapshot_name, description, schemas_included
    ) VALUES (
        v_snapshot_id, p_snapshot_name, p_description, p_schemas
    );
    
    -- Capture all objects
    INSERT INTO snapshot_objects (snapshot_id, object_uuid, object_definition, ...)
    SELECT 
        v_snapshot_id,
        object_uuid,
        generate_create_statement(object_uuid),
        ...
    FROM [root].[sys].all_objects
    WHERE schema_path = ANY(p_schemas) OR p_schemas IS NULL;
    
    -- Create Git tag if enabled
    IF git_sync_enabled() THEN
        CALL git_create_tag('snapshot_' || p_snapshot_name);
    END IF;
END;

-- Restore from snapshot
CREATE PROCEDURE [root].[sys].restore_snapshot(
    p_snapshot_id UUID,
    p_target_schema VARCHAR(1000) DEFAULT NULL  -- NULL means original locations
) AS BEGIN
    -- Complex restore logic
    -- Handles dependencies, drops existing objects, recreates from snapshot
END;
```

## Change Management Workflow

```sql
-- Create change set for review
CREATE PROCEDURE [root].[sys].create_changeset(
    p_name VARCHAR(255),
    p_description TEXT
) RETURNS UUID AS BEGIN
    DECLARE v_changeset_id UUID = gen_random_uuid();
    
    INSERT INTO change_sets (
        changeset_id, changeset_name, description, status
    ) VALUES (
        v_changeset_id, p_name, p_description, 'DRAFT'
    );
    
    -- Set session to track changes
    SET SESSION current_changeset = v_changeset_id;
    
    RETURN v_changeset_id;
END;

-- Add changes to changeset
CREATE TRIGGER capture_changeset_changes
AFTER INSERT ON version_control_log
FOR EACH ROW
WHEN current_setting('current_changeset') IS NOT NULL
BEGIN
    UPDATE change_sets
    SET change_ids = array_append(change_ids, NEW.change_id)
    WHERE changeset_id = current_setting('current_changeset')::UUID;
END;

-- Review changeset
CREATE PROCEDURE [root].[sys].review_changeset(
    p_changeset_id UUID
) RETURNS TABLE (
    change_number INTEGER,
    object_path VARCHAR(1000),
    change_type VARCHAR(20),
    ddl_statement TEXT,
    impact_analysis JSONB
) AS BEGIN
    -- Show all changes and analyze impact
END;

-- Apply changeset
CREATE PROCEDURE [root].[sys].apply_changeset(
    p_changeset_id UUID,
    p_dry_run BOOLEAN DEFAULT TRUE
) AS BEGIN
    -- Apply all changes in order
    -- Handle dependencies
    -- Rollback on error if not dry run
END;
```

## Configuration and Management

```sql
-- Enable/disable version control
ALTER SYSTEM SET version_control_enabled = TRUE;
ALTER SYSTEM SET version_control_level = 'FULL';  -- FULL, DDL_ONLY, NONE

-- Configure Git integration
ALTER SYSTEM SET git_sync_enabled = TRUE;
ALTER SYSTEM SET git_repo_path = '/var/lib/scratchbird/schema_repo';
ALTER SYSTEM SET git_remote_url = 'git@github.com:company/db-schema.git';
ALTER SYSTEM SET git_auto_commit = TRUE;
ALTER SYSTEM SET git_auto_push = FALSE;
ALTER SYSTEM SET git_sync_interval = '5 minutes';

-- Set up automatic snapshots
CREATE EVENT create_daily_snapshot
ON SCHEDULE EVERY 1 DAY
STARTS '2024-01-01 02:00:00'
DO CALL [root].[sys].create_schema_snapshot(
    'auto_daily_' || to_char(CURRENT_DATE, 'YYYY_MM_DD'),
    NULL,
    'Automatic daily snapshot'
);

-- View current configuration
SELECT * FROM [root].[sys].version_control_config;
```

## Benefits

1. **Complete Audit Trail**: Every schema change is logged
2. **Git Integration**: Schema as code with full Git workflow
3. **Rollback Capability**: Undo changes at any level
4. **Change Management**: Review and approve changes before applying
5. **Snapshots**: Point-in-time recovery for schema
6. **Diff and Compare**: See what changed between versions
7. **Dependencies Tracked**: Know impact of changes
8. **Collaboration**: Multiple developers can work on schema

This system provides enterprise-grade version control for database schemas, making database development as manageable as application development!