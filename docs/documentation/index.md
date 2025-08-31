### ScratchBird User Documentation

**What it is**

ScratchBird is a comprehensive SQL database engine with procedural SQL (PSQL) support, offering a rich dialect that combines standard SQL features with advanced capabilities like foreign data wrappers, window functions, and stored procedures. This documentation provides a complete reference sourced directly from the codebase implementation.

**Why it matters**

- **For beginners**: Clear explanations of SQL concepts with runnable examples help you learn database programming from the ground up
- **For experienced developers**: Direct code anchors to the engine implementation (`src/engine/` and `include/scratchbird/engine/`) provide authoritative truth about feature behavior
- **For system integrators**: Detailed configuration, installation, and operational guides enable smooth deployment and management
- **For contributors**: Understanding the parser-to-runtime pipeline helps you extend the engine effectively

**How to use it**

1. **New to SQL?** Start with [SQL Overview](./sql-overview.md) to understand basic concepts, then explore [Data Types](./sql-data-types.md) and [SELECT](./sql-select.md) statements
2. **Building applications?** Focus on [DML](./sql-dml.md) for data manipulation, [Tables](./ddl-tables.md) and [Indexes](./ddl-indexes.md) for schema design
3. **Advanced features?** Dive into [PSQL Runtime](./psql-runtime.md) for stored procedures, [Foreign Data](./ddl-foreign-data.md) for external data access
4. **Deploying ScratchBird?** Review [Installation](./installation.md), [Configuration](./configuration.md), and [CLI Tools](./cli-tools.md)

**Key Features**

- **Full SQL Support**: SELECT with CTEs/RECURSIVE, JOIN trees, window functions, set operations
- **Rich DDL**: Tables, indexes, views, sequences, domains, schemas, and more
- **Procedural SQL**: EXECUTE BLOCK, stored procedures/functions, triggers, cursors, exception handling
- **Advanced Capabilities**: Foreign data wrappers, materialized views, row-level security, tablespaces
- **Developer Tools**: EXPLAIN/ANALYZE, syntax validation, dependency analysis, formatting

**Architecture Overview**

The engine follows a clear pipeline from SQL text to execution:
- **Lexical Analysis** (`include/scratchbird/engine/lexer.h`): Tokenizes SQL into identifiers, keywords, literals
- **Expression Parsing** (`include/scratchbird/engine/expr.h`): Handles operators, predicates, projections
- **AST Construction** (`include/scratchbird/engine/ast.h`): Builds abstract syntax trees for statements
- **Statement Routing** (`src/engine/parser.cpp`): Dispatches to specialized parsers (SELECT, DML, DDL, PSQL)
- **Execution** (`src/engine/psql_executor.cpp`): Runtime evaluation and data manipulation

**Documentation Structure**

This documentation is organized into logical sections that mirror how you'll use ScratchBird:

**Core SQL Language** - Foundation concepts and syntax:
- [Overview](./sql-overview.md) - SQL dialect overview, parser architecture, semantics vs parsing
- [Lexical Structure](./sql-lexical.md) - Tokens, identifiers, literals, comments, string forms
- [Operators](./sql-operators.md) - Operator precedence, associativity, runtime evaluation
- [Reserved Words](./sql-reserved-words.md) - Keywords, quoting rules, identifier handling
- [Data Types](./sql-data-types.md) - Complete type catalog: numeric, text, temporal, arrays

**Data Manipulation** - Querying and modifying data:
- [SELECT Statements](./sql-select.md) - WITH/RECURSIVE CTEs, JOINs, GROUP BY, window functions, set operations
- [DML Operations](./sql-dml.md) - INSERT, UPDATE, DELETE, MERGE, UPSERT with RETURNING clauses

**Data Definition (DDL)** - Schema and object management:

*Core Objects:*
- [Tables](./ddl-tables.md) - CREATE/ALTER/DROP/RECREATE tables, constraints, partitioning
- [Indexes](./ddl-indexes.md) - Index types (B-tree, hash, bitmap, GIN), expressions, partial indexes
- [Schemas](./ddl-schemas.md) - Namespace management, search paths
- [Views](./ddl-views.md) - Standard views, WITH CHECK OPTION

*Data Types and Sequences:*
- [Sequences](./ddl-sequences.md) - Auto-increment, cycle options, identity columns
- [Domains](./ddl-domains.md) - Custom types with constraints, defaults, collations
- [Collations & Charsets](./ddl-collations-charsets.md) - Text comparison and encoding rules

*Security and Access:*
- [Roles, Users, Grants](./ddl-roles-users-grants.md) - Authentication, authorization, privileges
- [Policies (RLS)](./ddl-policies-rls.md) - Row-level security with USING conditions
- [Exceptions & Comments](./ddl-exceptions-and-comments.md) - Error handling, documentation

*Advanced Features:*
- [Tablespaces](./ddl-tablespaces.md) - Storage management, LOCATION/FILE options
- [Foreign Data](./ddl-foreign-data.md) - External data access, FDW servers and mappings
- [Materialized Views](./ddl-materialized-views.md) - Cached query results, refresh strategies
- [Publication & Subscription](./ddl-publication-subscription.md) - Logical replication setup
- [Cluster](./ddl-cluster.md) - Multi-node configuration, services
- [Database Links](./ddl-database-links.md) - Cross-database queries
- [Blob Filter, Mapping, GTT](./ddl-blob-filter-mapping-gtt.md) - Binary data, mappings, global temp tables

**Procedural SQL and Control Flow**:
- [Session & Transaction](./session-and-transaction.md) - Database connections, transaction control, session settings
- [PSQL Runtime](./psql-runtime.md) - EXECUTE BLOCK, control structures (IF/WHILE/FOR), cursors, exceptions
- [Routines & Triggers](./psql-routines-and-triggers.md) - Stored procedures, functions, packages, event triggers
- [EXPLAIN / ANALYZE](./explain-analyze.md) - Query plans, performance analysis, execution statistics

**System Administration**:
- [Installation](./installation.md) - System requirements, installation steps, systemd integration
- [Configuration](./configuration.md) - Engine settings, SB_CONFIG parameters, tuning options
- [CLI Tools](./cli-tools.md) - dbcheck validation, dbspace management, command-line utilities
- [Developer Tools](./dev-tools.md) - Dependency analyzer, SQL formatter, profiler, syntax validator

**Reference**:
- [Missing and Future](./missing-and-future.md) - Known limitations, parser-only features, roadmap items

**Quick Examples**

```sql
-- Create a table with various data types
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10,2),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert data with RETURNING clause
INSERT INTO products (name, price) 
VALUES ('Widget', 19.99)
RETURNING id, created_at;

-- Complex SELECT with CTE and window function
WITH ranked_products AS (
    SELECT name, price,
           ROW_NUMBER() OVER (ORDER BY price DESC) as rank
    FROM products
    WHERE price > 10
)
SELECT * FROM ranked_products WHERE rank <= 5;

-- PSQL procedural block
EXECUTE BLOCK
RETURNS (product_count INTEGER)
AS
BEGIN
    SELECT COUNT(*) FROM products INTO :product_count;
    SUSPEND;
END
```

**Note**: This documentation is generated from the ScratchBird codebase. Each page includes direct references to source files for authoritative implementation details.
