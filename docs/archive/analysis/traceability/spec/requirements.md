[ScratchBird Analysis Documentation](../../index.md)

### Requirements index (from ProjectPlan)

Each item links back to `ProjectPlan/*` sources and will be mapped to code anchors and docs. See grouped sections below for full entries.

Future phases (12–22) to be added as scope matures.

### Grouped requirements with details

### CORE-HEAP
<a id="req-core-heap-ods"></a>
- REQ-CORE-HEAP-ODS
  - title: Heap on-disk structures and page layout
  - source: ProjectPlan/Phase 1 — Heap storage and row format: detailed implementation plan.md — "1. On-disk structures (ODS) and constants"
  - priority: Must
<a id="req-core-heap-tuple-format"></a>
- REQ-CORE-HEAP-TUPLE-FORMAT
  - title: Heap tuple record header and varlena/overflow encoding
  - source: ProjectPlan/Phase 1 — Heap storage and row format: detailed implementation plan.md — "3. Encoding/decoding and null/varlen handling"
  - priority: Must
<a id="req-core-heap-api"></a>
- REQ-CORE-HEAP-API
  - title: HeapRelation API and HeapScan iterator
  - source: ProjectPlan/Phase 1 — Heap storage and row format: detailed implementation plan.md — "2. Core APIs and modules"
  - priority: Must
<a id="req-core-heap-scan"></a>
- REQ-CORE-HEAP-SCAN
  - title: Sequential scan and fetch semantics with visibility hooks
  - source: ProjectPlan/Phase 1 — Heap storage and row format: detailed implementation plan.md — "5. Scanners and basic selection"
  - priority: Must
<a id="req-core-heap-validator"></a>
- REQ-CORE-HEAP-VALIDATOR
  - title: Heap page validator CLI (heap_check)
  - source: ProjectPlan/Phase 1 — Heap storage and row format: detailed implementation plan.md — "6. Validation and page tools"
  - priority: Must

### CORE-SPACE
<a id="req-core-space-pip"></a>
- REQ-CORE-SPACE-PIP
  - title: Pointer/Free Page Map (PIP) layout and semantics
  - source: ProjectPlan/Phase 2 — Space management and allocation: detailed implementation plan.md — "ODS and on-disk structures (extensions) → PIP layout"
  - priority: Must
<a id="req-core-space-tip-seed"></a>
- REQ-CORE-SPACE-TIP-SEED
  - title: TIP seed and basic transaction state bytes
  - source: ProjectPlan/Phase 2 — Space management and allocation: detailed implementation plan.md — "ODS and on-disk structures (extensions) → TIP layout (Phase 2 seed)"
  - priority: Must
<a id="req-core-space-catalog"></a>
- REQ-CORE-SPACE-CATALOG
  - title: Space Catalog definitions and entries
  - source: ProjectPlan/Phase 2 — Space management and allocation: detailed implementation plan.md — "ODS and on-disk structures (extensions) → Space Catalog"
  - priority: Must
<a id="req-core-space-allocator"></a>
- REQ-CORE-SPACE-ALLOCATOR
  - title: Allocator API and deterministic growth policy
  - source: ProjectPlan/Phase 2 — Space management and allocation: detailed implementation plan.md — "Allocator design"
  - priority: Must
<a id="req-core-space-reclaim"></a>
- REQ-CORE-SPACE-RECLAIM
  - title: Reclaim on drop/truncate via Allocator::free_page
  - source: ProjectPlan/Phase 2 — Space management and allocation: detailed implementation plan.md — "Free space tracking → Reclaim on drop/truncate"
  - priority: Must

### TXN-MGA
<a id="req-txn-mga-tip"></a>
- REQ-TXN-MGA-TIP
  - title: TIP usage and transaction state encoding
  - source: ProjectPlan/Phase 3 — Transactions and MGA: detailed implementation plan.md — "ODS updates and semantics → TIP (Transaction Inventory Page) usage"
  - priority: Must
<a id="req-txn-mga-snapshot-rc"></a>
- REQ-TXN-MGA-SNAPSHOT-RC
  - title: Read Committed snapshot acquisition and semantics
  - source: ProjectPlan/Phase 3 — Transactions and MGA: detailed implementation plan.md — "Row visibility rules" and "Transaction Manager → snapshot_read_committed()"
  - priority: Must
<a id="req-txn-mga-rr-scaffold"></a>
- REQ-TXN-MGA-RR-SCAFFOLD
  - title: Repeatable Read/Snapshot Isolation scaffolding
  - source: ProjectPlan/Phase 3 — Transactions and MGA: detailed implementation plan.md — "Isolation levels (Phase 3 scope)"
  - priority: Must
<a id="req-txn-mga-visibility"></a>
- REQ-TXN-MGA-VISIBILITY
  - title: Tuple version visibility rules and chain traversal
  - source: ProjectPlan/Phase 3 — Transactions and MGA: detailed implementation plan.md — "Row visibility rules" and "Visibility and executor integration"
  - priority: Must
<a id="req-txn-mga-conflicts"></a>
- REQ-TXN-MGA-CONFLICTS
  - title: Writer conflict detection under RC
  - source: ProjectPlan/Phase 3 — Transactions and MGA: detailed implementation plan.md — "Conflict detection (Phase 3 scope)"
  - priority: Must

### CATALOG-BOOT
<a id="req-catalog-boot-sdb-tables"></a>
- REQ-CATALOG-BOOT-SDB-TABLES
  - title: SDB$* catalog schema materialization
  - source: ProjectPlan/Phase 4 — Catalog persistence and bootstrap: detailed implementation plan.md — "SDB$ schema (core)"
  - priority: Must
<a id="req-catalog-boot-uuid"></a>
- REQ-CATALOG-BOOT-UUID
  - title: Fixed UUIDs and well-known IDs for core objects
  - source: ProjectPlan/Phase 4 — Catalog persistence and bootstrap: detailed implementation plan.md — "Fixed UUIDs and well-known IDs"
  - priority: Must
<a id="req-catalog-boot-bootstrap-sql"></a>
- REQ-CATALOG-BOOT-BOOTSTRAP-SQL
  - title: Bootstrap SQL execution for views and system objects
  - source: ProjectPlan/Phase 4 — Catalog persistence and bootstrap: detailed implementation plan.md — "Bootstrap sequence (idempotent)"
  - priority: Must
<a id="req-catalog-boot-ddl-txn"></a>
- REQ-CATALOG-BOOT-DDL-TXN
  - title: Transactional DDL surfaces and CatalogManager APIs
  - source: ProjectPlan/Phase 4 — Catalog persistence and bootstrap: detailed implementation plan.md — "Minimal DDL executor (Phase 4)" and "CatalogManager APIs"
  - priority: Must
<a id="req-catalog-boot-compat-views"></a>
- REQ-CATALOG-BOOT-COMPAT-VIEWS
  - title: RDB$*/MON$* compatibility views
  - source: ProjectPlan/Phase 4 — Catalog persistence and bootstrap: detailed implementation plan.md — "Compatibility views (RDB$*/MON$*)"
  - priority: Must

### EXEC-ENGINE
<a id="req-exec-engine-expr"></a>
- REQ-EXEC-ENGINE-EXPR
  - title: Expression evaluator and lowering from AST
  - source: ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md — "Expression evaluator"
  - priority: Must
<a id="req-exec-engine-scan-seq"></a>
- REQ-EXEC-ENGINE-SCAN-SEQ
  - title: Sequential table scan with predicate pushdown
  - source: ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md — "SeqScanNode"
  - priority: Must
<a id="req-exec-engine-scan-index"></a>
- REQ-EXEC-ENGINE-SCAN-INDEX
  - title: Index scan for point and range with recheck
  - source: ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md — "IndexScanNode"
  - priority: Must
<a id="req-exec-engine-sort"></a>
- REQ-EXEC-ENGINE-SORT
  - title: ORDER BY with stable sort and spill to external merge
  - source: ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md — "SortNode"
  - priority: Must
<a id="req-exec-engine-limit"></a>
- REQ-EXEC-ENGINE-LIMIT
  - title: LIMIT/OFFSET operator semantics
  - source: ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md — "LimitNode"
  - priority: Must
<a id="req-exec-engine-agg"></a>
- REQ-EXEC-ENGINE-AGG
  - title: Hash aggregation with spill and fallback to sort-based
  - source: ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md — "HashAggNode"
  - priority: Must
<a id="req-exec-engine-join-nl"></a>
- REQ-EXEC-ENGINE-JOIN-NL
  - title: Nested loop join (inner and left outer)
  - source: ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md — "NestedLoopJoinNode"
  - priority: Must
<a id="req-exec-engine-window-subset"></a>
- REQ-EXEC-ENGINE-WINDOW-SUBSET
  - title: Window functions subset (ROW_NUMBER/RANK/DENSE_RANK, SUM/AVG)
  - source: ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md — "WindowNode (subset)"
  - priority: Must
<a id="req-exec-engine-explain"></a>
- REQ-EXEC-ENGINE-EXPLAIN
  - title: EXPLAIN and EXPLAIN ANALYZE with instrumentation
  - source: ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md — "Instrumentation and EXPLAIN ANALYZE"
  - priority: Must

### OPT-STAT
<a id="req-opt-stat-cardinality"></a>
- REQ-OPT-STAT-CARDINALITY
  - title: Cardinality/selectivity estimation using stats (histograms/MCVs/correlation)
  - source: ProjectPlan/Phase 6 — Optimizer and statistics: detailed implementation plan.md — "Statistics and estimation"
  - priority: Must
<a id="req-opt-stat-cost-model"></a>
- REQ-OPT-STAT-COST-MODEL
  - title: Cost model for operators (I/O + CPU)
  - source: ProjectPlan/Phase 6 — Optimizer and statistics: detailed implementation plan.md — "Operator costing"
  - priority: Must
<a id="req-opt-stat-join-ordering"></a>
- REQ-OPT-STAT-JOIN-ORDERING
  - title: Join ordering (DP for small N; greedy for larger N)
  - source: ProjectPlan/Phase 6 — Optimizer and statistics: detailed implementation plan.md — "Join ordering"
  - priority: Must
<a id="req-opt-stat-index-selection"></a>
- REQ-OPT-STAT-INDEX-SELECTION
  - title: Index selection based on sargability and cost
  - source: ProjectPlan/Phase 6 — Optimizer and statistics: detailed implementation plan.md — "Index selection"
  - priority: Must
<a id="req-opt-stat-plan-cache"></a>
- REQ-OPT-STAT-PLAN-CACHE
  - title: Plan cache for prepared statements with invalidation
  - source: ProjectPlan/Phase 6 — Optimizer and statistics: detailed implementation plan.md — "Plan cache and prepared statements"
  - priority: Must
<a id="req-opt-stat-explain-analyze"></a>
- REQ-OPT-STAT-EXPLAIN-ANALYZE
  - title: EXPLAIN/ANALYZE alignment and skew warnings
  - source: ProjectPlan/Phase 6 — Optimizer and statistics: detailed implementation plan.md — "EXPLAIN and instrumentation"
  - priority: Must

### INTEGRITY
<a id="req-integrity-check"></a>
- REQ-INTEGRITY-CHECK
  - title: CHECK constraint evaluation and deferrable handling
  - source: ProjectPlan/Phase 7 — Constraints, RI, triggers: detailed implementation plan.md — "Execution model and enforcement → CHECK"
  - priority: Must
<a id="req-integrity-notnull"></a>
- REQ-INTEGRITY-NOTNULL
  - title: NOT NULL enforcement in write path
  - source: ProjectPlan/Phase 7 — Constraints, RI, triggers: detailed implementation plan.md — "Execution model and enforcement → NOT NULL"
  - priority: Must
<a id="req-integrity-unique-pk"></a>
- REQ-INTEGRITY-UNIQUE-PK
  - title: UNIQUE/PRIMARY KEY enforcement with backing unique index
  - source: ProjectPlan/Phase 7 — Constraints, RI, triggers: detailed implementation plan.md — "Execution model and enforcement → UNIQUE/PRIMARY KEY"
  - priority: Must
<a id="req-integrity-fk"></a>
- REQ-INTEGRITY-FK
  - title: FOREIGN KEY enforcement (immediate and deferred; referential actions)
  - source: ProjectPlan/Phase 7 — Constraints, RI, triggers: detailed implementation plan.md — "Execution model and enforcement → FOREIGN KEY"
  - priority: Must
<a id="req-integrity-deferrability"></a>
- REQ-INTEGRITY-DEFERRABILITY
  - title: DEFERRABLE/INITIALLY and SET CONSTRAINTS semantics
  - source: ProjectPlan/Phase 7 — Constraints, RI, triggers: detailed implementation plan.md — "Deferrability model" and "SET CONSTRAINTS"
  - priority: Must

### TRIGGERS
<a id="req-triggers-row-statement"></a>
- REQ-TRIGGERS-ROW-STATEMENT
  - title: BEFORE/AFTER row- and statement-level trigger execution model
  - source: ProjectPlan/Phase 7 — Constraints, RI, triggers: detailed implementation plan.md — "Trigger execution engine" and "Execution cascade per statement"
  - priority: Must
<a id="req-triggers-transition-tables"></a>
- REQ-TRIGGERS-TRANSITION-TABLES
  - title: Transition tables (NEW/OLD; INSERTED/DELETED sets)
  - source: ProjectPlan/Phase 7 — Constraints, RI, triggers: detailed implementation plan.md — "Triggers" and transition data handling
  - priority: Must

### PSQL-RUNTIME
<a id="req-psql-runtime-execute-block"></a>
- REQ-PSQL-RUNTIME-EXECUTE-BLOCK
  - title: EXECUTE BLOCK execution in main executor
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "1.2 EXECUTE BLOCK Execution"
  - priority: Must
<a id="req-psql-runtime-vars"></a>
- REQ-PSQL-RUNTIME-VARS
  - title: Variable storage, typing, and scoping
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "1.1 PSQL Execution Context" and "1.3 Variable Management"
  - priority: Must
<a id="req-psql-runtime-control-flow"></a>
- REQ-PSQL-RUNTIME-CONTROL-FLOW
  - title: IF/WHILE control flow semantics
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "1.4 Basic Control Flow"
  - priority: Must
<a id="req-psql-runtime-procedure"></a>
- REQ-PSQL-RUNTIME-PROCEDURE
  - title: CREATE/EXECUTE PROCEDURE lifecycle
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "2.1 Stored Procedures"
  - priority: Must
<a id="req-psql-runtime-function"></a>
- REQ-PSQL-RUNTIME-FUNCTION
  - title: CREATE/EXECUTE FUNCTION and return handling
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "2.2 User-Defined Functions" and "3.2 Function Execution Pipeline"
  - priority: Must
<a id="req-psql-runtime-exception"></a>
- REQ-PSQL-RUNTIME-EXCEPTION
  - title: Exceptions, RAISE, and WHEN handlers
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "4.1 Exception Handling"
  - priority: Must
<a id="req-psql-runtime-cursor"></a>
- REQ-PSQL-RUNTIME-CURSOR
  - title: DECLARE/OPEN/FETCH/CLOSE cursor operations
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "4.2 Cursor Support"
  - priority: Must
<a id="req-psql-runtime-security-context"></a>
- REQ-PSQL-RUNTIME-SECURITY-CONTEXT
  - title: SECURITY DEFINER/INVOKER execution context
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "4.3 Security Context Management"
  - priority: Must
<a id="req-psql-runtime-packages"></a>
- REQ-PSQL-RUNTIME-PACKAGES
  - title: Package specification/body infrastructure
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "4.5 Package Support (Basic)"
  - priority: Must
<a id="req-psql-runtime-dev-tools"></a>
- REQ-PSQL-RUNTIME-DEV-TOOLS
  - title: Development tools (definition/reference search, formatter, profiler)
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "4.8 Development Tools" and "5.4 Development Tools Enhancements"
  - priority: Must
<a id="req-psql-runtime-debug"></a>
- REQ-PSQL-RUNTIME-DEBUG
  - title: PSQL debugging (breakpoints, step controls, inspection)
  - source: ProjectPlan/Phase 8 — PSQL Runtume: detailed implementation plan.md — "4.7 PSQL Debugging Support"
  - priority: Must

### INDEX-FAMILIES
<a id="req-index-families-btree"></a>
- REQ-INDEX-FAMILIES-BTREE
  - title: B-Tree index functionality and online build
  - source: ProjectPlan/Phase 9 — Index Index families and advanced options.md — "Complete Index Family Portfolio" and B-Tree baseline
  - priority: Must
<a id="req-index-families-hash"></a>
- REQ-INDEX-FAMILIES-HASH
  - title: Hash index with extensible hashing and uniqueness support
  - source: ProjectPlan/Phase 9 — Index Index families and advanced options.md — "Advanced Index Types — Hash Index Architecture"
  - priority: Must
<a id="req-index-families-bitmap"></a>
- REQ-INDEX-FAMILIES-BITMAP
  - title: Bitmap index (WAH/RLE) for low-cardinality data
  - source: ProjectPlan/Phase 9 — Index Index families and advanced options.md — "Complete Index Family Portfolio" and bitmap details
  - priority: Must
<a id="req-index-families-gin"></a>
- REQ-INDEX-FAMILIES-GIN
  - title: GIN index for full-text search with tokenization
  - source: ProjectPlan/Phase 9 — Index Index families and advanced options.md — "Complete Index Family Portfolio" and GIN details
  - priority: Must
<a id="req-index-families-rtree"></a>
- REQ-INDEX-FAMILIES-RTREE
  - title: R-Tree spatial index with rectangle queries
  - source: ProjectPlan/Phase 9 — Index Index families and advanced options.md — "Complete Index Family Portfolio" and R-Tree details
  - priority: Must
<a id="req-index-families-lsm"></a>
- REQ-INDEX-FAMILIES-LSM
  - title: LSM-Tree write-optimized index with compaction
  - source: ProjectPlan/Phase 9 — Index Index families and advanced options.md — "Advanced Index Types — LSM-Tree Architecture"
  - priority: Must
<a id="req-index-families-columnstore"></a>
- REQ-INDEX-FAMILIES-COLUMNSTORE
  - title: Columnstore index with compression and vectorization
  - source: ProjectPlan/Phase 9 — Index Index families and advanced options.md — "Advanced Index Types — Columnstore Architecture"
  - priority: Must
<a id="req-index-families-ttl"></a>
- REQ-INDEX-FAMILIES-TTL
  - title: TTL index with automatic expiry/cleanup
  - source: ProjectPlan/Phase 9 — Index Index families and advanced options.md — "Complete Index Family Portfolio" and TTL features
  - priority: Must
<a id="req-index-families-online-build"></a>
- REQ-INDEX-FAMILIES-ONLINE-BUILD
  - title: Online index build and validation paths
  - source: ProjectPlan/Phase 9 — Index Index families and advanced options.md — "Enterprise-Grade Infrastructure" and validation/management
  - priority: Must

### FDW
<a id="req-fdw-core"></a>
- REQ-FDW-CORE
  - title: FDW SPI and plugin architecture
  - source: ProjectPlan/Phase 10 — FDW⁄SPI and Database Links: detailed implementation plan.md — "10.1.1 FDW Service Provider Interface (SPI)"
  - priority: Must
<a id="req-fdw-csv"></a>
- REQ-FDW-CSV
  - title: CSV file FDW adapter
  - source: ProjectPlan/Phase 10 — FDW⁄SPI and Database Links: detailed implementation plan.md — "10.3.1 CSV File FDW"
  - priority: Must
<a id="req-fdw-json"></a>
- REQ-FDW-JSON
  - title: JSON file FDW adapter
  - source: ProjectPlan/Phase 10 — FDW⁄SPI and Database Links: detailed implementation plan.md — "10.3.2 JSON File FDW"
  - priority: Must
<a id="req-fdw-postgresql"></a>
- REQ-FDW-POSTGRESQL
  - title: PostgreSQL FDW (libpq, pushdown, type mapping)
  - source: ProjectPlan/Phase 10 — FDW⁄SPI and Database Links: detailed implementation plan.md — "10.4 PostgreSQL FDW Implementation"
  - priority: Must
<a id="req-fdw-catalog"></a>
- REQ-FDW-CATALOG
  - title: Catalog integration for FDW objects
  - source: ProjectPlan/Phase 10 — FDW⁄SPI and Database Links: detailed implementation plan.md — "10.9: Catalog Integration"
  - priority: Must
<a id="req-fdw-security"></a>
- REQ-FDW-SECURITY
  - title: FDW security model and credential handling
  - source: ProjectPlan/Phase 10 — FDW⁄SPI and Database Links: detailed implementation plan.md — "10.7 Security and Access Control"
  - priority: Must

### DBLINK
<a id="req-dblink-core"></a>
- REQ-DBLINK-CORE
  - title: Database link creation, resolution, and transaction support
  - source: ProjectPlan/Phase 10 — FDW⁄SPI and Database Links: detailed implementation plan.md — "10.5 Database Link Implementation"
  - priority: Must

### SERVER
<a id="req-server-listener"></a>
- REQ-SERVER-LISTENER
  - title: TCP network listener and accept loop
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.1.1 TCP Listener Infrastructure"
  - priority: Should
<a id="req-server-session"></a>
- REQ-SERVER-SESSION
  - title: Session lifecycle and resource tracking
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.1.1 Session management"
  - priority: Should

### PROTOCOL
<a id="req-protocol-firebird"></a>
- REQ-PROTOCOL-FIREBIRD
  - title: Firebird wire protocol compatibility and operations
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.2 Firebird Wire Protocol Implementation"
  - priority: Should
<a id="req-protocol-scratchbird"></a>
- REQ-PROTOCOL-SCRATCHBIRD
  - title: Protocol abstraction and message handling for native protocol
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.1.2 Protocol Handler Framework"
  - priority: Should

### PROVIDER
<a id="req-provider-dispatch"></a>
- REQ-PROVIDER-DISPATCH
  - title: Y-Valve provider registration and routing
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.3 Y-Valve Architecture"
  - priority: Should

### AUTH
<a id="req-auth-password"></a>
- REQ-AUTH-PASSWORD
  - title: Password authentication provider and policies
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.4.2 Password Authentication"
  - priority: Should
<a id="req-auth-trusted"></a>
- REQ-AUTH-TRUSTED
  - title: Trusted OS authentication (SSPI/Kerberos, PAM/Unix)
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.4.3 Trusted OS Authentication"
  - priority: Should
<a id="req-auth-2fa"></a>
- REQ-AUTH-2FA
  - title: Two-factor authentication (TOTP/SMS/hardware)
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.4.4 Two-Factor Authentication (2FA)"
  - priority: Should

### TLS
<a id="req-tls"></a>
- REQ-TLS
  - title: TLS server support and advanced TLS features
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.5 TLS and Security"
  - priority: Should

### NETWORK
<a id="req-network-buffer"></a>
- REQ-NETWORK-BUFFER
  - title: Protocol message framing, buffering, and parsing
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.1.2 Protocol Handler Framework — Message framing and parsing"
  - priority: Should

### CONNECTION
<a id="req-connection-pool"></a>
- REQ-CONNECTION-POOL
  - title: Server-side connection pooling and resource optimization
  - source: ProjectPlan/Phase 11 — Server (Y-Valve) and Protocol⁄Auth: Detailed Implementation.md — "11.7 Performance and Scalability — 11.7.1 Connection Pooling"
  - priority: Should

## Related
- [ScratchBird Analysis Documentation](../../index.md)
