# Parser Implementation Gaps (Derived from Audit Docs)

This document consolidates missing/stubbed/partial parser functionality for all four parsers.
Sources: `ScratchBird/docs/audit/languages/*/*.md`, plus per-parser correction checklists.


## native

### ScratchBird/docs/audit/languages/native/01_databases_and_schemas.md
### Spec Deltas (Implementation differs from specification)

**CREATE DATABASE:**
- Spec defines additional options not parsed in V2:
  - `PAGE_SIZE` (8K|16K|32K|64K|128K)
  - `DEFAULT CHARACTER SET`
  - `DEFAULT COLLATE`
  - `ENCRYPTED [WITH PASSWORD]`
  - `OWNER`
- Current implementation only supports basic syntax without these options
- Spec reference: `/docs/specifications/ddl/DDL_DATABASES.md`

**ALTER DATABASE:**
- Spec supports additional operations not parsed in V2:
  - `SET DEFAULT CHARACTER SET <charset>`
  - `SET DEFAULT COLLATE <collation>`
  - `SET SWEEP INTERVAL <integer>`
- Only RENAME TO, OWNER TO, and ALIAS operations are currently supported
- Spec reference: `/docs/specifications/ddl/DDL_DATABASES.md`

**DROP DATABASE:**
- CASCADE/RESTRICT modifiers are accepted by parser but executor uses FORCE semantics
- Active sessions are forcibly disconnected regardless of CASCADE/RESTRICT setting
- May not fully align with spec-defined behavior
- Spec reference: `/docs/specifications/ddl/CASCADE_DROP_SPECIFICATION.md`

**ALTER SCHEMA:**
- Behavior and use cases need clarification in specification
- May not be commonly used in practice
- Spec reference: `/docs/specifications/ddl/DDL_SCHEMAS.md`

### ScratchBird/docs/audit/languages/native/02_tables_and_constraints.md
### Partial Implementation

**TEMPORARY Tables:**
- TEMPORARY flag is parsed but not enforced end-to-end
- Tables created with TEMPORARY keyword become permanent tables
- Session-scoped cleanup does not occur
- Spec reference: `/docs/specifications/TEMPORARY_TABLES_SPECIFICATION.md`

**UNLOGGED Tables:**
- UNLOGGED flag is parsed but not enforced
- All tables are logged regardless of UNLOGGED keyword
- No performance benefit from UNLOGGED currently

**Table Options:**
- Storage parameters (TOAST, compression) are spec-defined but not fully wired in V2
- CREATE TABLE ... TABLESPACE is parsed but ignored in bytecode
- ALTER TABLE ... SET TABLESPACE is implemented but requires an existing tablespace
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

### Stubbed Features

**CREATE TABLE AS SELECT (CTAS):**
- Not parsed in V2
- Spec defines syntax: `CREATE TABLE <name> AS SELECT ... [WITH [NO] DATA]`
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

**CREATE TABLE LIKE:**
- Not parsed in V2
- Spec defines syntax: `CREATE TABLE <name> LIKE <source_table>`
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

**Table Inheritance:**
- INHERITS clause not supported
- PostgreSQL-style table inheritance not implemented

**Table Partitioning:**
- PARTITION BY clause not parsed
- Range, list, and hash partitioning not supported
- Spec reference: `/docs/specifications/ddl/DDL_TABLE_PARTITIONING.md`

### Missing Features

**ALTER TABLE Subcommands:**
- Not all spec-defined ALTER TABLE operations are implemented
- Missing operations include:
  - ALTER COLUMN SET STATISTICS
  - ALTER COLUMN SET STORAGE
  - ENABLE/DISABLE TRIGGER
  - INHERIT/NO INHERIT
  - ADD/DROP PARTITION
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

**Constraint Features:**
- DEFERRABLE and INITIALLY DEFERRED not supported
- Named constraint support is partial
- Constraint validation (VALIDATE CONSTRAINT) not implemented

**Identity Column Options:**
- Full IDENTITY column options not completely wired
- GENERATED ALWAYS vs BY DEFAULT may not be fully enforced
- Sequence options for IDENTITY columns limited

### Spec Deltas

**DROP TABLE CASCADE:**
- Executor uses conservative RESTRICT policy for some dependencies
- CASCADE behavior may not fully match specification
- Spec reference: `/docs/specifications/ddl/CASCADE_DROP_SPECIFICATION.md`

**TRUNCATE TABLE:**
- CASCADE semantics need verification against spec
- Behavior with foreign keys may differ from specification
- Spec reference: `/docs/specifications/ddl/CASCADE_DROP_SPECIFICATION.md`

**Constraint Enforcement:**
- Check constraint evaluation may not match all edge cases in spec
- Foreign key ON UPDATE/ON DELETE actions need full validation
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

### ScratchBird/docs/audit/languages/native/03_indexes_views_sequences.md
### Partial Implementation

**Index Types:**
- GIN: Partial implementation (Phase 1-3 complete, not fully tested)
- GiST: Stub implementation only
- BRIN: Stub implementation only
- HNSW: Stub (vector search)
- Spec reference: `/docs/specifications/indexes/INDEX_IMPLEMENTATION_SPEC.md`

**Index Type Gaps in V2 Parser:**
- SPGIST not parsed
- RTREE not parsed (use GiST instead)
- HNSW not parsed
- BITMAP not parsed
- COLUMNSTORE not parsed
- LSM not parsed
- Spec reference: `/docs/specifications/V2_PARSER_INDEX_TYPE_COMPLETENESS.md`

**Advanced Index Features:**
- Index-only scans work for B-Tree INCLUDE indexes
- Parallel index creation not implemented
- Concurrent index creation not supported
- Index reindexing (REINDEX) not exposed in V2 parser

### Stubbed Features

**Materialized View Refresh:**
- REFRESH MATERIALIZED VIEW command not parsed in V2
- Materialized views can be created but not refreshed through SQL
- Spec defines refresh behavior but parser doesn't expose it
- Spec reference: `/docs/specifications/ddl/DDL_VIEWS.md`

**Tablespace Commands:**
- CREATE TABLESPACE not parsed in V2
- ALTER TABLESPACE supports RENAME TO / SET SCHEMA (generic rename/move only)
- DROP TABLESPACE not parsed
- TABLESPACE clause in CREATE INDEX is parsed and enforced (errors if missing)
- Spec reference: `/docs/specifications/storage/TABLESPACE_SPECIFICATION.md`

### Missing Features

**View Features:**
- Updatable view detection incomplete
- INSTEAD OF triggers for views not implemented
- Security barrier views not supported
- Recursive views (WITH RECURSIVE) not in V2 parser

**Index Features:**
- Expression index validation limited
- Partial index predicate optimization incomplete
- Index usage in query plans needs improvement
- Functional/expression indexes parsed but optimization limited

**Sequence Features:**
- ALTER SEQUENCE OWNED BY may not be fully wired
- Distributed sequence generation not supported

### Spec Deltas

**Index Creation:**
- Index type validation incomplete
- Some index options parsed but ignored
- Spec reference: `/docs/specifications/indexes/AdvancedIndexes.md`

**Materialized Views:**
- Refresh behavior spec-defined but command not exposed
- Concurrent refresh not supported
- Materialized view indexes not automatically maintained
- Spec reference: `/docs/specifications/ddl/DDL_VIEWS.md`

**Sequences:**
- Full OWNED BY semantics may not be complete
- Sequence cache behavior in distributed scenarios undefined
- Spec reference: `/docs/specifications/ddl/DDL_SEQUENCES.md`

### ScratchBird/docs/audit/languages/native/04_types_and_domains.md
### Partial Implementation

**Domain Kinds:**
- RECORD domains: Implemented (serialization needs validation)
- SET domains: Implemented (serialization needs validation)
- VARIANT domains: Implemented (serialization needs validation)
- Spec reference: `/docs/specifications/types/03_TYPES_AND_DOMAINS.md`

### Missing Features

**CREATE TYPE Statement:**
- Separate CREATE TYPE not supported in V2 parser
- All custom types must be created as domains
- PostgreSQL-style CREATE TYPE AS ENUM not parsed
- Must use CREATE DOMAIN AS ENUM instead
- Spec reference: `/docs/specifications/types/03_TYPES_AND_DOMAINS.md`

**Domain Features:**
- COLLATE clause not fully supported for string domains
- Domain inheritance not supported
- Array domains (domain arrays) may have limitations

**Advanced Type Features:**
- Composite type constructors limited
- ROW type expressions partial
- Type resolution in complex expressions may have edge cases
- Custom type input/output functions not supported

### Spec Deltas

**Record/Variant Serialization:**
- Implementation exists but full serialization needs validation
- SBLR bytecode encoding for complex domains needs testing
- Cross-session persistence of complex domain values needs verification
- Spec reference: `/docs/specifications/sblr/SBLR_DOMAIN_PAYLOADS.md` (if exists)

**Domain Constraints:**
- Multi-column CHECK constraints not supported in domains
- CHECK constraint expression complexity may have limits
- Constraint evaluation order not specified
- Spec reference: `/docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md`

**Type Compatibility:**
- SET COMPAT/DROP COMPAT functionality not fully documented
- Compatibility modes for dialect emulation not complete
- Type coercion rules between domains need clarification

### ScratchBird/docs/audit/languages/native/05_programmable_sql.md
### Missing Features

**All Programmable SQL:**
- CREATE FUNCTION not parsed in V2
- CREATE PROCEDURE not parsed in V2
- CREATE TRIGGER not parsed in V2
- EXECUTE BLOCK not parsed in V2
- EXECUTE PROCEDURE not parsed
- EXECUTE STATEMENT not parsed
- All procedural statements (IF, WHILE, FOR, etc.) not parsed
- Variable declarations not supported
- Exception handling not implemented
- Spec references:
  - `/docs/specifications/ddl/DDL_FUNCTIONS.md`
  - `/docs/specifications/ddl/DDL_PROCEDURES.md`
  - `/docs/specifications/ddl/DDL_TRIGGERS.md`

**Parser Status:**
- AST nodes defined in `ast_v2.h` for functions, procedures, triggers
- Parser has TODO comments indicating planned implementation
- No bytecode generation or executor support currently
- Critical finding documented in `/docs/audit/parsers/CRITICAL_FINDINGS.md`

**Alternative Approaches:**
- Use multiple SQL statements instead of procedures
- Implement logic in application layer
- Use emulated database procedures (Firebird, PostgreSQL, etc.) via CREATE DATABASE EMULATED

### ScratchBird/docs/audit/languages/native/06_dml_select.md
### Missing Features

**Common Table Expressions (WITH):**
- WITH (CTE) syntax not parsed in V2
- WITH RECURSIVE not supported
- Named subqueries not available
- Workaround: Use subqueries or views
- Spec reference: `/docs/specifications/dml/01_SELECT.md`

**Advanced Join Features:**
- NATURAL JOIN not supported
- USING clause not supported (use ON instead)
- Some join type combinations may have limitations

**Window Function Features:**
- Some advanced window frame specifications may be limited
- RANGE frames vs ROWS frames need validation
- GROUPS frame type not supported

**Query Hints:**
- Optimizer hints not supported
- Index hints not available
- Join order hints not parsed

### Partial Implementation

**Set Operations:**
- UNION, INTERSECT, EXCEPT implemented
- ALL vs DISTINCT behavior needs validation
- Complex nested set operations may have limitations

**Locking Clauses:**
- FOR UPDATE/FOR SHARE parsed
- NOWAIT and SKIP LOCKED options parsed
- Full lock semantics with MGA transactions need validation
- Spec reference: `/docs/specifications/transaction/TRANSACTION_MAIN.md`

### Spec Deltas

**SELECT Features:**
- Some PostgreSQL-style extensions are present (FROM in UPDATE/DELETE)
- These may need explicit spec approval or documentation
- Query optimizer coverage incomplete
- Spec reference: `/docs/specifications/query/QUERY_OPTIMIZER_SPEC.md`

### ScratchBird/docs/audit/languages/native/09_security_dcl.md
Status flags:
- ## CREATE ROLE / CREATE USER: Status: Missing.

### ScratchBird/docs/audit/languages/native/11_utilities.md
Status flags:
- ## COPY: Status: Partial.
- ## ANALYZE (standalone): Status: Missing.
- ## DESCRIBE / SHOW CREATE DATABASE: Status: Missing.

### ScratchBird/docs/audit/languages/native/13_system_catalog.md
Status flags:
- ## Implementation status: Status: Partial.
- ## Implementation status: tables as not implemented; validate current catalog population against spec.

## firebirdsql

### ScratchBird/docs/audit/languages/firebirdsql/01_databases_and_schemas.md
### Partial Implementation

**ALTER DATABASE**
- Only ALIAS ADD and ALIAS DROP are supported
- Other Firebird ALTER DATABASE operations (OWNER, RENAME, SET DEFAULT CHARACTER SET, etc.) are not implemented
- Parser will generate errors for unsupported clauses

### Missing Features

**Schema Support**
- CREATE SCHEMA, ALTER SCHEMA, DROP SCHEMA are not available (by dialect design - Firebird doesn't support schemas)
- Schema-qualified object names (schema.table) are not supported in Firebird emulation mode

**ALTER DATABASE Extended Operations**
- Cannot change database OWNER
- Cannot RENAME database
- Cannot modify DEFAULT CHARACTER SET after creation
- Cannot add/drop DIFFERENCE FILE
- Cannot BEGIN/END BACKUP operations
- Cannot modify database-level SET options

### ScratchBird/docs/audit/languages/firebirdsql/02_tables_and_constraints.md
### Partial Implementation

**CREATE TABLE**
- Temporary table behavior (ON COMMIT DELETE/PRESERVE ROWS) is parsed but not fully enforced by the V2 executor
- Many constraint types are parsed but not fully validated during execution
- Computed columns (COMPUTED BY) are parsed but implementation is incomplete
- Identity columns (GENERATED AS IDENTITY) are parsed but may not work as expected
- Many column and table options are accepted by the parser but ignored by the V2 pipeline

**ALTER TABLE**
- Only basic operations supported: ADD COLUMN, DROP COLUMN, ALTER COLUMN TO (rename), ADD/DROP CONSTRAINT
- Cannot modify column data types
- Cannot modify column defaults after creation (SET DEFAULT, DROP DEFAULT not parsed)
- Cannot modify NOT NULL constraints after creation
- Cannot rename table (RENAME TO not supported)
- Many Firebird ALTER TABLE variants are not parsed and will cause errors

### Stubbed Implementation

**DROP TABLE**
- Parser accepts DROP TABLE and generates AST
- Bytecode payload format does not match executor expectations
- May result in errors or incomplete execution
- Status: Implementation mismatch between parser/semantic/executor

### Missing Features

**TRUNCATE TABLE**
- Not available by dialect design (Firebird doesn't support TRUNCATE)
- Use `DELETE FROM table_name` instead

**ALTER TABLE Extended Operations**
- ALTER COLUMN SET DATA TYPE - cannot change column type
- ALTER COLUMN SET DEFAULT - cannot modify defaults
- ALTER COLUMN DROP DEFAULT - cannot remove defaults
- ALTER COLUMN SET NOT NULL - cannot add NOT NULL to existing column
- ALTER COLUMN DROP NOT NULL - cannot remove NOT NULL
- RENAME TO - cannot rename table
- ADD CONSTRAINT variants for advanced constraints

**CREATE TABLE Advanced Features**
- EXTERNAL FILE tables - not supported
- Full constraint enforcement in executor
- Complete temporary table isolation and cleanup
- Computed column full execution
- Generated column expressions (beyond basic IDENTITY)

### ScratchBird/docs/audit/languages/firebirdsql/03_indexes_views_sequences.md
### Stubbed Implementation

**CREATE INDEX**
- Parser accepts full syntax including UNIQUE, ASC/DESC, expression indexes, and partial indexes (WHERE clause)
- Semantic analysis and bytecode generation succeed
- **Bytecode format mismatch**: The bytecode payload doesn't match executor expectations
- May result in parsing success but execution failure
- Indexes may not actually be created in the database

**DROP INDEX**
- Parser accepts DROP INDEX statements
- **Bytecode format mismatch**: Similar executor compatibility issue as CREATE INDEX
- May not actually drop the index

**CREATE VIEW / RECREATE VIEW**
- Parser accepts CREATE/OR REPLACE/RECREATE VIEW with full SELECT syntax
- Supports column name lists and WITH CHECK OPTION
- **Bytecode format mismatch**: Executor doesn't understand the generated payload
- Views may not be created successfully

**DROP VIEW**
- Parser accepts DROP VIEW statements
- **Bytecode format mismatch**: Executor compatibility issue
- May not successfully drop views

### ScratchBird/docs/audit/languages/firebirdsql/04_types_and_domains.md
### Missing Features

**CREATE TYPE**
- Not available by dialect design (Firebird doesn't support CREATE TYPE)
- Use CREATE DOMAIN instead for type aliases
- Workaround: Use domains with constraints for enumeration-like behavior

**Extended Domain Types**
- ENUM domains - not supported in Firebird emulation (ScratchBird-native only)
- RECORD domains - not supported in Firebird emulation
- SET domains - not supported in Firebird emulation
- VARIANT domains - not supported in Firebird emulation
- These extended types are documented in `/home/dcalford/CliWork/ScratchBird/docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md` but are exclusive to ScratchBird's native V2 parser

### ScratchBird/docs/audit/languages/firebirdsql/05_programmable_sql.md
### Stubbed (Parsed But Not Executed)

**PSQL Language Constructs**
- BEGIN...END, IF, WHILE, FOR - Parser creates AST nodes
- SemanticAnalyzerV2 rejects these nodes
- BytecodeGeneratorV2 cannot generate code for PSQL
- Executor has no support for procedural execution

### ScratchBird/docs/audit/languages/firebirdsql/06_dml_select.md
Status flags:
- ## SELECT: Status: Partial.

### ScratchBird/docs/audit/languages/firebirdsql/07_dml_modification.md
Status flags:
- ## INSERT: Status: Partial.
- ## UPDATE: Status: Partial (V2 pipeline limitations).
- ## DELETE: Status: Partial (V2 pipeline limitations).
- ## UPDATE OR INSERT: Status: Stubbed.
- ## UPDATE OR INSERT: Spec delta: Compiled as INSERT only; UPDATE path not implemented.
- ## MERGE: Description: Not implemented in Firebird parser.
- ## MERGE: Status: Missing.

### ScratchBird/docs/audit/languages/firebirdsql/09_security_dcl.md
Status flags:
- ## GRANT / REVOKE: Status: Missing.
- ## CREATE/ALTER/DROP ROLE, USER: Status: Missing.

### ScratchBird/docs/audit/languages/firebirdsql/10_session_show_set.md
Status flags:
- ## SHOW commands: Status: Missing.
- ## SET commands: Status: Missing (except SET TRANSACTION handled as TCL).

### ScratchBird/docs/audit/languages/firebirdsql/11_utilities.md
Status flags:
- ## EXPLAIN / PLAN: Status: Missing.
- ## COPY / DESCRIBE / COMMENT: Description: Not implemented in Firebird parser.
- ## COPY / DESCRIBE / COMMENT: Status: Missing.

### ScratchBird/docs/audit/languages/firebirdsql/13_system_catalog.md
Status flags:
- ## Implementation status: Status: Partial.

### ScratchBird/docs/audit/languages/firebirdsql/14_functions.md
Status flags:
- ## Status Notes: `RDB$GET_CONTEXT`, and `DATEADD` are not implemented in the current parser

## postgresql

### ScratchBird/docs/audit/languages/postgresql/01_databases_and_schemas.md
### Partial Implementation

⚠️ **WITH Options** - Most CREATE DATABASE WITH options (TEMPLATE, ENCODING, LOCALE, etc.) are parsed for compatibility but not fully enforced at the storage layer. The database is created as an emulated namespace regardless of these parameters.

⚠️ **ALTER DATABASE Configuration** - SET/RESET configuration parameters are parsed but parameter validation is limited. Not all PostgreSQL configuration parameters are supported.

### Spec Deltas

📝 **Physical Separation** - Unlike native PostgreSQL where each database is a separate physical entity, ScratchBird creates logical namespaces within the emulation layer. Databases share the same underlying storage engine.

📝 **Connection Model** - Database-level connection limits and other connection parameters are parsed but may not affect actual connection handling, which is managed at the ScratchBird server level.

📝 **Templates** - Template database copying is not implemented. The TEMPLATE option is accepted but ignored.


### ScratchBird/docs/audit/languages/postgresql/02_tables_and_constraints.md
### Stubbed Implementation

🚧 **CREATE TABLE** - Parser accepts full PostgreSQL syntax but executor bytecode format mismatch prevents execution. Parser emits IF NOT EXISTS byte and column/constraint format that executor doesn't expect.

🚧 **ALTER TABLE** - Parser emits legacy ALTER_TABLE format not compatible with current executor implementation.

🚧 **DROP TABLE** - Parser emits TABLE_REF lists and extra flags that don't match executor expectations (expects name string + flags).

🚧 **TRUNCATE TABLE** - Parser emits TABLE_REF list + flags not read by executor.

### Partial Implementation



⚠️ **Storage Parameters** - WITH options are parsed but may not affect actual storage behavior.


### Missing Features

❌ **EXCLUDE Constraints** - Parsed but not implemented in executor.

❌ **Tablespaces** - TABLESPACE option is parsed but tablespace management is not implemented.

❌ **ON COMMIT for Temp Tables** - Parsed but behavior may not be fully enforced.

### Spec Deltas

📝 **Bytecode Format** - The PostgreSQL parser emits SBLR bytecode with a different structure than the executor expects. CREATE TABLE includes an extra IF NOT EXISTS byte, and column definitions lack the COLUMN_REF qualifier that the executor expects. This prevents table creation from working end-to-end.

📝 **Constraint Handling** - Table constraints are emitted as inline opcodes (PRIMARY_KEY, UNIQUE_CONSTRAINT, TABLE_FK) within the column list, but executor expects a different format.


### ScratchBird/docs/audit/languages/postgresql/03_indexes_views_sequences.md
### Stubbed Implementation

🚧 **CREATE INDEX** - Parser accepts full syntax but bytecode format mismatch prevents execution. Parser emits different name/table/flags ordering than executor expects.

🚧 **DROP INDEX** - Parser emits different format than executor expects.

🚧 **CREATE VIEW** - Parser emits SELECT bytecode, but executor expects SQL string and flags. Bytecode structure mismatch prevents execution.

🚧 **DROP VIEW** - Stubbed due to payload mismatch.

🚧 **CREATE MATERIALIZED VIEW** - Uses CREATE_VIEW opcode with materialized flag but format doesn't match executor expectations.

🚧 **CREATE SEQUENCE** - Parser drops sequence options during emission; executor expects different payload structure.

🚧 **ALTER SEQUENCE** - Bytecode payload mismatch.

🚧 **DROP SEQUENCE** - Bytecode payload mismatch.

### Missing Features

❌ **CONCURRENTLY for Indexes** - CONCURRENTLY flag is parsed but concurrent index building is not implemented.

❌ **Advanced Index Types** - GiST, SP-GiST, GIN, BRIN parsing is supported but execution depends on index subsystem implementation.

❌ **REFRESH MATERIALIZED VIEW** - Not yet implemented in parser.

### Spec Deltas

📝 **View Definitions** - The parser emits compiled SELECT bytecode instead of storing the original SQL string. The executor expects a SQL string that it can re-execute. This prevents views from working end-to-end.

📝 **Sequence Options** - Parser parses all sequence options (START, INCREMENT, MINVALUE, etc.) but doesn't emit them in bytecode. Only sequence name and flags are emitted.

📝 **Index Methods** - USING clause for index methods is parsed but the specific index type (hash, gin, etc.) may not be fully supported by the storage engine.


### ScratchBird/docs/audit/languages/postgresql/04_types_and_domains.md
### Partial Implementation



⚠️ **DROP TYPE** - Partially supported.

### Spec Deltas

📝 **Type Mapping** - PostgreSQL CREATE TYPE for ENUMs and composite types is mapped to ScratchBird domain payloads. This provides compatibility but may have subtle behavioral differences from native PostgreSQL types.

📝 **Range Types** - Range type syntax is parsed but the full range type semantics (range operators, functions, etc.) depend on executor support.


### ScratchBird/docs/audit/languages/postgresql/06_dml_select.md
Status flags:
- ## SELECT: Status: Stubbed.
- ## WITH (CTE): Status: Stubbed (payload mismatch for EXT_WITH_CLAUSE).

### ScratchBird/docs/audit/languages/postgresql/07_dml_modification.md
Status flags:
- ## INSERT: Status: Stubbed (bytecode mismatch; RETURNING/ON CONFLICT unsupported by executor).
- ## UPDATE: Status: Stubbed (payload mismatch).
- ## DELETE: Status: Stubbed (payload mismatch).
- ## MERGE: Status: Stubbed.

### ScratchBird/docs/audit/languages/postgresql/09_security_dcl.md
Status flags:
- ## CREATE USER: Status: Stubbed.
- ## GRANT / REVOKE: Status: Stubbed (executor expects EXT_GRANT_PRIVILEGE/EXT_REVOKE_PRIVILEGE).

### ScratchBird/docs/audit/languages/postgresql/10_session_show_set.md
Status flags:
- ## SHOW commands: Status: Stubbed.
- ## SHOW commands: - PostgreSQL-style SHOW DATABASE/SHOW SCHEMA are not implemented; use catalog

### ScratchBird/docs/audit/languages/postgresql/11_utilities.md
Status flags:
- ## COPY: Status: Partial (file COPY handled in executor; protocol COPY streams handled by adapter).

### ScratchBird/docs/audit/languages/postgresql/13_system_catalog.md
Status flags:
- ## Implementation status: Status: Partial.

## mysql

### ScratchBird/docs/audit/languages/mysql/01_databases_and_schemas.md
### Partial Implementation

- **ALTER DATABASE RENAME**: The syntax `ALTER DATABASE old_name RENAME TO new_name` is parsed but currently rejected with an error message. This feature is not yet implemented.

### Missing Features

- **Database-level encryption options**: MySQL 8.0's `ENCRYPTION` clause is not supported
- **Read-only databases**: MySQL 8.0's `READ ONLY` option is not supported
- **Other ALTER DATABASE options**: Advanced MySQL 8.0 options like default table encryption are not supported

### Spec Deltas

- **Character set/collation handling**: While character sets and collations are stored in metadata, the full collation behavior may differ from MySQL in some edge cases
- **Database naming restrictions**: ScratchBird's identifier rules may differ slightly from MySQL's naming conventions

### ScratchBird/docs/audit/languages/mysql/02_tables_and_constraints.md
### Stubbed Implementation

- **CREATE TABLE bytecode mismatch**: The parser generates bytecode that does not fully match the executor's expectations. While basic table creation works, complex constraints and options may not be properly processed.
  - Column constraint encoding may not match executor format
  - Table constraint encoding may have format mismatches
  - Some constraint combinations may not work as expected

### Partial Implementation

- **ALTER TABLE - Rename Only**: Only the `RENAME TO` clause is implemented. All other ALTER TABLE operations (ADD COLUMN, DROP COLUMN, MODIFY COLUMN, etc.) are rejected by the parser.

### Missing Features

- **DROP TABLE**: Not implemented in the parser at all. Will cause parse errors.

- **TRUNCATE TABLE**: Not implemented in the parser at all. Will cause parse errors.

- **UNSIGNED and ZEROFILL type modifiers**: These are parsed and stored internally but NOT emitted to bytecode. Constraints are not enforced:
  ```sql
  CREATE TABLE test (
      id INT UNSIGNED,           -- Parsed but not enforced
      display INT ZEROFILL       -- Parsed but not enforced
  );
  ```
  - `UNSIGNED` should generate `CHECK (column >= 0)` constraints
  - `ZEROFILL` should add display formatting metadata

- **Advanced Index Types in CREATE TABLE**: Index types like `FULLTEXT`, `SPATIAL`, and index hints are parsed but may not be correctly emitted:
  ```sql
  CREATE TABLE articles (
      id INT PRIMARY KEY,
      content TEXT,
      FULLTEXT INDEX idx_content (content)  -- Parsed but may not work
  );
  ```

- **Foreign Key Options**: While basic foreign key constraints are supported, advanced options may not work:
  - `MATCH FULL`, `MATCH PARTIAL`, `MATCH SIMPLE`
  - Some referential action combinations

- **CREATE TABLE ... AS SELECT**: Not currently supported:
  ```sql
  CREATE TABLE new_table AS SELECT * FROM old_table;  -- Not supported
  ```

- **CREATE TABLE ... LIKE**: Not currently supported:
  ```sql
  CREATE TABLE new_table LIKE old_table;  -- Not supported
  ```

- **Partitioning**: All partitioning clauses are not supported:
  ```sql
  CREATE TABLE sales (
      id INT,
      sale_date DATE
  ) PARTITION BY RANGE (YEAR(sale_date)) ...;  -- Not supported
  ```

### Spec Deltas

- **INSERT modifiers**: Modifiers like `LOW_PRIORITY`, `HIGH_PRIORITY`, `DELAYED`, and `IGNORE` in CREATE TABLE context are not supported

- **Storage engine specifics**: ENGINE options are parsed but ScratchBird uses its own storage engine. Engine-specific features (MyISAM, InnoDB, MEMORY) do not apply

- **Character set/collation inheritance**: While these can be specified, the actual collation behavior may differ from MySQL

### ScratchBird/docs/audit/languages/mysql/03_indexes_views_sequences.md
### Missing Features

- **CREATE INDEX**: Standalone CREATE INDEX statements are not implemented. Parse errors will occur.
  - **Workaround**: Define indexes inline within CREATE TABLE statements
  - **Priority**: CRITICAL for Alpha release

- **DROP INDEX**: Not implemented. Cannot drop indexes via SQL.
  - **Workaround**: Recreate table without the index
  - **Priority**: CRITICAL for Alpha release

- **CREATE VIEW**: Not implemented. View creation will fail with parse errors.
  - **Workaround**: Use application-level abstraction or subqueries
  - **Priority**: CRITICAL for Alpha release

- **DROP VIEW**: Not implemented. Cannot drop views via SQL.
  - **Priority**: CRITICAL for Alpha release

- **ALTER VIEW**: Not implemented.
  - **Priority**: Medium

### Stubbed Features

- **Index types in CREATE TABLE**: While index definitions can be included in CREATE TABLE, advanced index types (FULLTEXT, SPATIAL) may not be properly emitted to bytecode:
  ```sql
  CREATE TABLE articles (
      id INT PRIMARY KEY,
      content TEXT,
      FULLTEXT INDEX idx_content (content)  -- Parsed but may not work
  );
  ```

### Spec Deltas

- **No standalone SEQUENCE objects**: By design, MySQL does not support CREATE SEQUENCE. Use AUTO_INCREMENT instead.

- **Index algorithm hints**: USING BTREE, USING HASH, and other algorithm hints are parsed but may not affect actual index implementation (ScratchBird uses its own index structures)

- **View algorithms**: ALGORITHM = {MERGE | TEMPTABLE} hints are not supported

### ScratchBird/docs/audit/languages/mysql/04_types_and_domains.md
### Missing Features

- **CREATE DOMAIN**: Not supported by MySQL dialect (by design)
  - **Workaround**: Use CHECK constraints on individual columns

- **ALTER DOMAIN**: Not supported by MySQL dialect (by design)

- **DROP DOMAIN**: Not supported by MySQL dialect (by design)

- **CREATE TYPE**: Not supported by MySQL dialect (by design)
  - **Workaround**: Use ENUM or SET types for custom value sets

### Partial Implementation

- **UNSIGNED modifier**: Parsed and stored internally but NOT emitted to bytecode
  - Constraint is not enforced
  - Should generate `CHECK (column >= 0)` constraint
  - **Status**: Requires implementation

- **ZEROFILL modifier**: Parsed but NOT implemented
  - Display formatting is not applied
  - Should add formatting metadata to catalog
  - **Status**: Requires implementation

### Spec Deltas

- **Type storage**: ScratchBird may use different internal representations than MySQL
- **Type limits**: Some type size limits may differ from MySQL
- **Collation behavior**: Collation support may not be complete for all character sets

### ScratchBird/docs/audit/languages/mysql/05_programmable_sql.md
### Missing Features

- **CREATE PROCEDURE**: Not implemented in the parser
  - **Priority**: High (Beta target)
  - **Workaround**: Use application-level logic

- **CREATE FUNCTION**: Not implemented in the parser
  - **Priority**: High (Beta target)
  - **Workaround**: Use application-level functions or inline expressions

- **CREATE TRIGGER**: Not implemented in the parser
  - **Priority**: High (Beta target)
  - **Workaround**: Implement trigger logic in application code

- **CALL statement**: Not implemented
  - **Priority**: High (Beta target)

- **EXECUTE/PREPARE**: Dynamic SQL not implemented
  - **Priority**: Medium (Beta target)

- **All procedural constructs**: Variables, control flow, cursors, error handlers
  - **Priority**: High (Beta target)
  - **Workaround**: Use application-level logic

### Spec Deltas

- **Procedural language**: MySQL's procedural SQL differs from other databases
- **Trigger timing**: BEFORE/AFTER semantics must match MySQL behavior
- **Function determinism**: Deterministic vs non-deterministic function behavior

### ScratchBird/docs/audit/languages/mysql/06_dml_select.md
### Stubbed Implementation

- **Bytecode format mismatch**: The MySQL parser generates SELECT bytecode that does not match the executor's expectations
  - DISTINCT flag encoding differs
  - Alias string encoding differs
  - Join clause encoding may not work correctly
  - **Impact**: SELECT statements parse correctly but may fail at execution time

### Partial Implementation

- **Complex queries may fail**: While simple SELECT statements work, complex queries with:
  - Multiple joins
  - Subqueries
  - Window functions
  - CTEs (Common Table Expressions)

  May not execute correctly due to bytecode mismatches

### Missing Features

- **Window functions**: Not supported in MySQL parser
  ```sql
  SELECT
      name,
      salary,
      ROW_NUMBER() OVER (ORDER BY salary DESC) AS rank  -- NOT SUPPORTED
  FROM employees;
  ```

- **Common Table Expressions (WITH)**: Not supported
  ```sql
  WITH high_value AS (
      SELECT * FROM orders WHERE total > 1000
  )
  SELECT * FROM high_value;  -- NOT SUPPORTED
  ```

- **SELECT INTO OUTFILE**: Not supported
  ```sql
  SELECT * INTO OUTFILE '/tmp/result.txt' FROM users;  -- NOT SUPPORTED
  ```


###Spec Deltas

- **Join algorithm hints**: MySQL join hints (STRAIGHT_JOIN priority) may not affect execution
- **Query optimizer hints**: Optimizer hints are not supported
- **Full-text search**: MATCH ... AGAINST not implemented

### ScratchBird/docs/audit/languages/mysql/07_dml_modification.md
### Stubbed Implementation

- **INSERT bytecode mismatch**: Multi-row INSERT and column qualifier encoding doesn't match executor
- **UPDATE bytecode mismatch**: Table list, alias, and qualified column references differ
- **DELETE bytecode mismatch**: Alias and ORDER BY/LIMIT encoding differs
- **REPLACE bytecode mismatch**: Encoded as unsupported EXT_ON_CONFLICT_DO_UPDATE

### Partial Implementation

- **ON DUPLICATE KEY UPDATE - CRITICAL ISSUE**: Parsed but bytecode emission is disabled
  - Should be remapped to MERGE statement
  - Currently does nothing
  - **Priority**: Alpha blocker

- **REPLACE semantics differ**: Maps to ON CONFLICT (UPDATE) instead of DELETE + INSERT
  - Different trigger behavior
  - Different OID behavior
  - **Priority**: Low (document difference)

### Missing Features

- **Multi-table UPDATE**: Not supported
  ```sql
  UPDATE t1, t2 SET t1.col = t2.col WHERE ...;  -- NOT SUPPORTED
  ```

- **Multi-table DELETE**: Not supported
  ```sql
  DELETE t1, t2 FROM t1 JOIN t2 WHERE ...;  -- NOT SUPPORTED
  ```

- **INSERT modifiers**: LOW_PRIORITY, HIGH_PRIORITY, DELAYED, IGNORE
  - Parsed but no implementation
  - **IGNORE** should map to ON CONFLICT DO NOTHING

### ScratchBird/docs/audit/languages/mysql/09_security_dcl.md
Status flags:
- ## GRANT / REVOKE: Description: MySQL privilege statements are not implemented in the parser.
- ## GRANT / REVOKE: Status: Missing.
- ## CREATE USER / ALTER USER / DROP USER: Description: Not implemented in MySQL parser.
- ## CREATE USER / ALTER USER / DROP USER: Status: Missing.

### ScratchBird/docs/audit/languages/mysql/11_utilities.md
Status flags:
- ## LOCK TABLES / UNLOCK TABLES: Status: Partial (no-op).
- ## EXPLAIN / ANALYZE / COPY: Description: Not implemented in MySQL parser.
- ## EXPLAIN / ANALYZE / COPY: Status: Missing.

### ScratchBird/docs/audit/languages/mysql/13_system_catalog.md
Status flags:
- ## Implementation status: Status: Partial.

### ScratchBird/docs/audit/languages/mysql/14_functions.md
Status flags:
- ## Status Notes: helpers like `DATE_ADD()` are not implemented in the parser.

## Cross-cutting Checklists (unchecked items)

### ScratchBird/docs/audit/19_postgresql_parser_correction_plan_checklist.md
- [ ] CREATE TABLE: move table-level constraints out of column list or teach executor to accept PRIMARY_KEY/UNIQUE/TABLE_FK opcodes inside column list.
- [ ] MERGE: add executor handling for EXT_MERGE_* or drop support in parser.
- [ ] SHOW ALL / SHOW VARIABLE / SHOW TRANSACTION LEVEL: executor has no EXT_SHOW_* handlers for these PostgreSQL opcodes.
- [ ] CREATE TABLE: WITH options and TABLESPACE are parsed but not emitted.
- [ ] CREATE INDEX: expression indexes are parsed but not emitted; INCLUDE and WHERE are parsed but only partially emitted.
- [ ] INSERT/UPDATE/DELETE: RETURNING and ON CONFLICT are emitted but executor has no support.
- [ ] Decide whether PostgreSQL parser should target executor (current) format or move executor to accept parser’s richer format.
- [ ] Add bytecode versioning or dialect flags if multiple payload formats must coexist.

### ScratchBird/docs/audit/20_mysql_parser_correction_plan_checklist.md
- [ ] REPLACE: encoded with legacy payload; still needs ON CONFLICT alignment and semantics.
- [ ] ALTER TABLE supports only RENAME; other ALTER actions are rejected.
- [ ] CREATE TABLE: many table options are parsed for syntax but not emitted.
- [ ] Decide whether MySQL parser should target executor (current) format or update executor to accept current parser payloads.
- [ ] Add dialect-specific bytecode versioning if multiple formats will coexist.

### ScratchBird/docs/audit/22_firebird_parser_correction_plan_checklist.md
- [ ] CREATE INDEX: V2 bytecode uses UUIDs/method/column indexes; executor expects name/table/column names.
- [ ] CREATE VIEW: V2 bytecode writes schema UUID + SBLR query; executor expects SQL definition string and flags layout.
- [ ] DROP TABLE/INDEX/VIEW: V2 bytecode writes UUID list; executor expects single name string + flags.
- [ ] TRUNCATE TABLE (V2): emits flags + UUID list; executor expects string + flags.
- [ ] CREATE TABLE FK payload: V2 writes TABLE_FK after tablespace with parent table placeholder; executor reads TABLE_FK before tablespace and expects parent table name strings.
- [ ] CREATE PROCEDURE / FUNCTION / TRIGGER / PACKAGE / ROLE / EXCEPTION: parser emits errors.
- [ ] ALTER INDEX: parser emits error.
- [ ] DROP SEQUENCE / GENERATOR: parser emits error.
- [ ] MERGE: parser emits error.
- [ ] EXECUTE PROCEDURE / EXECUTE STATEMENT: parser emits error.
- [ ] FOR EXECUTE STATEMENT / LOOP: parser emits error.
- [ ] SET / SHOW / GRANT / REVOKE / COMMENT: parser emits error.
- [ ] Firebird ISQL SHOW variants (SHOW TABLE/INDEX/TRIGGER/VIEW/PROCEDURE/FUNCTION/DOMAIN/GENERATOR/SCHEMA/ROLE/GRANTS/CHECKS/COLLATIONS/COMMENTS/DEPENDENCIES/PACKAGE/SYSTEM/SQL_DIALECT/VERSION/DATABASE): not parsed; executor expects opcode + payload string or no payload depending on variant.
- [ ] PSQL statements (BEGIN...END, IF/WHILE/FOR, SUSPEND, etc.) are parsed but not supported by SemanticAnalyzerV2/BytecodeGeneratorV2.
- [ ] UPDATE OR INSERT compiles as INSERT only; UPDATE semantics not implemented.
- [ ] CREATE SEQUENCE/GENERATOR is parsed but rejected by semantic analyzer.
- [ ] CREATE TABLE: OR REPLACE/OR ALTER, TEMPORARY/ON COMMIT, PK/UNIQUE/FK/CHECK/GENERATED/COMPUTED constraints not enforced by executor.
- [ ] CREATE INDEX: expression indexes and WHERE clause parsed but not executed due to bytecode mismatch.
- [ ] CREATE VIEW: WITH CHECK OPTION parsed but not executed due to bytecode mismatch.
- [ ] Schema-qualified names are rejected; parser logs errors on dotted identifiers.
- [ ] Decide whether to align V2 bytecode with executor or extend executor to support V2 formats.
- [ ] Implement PSQL pipeline or explicitly disable PSQL in Firebird parser to avoid false-positive coverage.

### ScratchBird/docs/audit/26_show_set_correction_plan_checklist.md
- [ ] Add executor handlers for EXT_SHOW_VARIABLE, EXT_SHOW_ALL, EXT_SHOW_TRANSACTION_LEVEL or stop emitting them from V2.
- [ ] Fix SET ROLE payload: generator should emit flags byte (bit0=reset) + role string; executor currently expects flags.
- [ ] Fix SET SESSION AUTHORIZATION payload: generator should emit flags byte (bit0=reset) + user string; executor expects flags.
- [ ] Add bytecode emission for SET TIME ZONE and executor storage (no opcode today; decide on EXT_SET_TIME_ZONE or explicit variable semantics).
- [ ] Implement SET CONSTRAINTS parsing and bytecode emission (EXT_SET_CONSTRAINTS exists and executor implements it).
- [ ] Decide canonical SET SCHEMA syntax (BNF vs Master Grammar) and accept aliases (SET CURRENT SCHEMA, SET SCHEMA TO/=).
- [ ] Decide canonical SET SEARCH_PATH syntax (SEARCH_PATH vs SEARCH PATH) and accept aliases.
- [ ] Implement SET SESSION CHARACTERISTICS AS TRANSACTION as alias of SET TRANSACTION or remove from spec.
- [ ] Implement DESCRIBE/DESC (alias for SHOW COLUMNS) or remove from grammar.
- [ ] Require names where executor requires them (SHOW TRIGGER/VIEW/PROCEDURE/FUNCTION/DOMAIN/GENERATOR/ROLE/CHECKS).
- [ ] Implement SHOW SEARCH_PATH, SHOW TIME ZONE, SHOW ALL (session-control spec) using schema-navigation opcodes or variable show.
- [ ] Decide support for SHOW VARIABLES/STATUS/WARNINGS/ERRORS/PROCESSLIST and SHOW CREATE DATABASE (BNF).
- [ ] Decide support for MySQL user variables (SET @var, SELECT INTO @var) or remove from BNF.
- [ ] Decide support for SHOW SCHEMAS and map to SHOW DATABASES (list schemas) or implement separate.
- [ ] Preserve SET SEARCH_PATH list values in semantic analyzer and emit BEGIN_LIST in bytecode; executor already supports list form.
- [ ] Encode SESSION/LOCAL scope in bytecode or remove scope from grammar if it will remain no-op.
- [ ] Define payloads for schema-navigation SHOW commands (SCHEMA PATH/TREE/SEARCH PATH/LOCATION/RESOLVED/OBJECTS) and add parser emission.
- [ ] Clarify behavior for SHOW INDEXES with no FROM (error vs list all) and align parser/executor accordingly.
- [ ] Decide whether generic SET should accept more variables beyond SEARCH_PATH or restrict grammar to SEARCH_PATH only.

### ScratchBird/docs/audit/31_operator_correction_plan_checklist.md
- [ ] Fix `||` concatenation encoding (BinaryOp::CONCAT is parsed but emitted as EXPR_ADD).
- [ ] Implement null-safe `IS DISTINCT FROM` / `IS NOT DISTINCT FROM` using EXT_NULL_SAFE_EQ.
- [ ] Replace unary NOT implementation with a boolean NOT opcode (preserve NULL).
- [ ] Add bitwise operators: `&`, `|`, `^`, `~`, `<<`, `>>` (parser + bytecode + executor mapping).
- [ ] Add array operators: `@>`, `<@`, `&&` (parser + bytecode; executor already supports).
- [ ] Add JSON existence operators: `?`, `?|`, `?&` (parser + bytecode mapping).
- [ ] Add `::` cast operator (parser only; bytecode/executor already support CAST).
- [ ] Decide and implement `^` (power) operator or document as unsupported (POWER() only).
- [ ] Implement Firebird `CONTAINING` and `STARTING WITH` match kinds in bytecode generation.
- [ ] Fix `NOT` to emit boolean NOT, not EXT_BIT_NOT.
- [ ] Fix `||` to emit string concatenation, not EXT_ARRAY_CAT.
- [ ] Add regex operators `~`, `~*`, `!~`, `!~*` if required by dialect.
- [ ] Implement EXT_ARRAY_SUBSCRIPT in executor or stop emitting it.
- [ ] Fix `NOT` to emit boolean NOT, not EXT_BIT_NOT.
- [ ] Fix `XOR` to emit boolean XOR semantics (not EXT_BIT_XOR).
- [ ] Implement NOT IN as a first-class comparison (current parse order treats NOT as unary).
- [ ] Fix `DIV` to emit integer division instead of modulo.
