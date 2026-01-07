# Git Integration for Metadata Versioning Specification

## 1. Overview

### 1.1 Purpose

ScratchBird Git Integration provides version control for database schema and metadata, enabling:
- Full history of all DDL changes
- DevOps-friendly schema management
- Migration script generation
- Rollback/forward capabilities
- Team collaboration on schema changes
- Audit trail for compliance

### 1.2 Design Philosophy

- **Git as Source of Truth:** Schema definitions stored in Git, applied to databases
- **Bidirectional Sync:** Export from database to Git, import from Git to database
- **Non-Invasive:** Optional feature, databases work without Git integration
- **Standard Git:** Works with any Git repository (GitHub, GitLab, Bitbucket, self-hosted)

### 1.3 Scope (Nice to Have for Alpha 3)

| Feature | Priority |
|---------|----------|
| Schema export to Git | Core |
| DDL change tracking | Core |
| Migration script generation | Core |
| Apply migrations from Git | Core |
| Conflict detection | Core |
| Branch-based environments | Extended |
| PR-based schema review | Extended |
| Automatic rollback scripts | Extended |

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  DevOps Workflow                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ Developer   │  │ CI/CD       │  │ DBA         │             │
│  │ Workstation │  │ Pipeline    │  │ Console     │             │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘             │
└─────────┼────────────────┼────────────────┼─────────────────────┘
          │                │                │
          ▼                ▼                ▼
┌─────────────────────────────────────────────────────────────────┐
│  Git Repository                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  schema/                                                  │  │
│  │  ├── tables/                                              │  │
│  │  │   ├── users.sql                                        │  │
│  │  │   └── orders.sql                                       │  │
│  │  ├── views/                                               │  │
│  │  ├── functions/                                           │  │
│  │  ├── procedures/                                          │  │
│  │  └── triggers/                                            │  │
│  │  migrations/                                              │  │
│  │  ├── 001_initial_schema.sql                               │  │
│  │  ├── 002_add_users_email_index.sql                        │  │
│  │  └── 003_create_orders_table.sql                          │  │
│  │  .scratchbird.yml                                         │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │ Git operations
┌─────────────────────▼───────────────────────────────────────────┐
│  ScratchBird Git Integration Module                              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  SchemaExporter      - Export DDL to files                │  │
│  │  SchemaImporter      - Apply DDL from files               │  │
│  │  MigrationGenerator  - Generate migration scripts         │  │
│  │  MigrationRunner     - Execute migrations                 │  │
│  │  ConflictDetector    - Detect schema conflicts            │  │
│  │  GitOperations       - Clone, pull, push, commit          │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────┐
│  ScratchBird Database                                            │
│  - Schema catalog                                                │
│  - Migration history table                                       │
│  - DDL event triggers                                            │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Repository Structure

### 3.1 Standard Directory Layout

```
my-database-repo/
├── .scratchbird.yml           # Configuration file
├── schema/                    # Current schema definitions
│   ├── tables/
│   │   ├── public.users.sql
│   │   ├── public.orders.sql
│   │   └── public.products.sql
│   ├── views/
│   │   └── public.active_users.sql
│   ├── functions/
│   │   └── public.calculate_total.sql
│   ├── procedures/
│   │   └── public.process_order.sql
│   ├── triggers/
│   │   └── public.audit_users.sql
│   ├── indexes/
│   │   └── public.idx_users_email.sql
│   ├── sequences/
│   │   └── public.users_id_seq.sql
│   ├── domains/
│   │   └── public.email_address.sql
│   └── types/
│       └── public.address_type.sql
├── migrations/                # Ordered migration scripts
│   ├── V001__initial_schema.sql
│   ├── V002__add_email_index.sql
│   ├── V003__create_orders.sql
│   └── V004__add_order_status.sql
├── seeds/                     # Reference data
│   ├── countries.sql
│   └── currencies.sql
├── tests/                     # Schema tests
│   └── test_constraints.sql
└── environments/              # Environment-specific overrides
    ├── development.yml
    ├── staging.yml
    └── production.yml
```

### 3.2 Schema File Format

Each object exported as individual SQL file:

```sql
-- schema/tables/public.users.sql
-- ScratchBird Schema Export
-- Object: TABLE public.users
-- Exported: 2024-03-01T10:30:00Z
-- Checksum: sha256:abc123...

CREATE TABLE public.users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email VARCHAR(255) NOT NULL,
    name VARCHAR(100) NOT NULL,
    status VARCHAR(20) DEFAULT 'active',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP,
    CONSTRAINT users_email_unique UNIQUE (email),
    CONSTRAINT users_status_check CHECK (status IN ('active', 'inactive', 'suspended'))
);

COMMENT ON TABLE public.users IS 'User accounts';
COMMENT ON COLUMN public.users.email IS 'User email address, must be unique';

-- Grants
GRANT SELECT, INSERT, UPDATE ON public.users TO app_role;
GRANT SELECT ON public.users TO readonly_role;
```

### 3.3 Migration File Format

```sql
-- migrations/V003__create_orders.sql
-- Migration: V003
-- Description: Create orders table
-- Author: developer@example.com
-- Date: 2024-03-01
-- Dependencies: V002

-- @up
CREATE TABLE public.orders (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES public.users(id),
    total NUMERIC(12,2) NOT NULL,
    status VARCHAR(20) DEFAULT 'pending',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_orders_user ON public.orders(user_id);
CREATE INDEX idx_orders_status ON public.orders(status);

-- @down
DROP INDEX IF EXISTS idx_orders_status;
DROP INDEX IF EXISTS idx_orders_user;
DROP TABLE IF EXISTS public.orders;
```

---

## 4. Configuration

### 4.1 Configuration File (.scratchbird.yml)

```yaml
# .scratchbird.yml
version: 1

# Repository settings
repository:
  type: git
  url: git@github.com:company/database-schema.git
  branch: main
  auto_commit: false
  auto_push: false

# Schema export settings
schema:
  directory: schema
  include_grants: true
  include_comments: true
  include_defaults: true
  separate_indexes: true
  file_per_object: true
  schemas:
    - public
    - app
  exclude_schemas:
    - pg_catalog
    - information_schema
  exclude_tables:
    - temporary_*
    - _migration_*

# Migration settings
migrations:
  directory: migrations
  table: public._migrations
  naming: versioned        # versioned (V001__), timestamp (20240301_), sequential
  generate_down: true
  transaction_per_file: true
  checksum_validation: true

# Environment settings
environments:
  development:
    database: dev_db
    auto_apply: true
  staging:
    database: staging_db
    approval_required: false
  production:
    database: prod_db
    approval_required: true
    backup_before_apply: true

# Hooks
hooks:
  pre_export:
    - ./scripts/validate_naming.sh
  post_export:
    - ./scripts/update_docs.sh
  pre_migrate:
    - ./scripts/backup_schema.sh
  post_migrate:
    - ./scripts/notify_team.sh

# Notifications
notifications:
  slack:
    webhook: ${SLACK_WEBHOOK_URL}
    channel: "#database-changes"
  email:
    recipients:
      - dba@example.com
```

---

## 5. SQL Interface

### 5.1 Git Repository Management

```sql
-- Initialize Git integration for database
INIT GIT REPOSITORY
    URL 'git@github.com:company/db-schema.git'
    BRANCH 'main'
    OPTIONS (
        ssh_key '/path/to/id_rsa',
        auto_commit FALSE
    );

-- Show Git status
SHOW GIT STATUS;
/*
Repository: git@github.com:company/db-schema.git
Branch: main
Status: clean
Last Sync: 2024-03-01 10:30:00
Pending Changes: 0
*/

-- Pull latest from remote
GIT PULL;

-- Push local changes
GIT PUSH;

-- Switch branch
GIT CHECKOUT BRANCH 'feature/new-tables';

-- Create new branch
GIT CREATE BRANCH 'feature/add-orders';
```

### 5.2 Schema Export

```sql
-- Export entire schema to Git repository
EXPORT SCHEMA TO GIT;

-- Export specific schema
EXPORT SCHEMA public TO GIT;

-- Export specific objects
EXPORT TABLE users, orders TO GIT;
EXPORT FUNCTION calculate_total TO GIT;

-- Export with options
EXPORT SCHEMA TO GIT
    OPTIONS (
        include_grants TRUE,
        include_data FALSE,
        commit_message 'Export schema after refactoring'
    );

-- Show what would be exported (dry run)
EXPORT SCHEMA TO GIT DRY RUN;
```

### 5.3 Schema Import

```sql
-- Import schema from Git (preview only)
IMPORT SCHEMA FROM GIT DRY RUN;
/*
Changes to apply:
+ CREATE TABLE public.orders (...)
~ ALTER TABLE public.users ADD COLUMN phone VARCHAR(20)
- DROP INDEX old_unused_index
*/

-- Apply schema from Git
IMPORT SCHEMA FROM GIT;

-- Import specific objects
IMPORT TABLE orders FROM GIT;

-- Import from specific branch
IMPORT SCHEMA FROM GIT BRANCH 'feature/new-tables';

-- Import with conflict resolution
IMPORT SCHEMA FROM GIT
    ON CONFLICT (
        TABLES: GIT_WINS,
        INDEXES: LOCAL_WINS,
        FUNCTIONS: PROMPT
    );
```

### 5.4 Migration Management

```sql
-- Generate migration from current changes
GENERATE MIGRATION 'add_orders_table';
/*
Created: migrations/V004__add_orders_table.sql
*/

-- Show pending migrations
SHOW MIGRATIONS PENDING;
/*
Version | Description          | File                        | Status
V004    | add_orders_table     | V004__add_orders_table.sql  | pending
V005    | add_order_status     | V005__add_order_status.sql  | pending
*/

-- Show migration history
SHOW MIGRATIONS HISTORY;
/*
Version | Description          | Applied At          | Duration | Checksum
V001    | initial_schema       | 2024-01-15 10:00:00 | 1.234s   | abc123
V002    | add_email_index      | 2024-02-01 14:30:00 | 0.056s   | def456
V003    | create_orders        | 2024-03-01 09:00:00 | 0.234s   | ghi789
*/

-- Apply pending migrations
APPLY MIGRATIONS;

-- Apply specific migration
APPLY MIGRATION V004;

-- Apply up to specific version
APPLY MIGRATIONS TO V005;

-- Rollback last migration
ROLLBACK MIGRATION;

-- Rollback to specific version
ROLLBACK MIGRATIONS TO V003;

-- Validate migrations without applying
VALIDATE MIGRATIONS;
```

### 5.5 Diff and Comparison

```sql
-- Show diff between database and Git
SHOW SCHEMA DIFF;
/*
Object                  | Database | Git     | Difference
public.users            | exists   | exists  | column 'phone' added locally
public.orders           | missing  | exists  | table missing in database
public.old_table        | exists   | missing | table missing in Git
idx_users_email         | exists   | exists  | no difference
*/

-- Detailed diff for specific object
SHOW SCHEMA DIFF FOR TABLE users;
/*
--- Git: schema/tables/public.users.sql
+++ Database: public.users
@@ -5,6 +5,7 @@
     email VARCHAR(255) NOT NULL,
     name VARCHAR(100) NOT NULL,
+    phone VARCHAR(20),
     status VARCHAR(20) DEFAULT 'active',
*/

-- Compare with specific branch
SHOW SCHEMA DIFF WITH BRANCH 'production';

-- Compare two branches
SHOW SCHEMA DIFF BRANCH 'main' WITH BRANCH 'feature/orders';
```

---

## 6. DDL Change Tracking

### 6.1 Automatic Change Capture

DDL statements are automatically captured when Git integration is enabled:

```sql
-- Enable DDL tracking
ALTER DATABASE SET git_track_ddl = TRUE;

-- All DDL changes are now logged
CREATE TABLE new_table (...);
-- Logged: CREATE TABLE new_table at 2024-03-01 10:30:00 by user 'admin'

ALTER TABLE users ADD COLUMN phone VARCHAR(20);
-- Logged: ALTER TABLE users at 2024-03-01 10:31:00 by user 'admin'
```

### 6.2 Change History Table

```sql
-- System table for DDL tracking
CREATE TABLE SYS$DDL_HISTORY (
    event_id UUID PRIMARY KEY,
    event_time TIMESTAMP NOT NULL,
    user_name VARCHAR(128) NOT NULL,
    session_id UUID,
    ddl_type VARCHAR(32) NOT NULL,      -- CREATE, ALTER, DROP, etc.
    object_type VARCHAR(32) NOT NULL,   -- TABLE, INDEX, FUNCTION, etc.
    schema_name VARCHAR(128),
    object_name VARCHAR(128),
    ddl_statement TEXT NOT NULL,
    old_definition TEXT,                -- For ALTER/DROP
    new_definition TEXT,                -- For CREATE/ALTER
    git_commit VARCHAR(40),             -- If committed to Git
    application_name VARCHAR(128),
    client_ip VARCHAR(45)
);

-- Query change history
SELECT * FROM SYS$DDL_HISTORY
WHERE object_name = 'users'
ORDER BY event_time DESC;
```

### 6.3 Change Review

```sql
-- Show uncommitted changes
SHOW GIT UNCOMMITTED CHANGES;
/*
Time                | User   | Type   | Object          | Statement
2024-03-01 10:30:00 | admin  | CREATE | public.orders   | CREATE TABLE...
2024-03-01 10:31:00 | admin  | ALTER  | public.users    | ALTER TABLE...
*/

-- Commit changes to Git
GIT COMMIT MESSAGE 'Add orders table and phone column';

-- Discard uncommitted changes (revert to Git state)
GIT DISCARD CHANGES;

-- Discard specific change
GIT DISCARD CHANGE event_id;
```

---

## 7. Migration Generation

### 7.1 Automatic Migration Generation

```sql
-- Compare database to Git and generate migration
GENERATE MIGRATION 'sync_changes' FROM DIFF;

-- Generated migration:
-- migrations/V005__sync_changes.sql
/*
-- @up
CREATE TABLE public.orders (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES public.users(id),
    total NUMERIC(12,2) NOT NULL
);

ALTER TABLE public.users ADD COLUMN phone VARCHAR(20);

-- @down
ALTER TABLE public.users DROP COLUMN phone;
DROP TABLE public.orders;
*/
```

### 7.2 Migration Templates

```sql
-- Generate empty migration
GENERATE MIGRATION 'add_audit_columns' EMPTY;

-- Generate from template
GENERATE MIGRATION 'add_timestamp_columns'
    TEMPLATE 'add_timestamps'
    PARAMETERS (table_name = 'orders');
```

### 7.3 Migration Dependencies

```sql
-- Specify dependencies
GENERATE MIGRATION 'add_order_items'
    DEPENDS ON 'V004__add_orders';

-- View dependency graph
SHOW MIGRATION DEPENDENCIES;
/*
V001 (initial_schema)
  └── V002 (add_email_index)
  └── V003 (create_orders)
       └── V004 (add_order_items)
       └── V005 (add_order_status)
*/
```

---

## 8. Environment Management

### 8.1 Environment-Specific Configuration

```yaml
# environments/production.yml
database:
  host: prod-db.example.com
  port: 3092
  name: production

migration:
  approval_required: true
  backup_before_apply: true
  notify_on_apply: true
  max_lock_wait: 60s

restrictions:
  disallow_drop_table: true
  disallow_drop_column: true
  require_index_for_fk: true
```

### 8.2 Environment Commands

```sql
-- Show current environment
SHOW GIT ENVIRONMENT;

-- Switch environment
SET GIT ENVIRONMENT = 'production';

-- Apply migrations to specific environment
APPLY MIGRATIONS ENVIRONMENT 'staging';

-- Compare environments
SHOW SCHEMA DIFF ENVIRONMENT 'staging' WITH 'production';
```

---

## 9. CI/CD Integration

### 9.1 GitHub Actions Example

```yaml
# .github/workflows/schema-deploy.yml
name: Schema Deployment

on:
  push:
    branches: [main]
    paths: ['migrations/**']

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Validate Migrations
        run: |
          sb_admin migration validate \
            --host ${{ secrets.DB_HOST }} \
            --database ${{ secrets.DB_NAME }} \
            --user ${{ secrets.DB_USER }} \
            --password ${{ secrets.DB_PASSWORD }}

      - name: Apply Migrations (Staging)
        if: github.ref == 'refs/heads/main'
        run: |
          sb_admin migration apply \
            --environment staging \
            --config .scratchbird.yml

      - name: Notify Slack
        uses: slackapi/slack-github-action@v1
        with:
          payload: |
            {"text": "Schema migration applied to staging"}
```

### 9.2 GitLab CI Example

```yaml
# .gitlab-ci.yml
stages:
  - validate
  - deploy-staging
  - deploy-production

validate-migrations:
  stage: validate
  script:
    - sb_admin migration validate --config .scratchbird.yml
  only:
    changes:
      - migrations/**

deploy-staging:
  stage: deploy-staging
  script:
    - sb_admin migration apply --environment staging
  only:
    - main
  when: on_success

deploy-production:
  stage: deploy-production
  script:
    - sb_admin migration apply --environment production
  only:
    - main
  when: manual
  environment:
    name: production
```

### 9.3 sb_admin CLI Commands

```bash
# Initialize Git integration
sb_admin git init --url git@github.com:company/schema.git

# Export schema
sb_admin schema export --output ./schema

# Generate migration
sb_admin migration generate --name "add_orders" --from-diff

# Validate migrations
sb_admin migration validate

# Apply migrations
sb_admin migration apply --environment production

# Rollback
sb_admin migration rollback --steps 1

# Show status
sb_admin git status
sb_admin migration status

# Diff
sb_admin schema diff --branch main
```

---

## 10. Conflict Resolution

### 10.1 Conflict Detection

```sql
-- Detect conflicts before import
SHOW GIT CONFLICTS;
/*
Object         | Conflict Type      | Local                | Git
public.users   | COLUMN_MODIFIED    | phone VARCHAR(20)    | phone VARCHAR(15)
public.orders  | CONSTRAINT_ADDED   | (none)               | CHECK (total > 0)
idx_users_name | INDEX_DROPPED      | exists               | missing
*/
```

### 10.2 Resolution Strategies

```sql
-- Import with resolution strategy
IMPORT SCHEMA FROM GIT
    ON CONFLICT (
        -- Per-object-type resolution
        TABLES: PROMPT,           -- Ask for each conflict
        INDEXES: GIT_WINS,        -- Always use Git version
        FUNCTIONS: LOCAL_WINS,    -- Always keep local version
        TRIGGERS: MERGE           -- Attempt automatic merge
    );

-- Resolve specific conflict
RESOLVE GIT CONFLICT FOR TABLE users
    STRATEGY GIT_WINS;

-- Manual resolution
RESOLVE GIT CONFLICT FOR TABLE users
    WITH 'ALTER TABLE users ALTER COLUMN phone TYPE VARCHAR(20)';
```

### 10.3 Merge Strategies

| Strategy | Description |
|----------|-------------|
| `GIT_WINS` | Use Git version, discard local changes |
| `LOCAL_WINS` | Keep local version, ignore Git changes |
| `MERGE` | Attempt automatic merge of non-conflicting changes |
| `PROMPT` | Interactive resolution for each conflict |
| `FAIL` | Abort on any conflict |

---

## 11. Audit and Compliance

### 11.1 Audit Trail

```sql
-- Full audit of schema changes
SELECT
    event_time,
    user_name,
    ddl_type,
    object_type,
    schema_name || '.' || object_name AS object,
    git_commit,
    application_name
FROM SYS$DDL_HISTORY
WHERE event_time > CURRENT_TIMESTAMP - INTERVAL '30 days'
ORDER BY event_time DESC;
```

### 11.2 Compliance Reports

```sql
-- Generate compliance report
GENERATE GIT REPORT
    TYPE 'schema_changes'
    FROM '2024-01-01'
    TO '2024-03-31'
    FORMAT 'PDF'
    OUTPUT '/reports/q1_schema_changes.pdf';

-- Available report types:
-- schema_changes - All DDL changes with authors
-- migration_history - Applied migrations timeline
-- rollback_events - All rollbacks with reasons
-- access_changes - GRANT/REVOKE history
-- conflict_resolutions - Conflict resolution decisions
```

---

## 12. System Tables

### 12.1 Migration Tracking

```sql
-- Migration history table (created automatically)
CREATE TABLE SYS$MIGRATIONS (
    version VARCHAR(64) PRIMARY KEY,
    description VARCHAR(256),
    script_name VARCHAR(256) NOT NULL,
    checksum VARCHAR(64) NOT NULL,
    applied_at TIMESTAMP NOT NULL,
    applied_by VARCHAR(128) NOT NULL,
    execution_time_ms INTEGER,
    success BOOLEAN NOT NULL,
    error_message TEXT,
    rolled_back_at TIMESTAMP,
    rolled_back_by VARCHAR(128)
);

-- Migration locks (prevent concurrent migrations)
CREATE TABLE SYS$MIGRATION_LOCK (
    lock_id INTEGER PRIMARY KEY DEFAULT 1,
    locked_at TIMESTAMP,
    locked_by VARCHAR(128),
    lock_reason VARCHAR(256),
    CONSTRAINT single_lock CHECK (lock_id = 1)
);
```

### 12.2 Git Integration State

```sql
-- Git repository configuration
CREATE TABLE SYS$GIT_CONFIG (
    key VARCHAR(128) PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TIMESTAMP NOT NULL
);

-- Git sync history
CREATE TABLE SYS$GIT_SYNC_HISTORY (
    sync_id UUID PRIMARY KEY,
    sync_time TIMESTAMP NOT NULL,
    sync_type VARCHAR(32) NOT NULL,  -- PULL, PUSH, EXPORT, IMPORT
    branch VARCHAR(128),
    commit_before VARCHAR(40),
    commit_after VARCHAR(40),
    objects_affected INTEGER,
    user_name VARCHAR(128),
    success BOOLEAN NOT NULL,
    error_message TEXT
);
```

---

## 13. Security Considerations

### 13.1 Credentials Management

```sql
-- Store Git credentials securely
ALTER GIT REPOSITORY
    SET SSH_KEY = '/secure/path/to/key'
    SET SSH_KEY_PASSPHRASE = 'encrypted:...'

-- Use credential helper
ALTER GIT REPOSITORY
    SET CREDENTIAL_HELPER = 'store --file=/secure/.git-credentials'

-- Environment variable references
ALTER GIT REPOSITORY
    SET URL = '${GIT_REPO_URL}'
    SET SSH_KEY = '${GIT_SSH_KEY_PATH}'
```

### 13.2 Access Control

```sql
-- Grant Git operations to specific roles
GRANT GIT_ADMIN TO dba_role;
GRANT GIT_EXPORT TO developer_role;
GRANT GIT_IMPORT TO deployment_role;
REVOKE GIT_PUSH FROM developer_role;

-- Permissions:
-- GIT_ADMIN - Full Git management
-- GIT_EXPORT - Export schema to Git
-- GIT_IMPORT - Import schema from Git
-- GIT_MIGRATE - Apply migrations
-- GIT_ROLLBACK - Rollback migrations
-- GIT_PUSH - Push to remote
-- GIT_PULL - Pull from remote
```

### 13.3 Sensitive Data Handling

```yaml
# .scratchbird.yml
schema:
  redact_patterns:
    - 'password'
    - 'secret'
    - 'api_key'
  exclude_tables:
    - 'credentials'
    - 'secrets'
  exclude_grants_for:
    - 'admin_role'
```

---

## 14. Error Handling

### 14.1 Error Codes

| Code | Description |
|------|-------------|
| GIT001 | Repository not initialized |
| GIT002 | Remote connection failed |
| GIT003 | Authentication failed |
| GIT004 | Branch not found |
| GIT005 | Merge conflict detected |
| GIT006 | Push rejected (non-fast-forward) |
| MIG001 | Migration file not found |
| MIG002 | Migration already applied |
| MIG003 | Migration checksum mismatch |
| MIG004 | Migration dependency not met |
| MIG005 | Migration execution failed |
| MIG006 | Rollback not possible |
| MIG007 | Migration lock held |

### 14.2 Recovery Procedures

```sql
-- Force release migration lock
RELEASE MIGRATION LOCK FORCE;

-- Mark migration as applied (skip execution)
APPLY MIGRATION V004 SKIP;

-- Repair migration history
REPAIR MIGRATION V004
    SET checksum = 'correct_checksum';

-- Re-baseline migrations
BASELINE MIGRATIONS AT V005;
```

---

## 15. Implementation Notes

### 15.1 Dependencies

- libgit2 for Git operations
- OpenSSL for SSH authentication
- Optional: GitHub/GitLab API for PR integration

### 15.2 Performance Considerations

- Large schema exports chunked by object type
- Incremental export (only changed objects)
- Background sync with configurable intervals
- Migration execution with statement timeout

### 15.3 Limitations

- Binary files (BLOBs) not versioned
- Table data not included (use seeds for reference data)
- Some DDL operations may require exclusive locks
- Cross-database dependencies not tracked
