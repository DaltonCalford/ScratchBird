[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native V2 SQL - Programmable SQL (PSQL)

## Overview

This document describes programmable SQL features in ScratchBird's Native V2 SQL dialect, including stored procedures, functions, triggers, and procedural blocks.

**Parser Pipeline:** V2 Parser → AST v2 → SemanticAnalyzerV2 → BytecodeGeneratorV2 → Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/parser_v2.cpp`
- AST: `/ScratchBird/include/scratchbird/parser/ast_v2.h`
- Semantic/Bytecode: `/ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `/ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## CREATE FUNCTION

### Description

Creates a stored function that returns a value. Functions can be used in SQL expressions and must return a value.

### Syntax

```sql
CREATE [OR REPLACE] FUNCTION <function_name> (
    [<parameter_name> [IN | OUT | INOUT] <parameter_type> [DEFAULT <value>] [, ...]]
)
RETURNS <return_type>
[DETERMINISTIC]
[SQL SECURITY {DEFINER | INVOKER}]
AS
BEGIN
    <function_body>
END
```

### Parameters

- **OR REPLACE**: Replaces existing function with same name
- **function_name**: Name of the function (schema-qualified paths supported)
- **IN / OUT / INOUT**: Parameter direction (IN is default)
- **DEFAULT**: Default value for parameter
- **DETERMINISTIC**: Marks function as producing same output for same input
- **SQL SECURITY**: Controls execution context (DEFINER runs as creator, INVOKER as caller)
- **return_type**: Return data type
- **function_body**: Procedural SQL statements

### Examples

**Example 1: Simple function**
```sql
CREATE FUNCTION calculate_tax(amount DECIMAL)
RETURNS DECIMAL
AS
BEGIN
    RETURN amount * 0.08;
END
```

**Example 2: Function with conditional logic**
```sql
CREATE FUNCTION get_discount(customer_level VARCHAR)
RETURNS DECIMAL
DETERMINISTIC
AS
BEGIN
    IF customer_level = 'gold' THEN
        RETURN 0.20;
    ELSIF customer_level = 'silver' THEN
        RETURN 0.10;
    ELSE
        RETURN 0.05;
    END IF;
END
```

**Example 3: Function querying data**
```sql
CREATE FUNCTION get_customer_total(cust_id INTEGER)
RETURNS DECIMAL
SQL SECURITY INVOKER
AS
    total DECIMAL;
BEGIN
    SELECT SUM(amount) INTO total
    FROM orders
    WHERE customer_id = cust_id;
    RETURN COALESCE(total, 0);
END
```

### Implementation Status

- V2 parser: Parses CREATE [OR REPLACE] FUNCTION with full parameter list, RETURNS, DETERMINISTIC, SQL SECURITY, and body capture
- Bytecode generator: Emits `EXT_CREATE_FUNCTION_STMT` opcode with all metadata
- Executor: Registers function in catalog via `registerFunction()` with schema resolution, ownership, and dependency tracking
- Runtime execution: Functions can be invoked at runtime via `executeFunction()` (executor.cpp:33441). Security context (DEFINER/INVOKER) is enforced during execution.

---

## CREATE PROCEDURE

### Description

Creates a stored procedure for executing procedural logic. Unlike functions, procedures don't return values directly but can have OUT parameters and a RETURNS clause.

### Syntax

```sql
CREATE [OR REPLACE] PROCEDURE <procedure_name> (
    [<parameter_name> [IN | OUT | INOUT] <parameter_type> [DEFAULT <value>] [, ...]]
)
[RETURNS (<param_name> <type> [, ...])]
[SQL SECURITY {DEFINER | INVOKER}]
AS
BEGIN
    <procedure_body>
END
```

### Parameters

- **OR REPLACE**: Replaces existing procedure
- **procedure_name**: Name of the procedure (schema-qualified paths supported)
- **IN**: Input parameter (default)
- **OUT**: Output parameter
- **INOUT**: Input/output parameter
- **RETURNS**: Named output parameters (Firebird-style)
- **SQL SECURITY**: Execution context
- **procedure_body**: Procedural SQL statements

### Examples

**Example 1: Simple procedure**
```sql
CREATE PROCEDURE update_user_status(
    user_id INTEGER,
    new_status VARCHAR
)
AS
BEGIN
    UPDATE users
    SET status = new_status,
        updated_at = CURRENT_TIMESTAMP
    WHERE id = user_id;
END
```

**Example 2: Procedure with OUT parameter**
```sql
CREATE PROCEDURE create_order(
    cust_id INTEGER,
    amount DECIMAL,
    OUT order_id INTEGER
)
AS
BEGIN
    INSERT INTO orders (customer_id, amount, created_at)
    VALUES (cust_id, amount, CURRENT_TIMESTAMP)
    RETURNING id INTO order_id;
END
```

### Implementation Status

- V2 parser: Parses CREATE [OR REPLACE] PROCEDURE with IN/OUT/INOUT params, RETURNS output params, SQL SECURITY, and body capture
- Bytecode generator: Emits `EXT_CREATE_PROCEDURE_STMT` opcode with all metadata
- Executor: Registers procedure in catalog via `registerProcedure()` with schema resolution, ownership, and dependency tracking
- **Limitation:** Procedure bodies are stored as source text. Procedural body interpretation for runtime execution of complex control flow is not yet wired.

---

## CREATE TRIGGER

### Description

Creates a trigger that automatically executes in response to INSERT, UPDATE, or DELETE operations on a table.

### Syntax

```sql
CREATE [OR REPLACE] TRIGGER <trigger_name>
    FOR <table_name>
    [BEFORE | AFTER] [INSERT | UPDATE | DELETE]
    [POSITION <number>]
AS
BEGIN
    <trigger_body>
END
```

### Parameters

- **OR REPLACE**: Replaces existing trigger
- **trigger_name**: Name of the trigger
- **table_name**: Table the trigger is attached to
- **BEFORE | AFTER**: Execution timing
- **INSERT | UPDATE | DELETE**: Event type
- **POSITION**: Execution order (lower numbers first)
- **trigger_body**: Procedural SQL statements

### Examples

**Example 1: Before insert trigger**
```sql
CREATE TRIGGER set_created_timestamp
    FOR users
    BEFORE INSERT
AS
BEGIN
    NEW.created_at = CURRENT_TIMESTAMP;
END
```

**Example 2: After update trigger**
```sql
CREATE TRIGGER log_status_change
    FOR orders
    AFTER UPDATE
AS
BEGIN
    IF OLD.status <> NEW.status THEN
        INSERT INTO order_status_log (order_id, old_status, new_status, changed_at)
        VALUES (NEW.id, OLD.status, NEW.status, CURRENT_TIMESTAMP);
    END IF;
END
```

### Implementation Status

- V2 parser: Parses CREATE [OR REPLACE] TRIGGER with FOR table, BEFORE/AFTER timing, INSERT/UPDATE/DELETE events, POSITION, and body capture
- Bytecode generator: Emits `EXT_CREATE_TRIGGER` opcode with all metadata
- Executor: Registers trigger in catalog via `createTrigger()` with table binding, event type, and timing
- Runtime triggers (C++-registered) expose old/new values via TriggerContext
- **Limitation:** SQL trigger body interpretation is not yet fully wired for procedural runtime execution

Trigger quick reference: [Trigger Cheat Sheet](../../user-guides/Trigger-Cheat-Sheet.md)

---

### Trigger Context Variables (NEW / OLD)

#### Description
Row-level triggers expose pseudo-records for column values before and after the
change.

#### Syntax
```sql
NEW.<column>
OLD.<column>
```

#### Examples
```sql
-- BEFORE INSERT
NEW.created_at = CURRENT_TIMESTAMP;

-- AFTER UPDATE
IF OLD.status <> NEW.status THEN
    INSERT INTO audit_log (id, old_status, new_status)
    VALUES (NEW.id, OLD.status, NEW.status);
END IF;
```

#### Status
- CREATE TRIGGER is parsed in V2 with full DDL support
- Runtime triggers (C++-registered) expose old/new values via TriggerContext
- SQL trigger body procedural interpretation is not yet wired for runtime execution

---

## EXECUTE BLOCK

### Description

Executes an anonymous procedural block without creating a stored procedure.

### Syntax

```sql
EXECUTE BLOCK [(parameter = value [, ...])]
[RETURNS (output_name type [, ...])]
AS
    [DECLARE VARIABLE name type [NOT NULL] [DEFAULT value];]
BEGIN
    <procedural_statements>
END
```

### Examples

**Example 1: Simple anonymous block**
```sql
EXECUTE BLOCK
AS
BEGIN
    UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = 42;
END
```

**Example 2: Block with variables**
```sql
EXECUTE BLOCK
AS
    DECLARE VARIABLE total_count INTEGER;
    DECLARE VARIABLE avg_amount DECIMAL;
BEGIN
    SELECT COUNT(*), AVG(amount)
    INTO total_count, avg_amount
    FROM orders
    WHERE status = 'completed';

    INSERT INTO daily_stats (stat_date, order_count, avg_order)
    VALUES (CURRENT_DATE, total_count, avg_amount);
END
```

**Example 3: Block with returns**
```sql
EXECUTE BLOCK
RETURNS (user_id INTEGER, user_name VARCHAR(100))
AS
BEGIN
    FOR SELECT id, name FROM users WHERE active = 1
        INTO user_id, user_name
    DO
        SUSPEND;
END
```

### Implementation Status

- V2 parser: Parses EXECUTE BLOCK with input parameters, RETURNS output definitions, DECLARE VARIABLE (with NOT NULL and DEFAULT), and BEGIN/END body parsing
- **Limitation:** Procedural body runtime execution (control flow, SUSPEND) is not yet fully wired

---

## EXECUTE PROCEDURE / EXECUTE STATEMENT

### Description

Executes a stored procedure or dynamic SQL statement.

### Syntax

```sql
EXECUTE PROCEDURE procedure_name [(parameters)]
    [RETURNING_VALUES variable_list];

EXECUTE STATEMENT sql_string
    [WITH AUTONOMOUS TRANSACTION]
    [INTO variable_list];
```

### Examples

**Example 1: Execute procedure**
```sql
EXECUTE PROCEDURE update_statistics;
```

**Example 2: Execute procedure with parameters**
```sql
EXECUTE PROCEDURE create_user('alice', 'alice@example.com')
    RETURNING_VALUES :new_user_id;
```

**Example 3: Execute dynamic SQL**
```sql
EXECUTE STATEMENT 'UPDATE ' || :table_name || ' SET active = 1';
```

### Implementation Status

- V2 parser: Parses EXECUTE PROCEDURE with arguments and RETURNING_VALUES clause
- V2 parser: Parses EXECUTE STATEMENT for dynamic SQL
- **Limitation:** Dynamic SQL runtime interpretation is not yet fully wired

---

## Procedural Statements

### Description

Procedural SQL provides flow control and variable handling within functions, procedures, triggers, and blocks.

### Control Flow

**IF Statement:**
```sql
IF condition THEN
    statements;
ELSIF condition THEN
    statements;
ELSE
    statements;
END IF;
```

**WHILE Loop:**
```sql
WHILE condition DO
BEGIN
    statements;
END
```

**FOR Loop:**
```sql
FOR variable IN (select_statement) DO
BEGIN
    statements;
END
```

**FOR Loop (numeric):**
```sql
FOR variable = start_value TO end_value [BY step] DO
BEGIN
    statements;
END
```

### Variables

**Declaration:**
```sql
DECLARE VARIABLE variable_name type [NOT NULL] [DEFAULT value];
```

**Assignment:**
```sql
variable_name = expression;
variable_name := expression; -- Alternative syntax
```

**SELECT INTO:**
```sql
SELECT column1, column2
INTO variable1, variable2
FROM table
WHERE condition;
```

### Other Statements

**SUSPEND:**
```sql
SUSPEND; -- Yield current row in result set
```

**EXIT:**
```sql
EXIT; -- Exit loop or procedure
```

**RETURN:**
```sql
RETURN value; -- Return from function
```

**EXCEPTION:**
```sql
EXCEPTION exception_name 'message';
```

### Implementation Status

- DECLARE VARIABLE is parsed within EXECUTE BLOCK (with NOT NULL, DEFAULT)
- BEGIN/END block structure is parsed
- **Limitation:** Procedural control flow (IF/WHILE/FOR) within bodies, runtime variable assignment, and SUSPEND are not yet fully wired for execution. Bodies are captured as source text during DDL and stored in the catalog.

---

## Job Scheduler

### CREATE JOB

#### Description

Creates a scheduled job that runs SQL statements, stored procedures, or external commands on a defined schedule. Jobs support cron expressions, one-time execution, and interval-based scheduling, with options for partitioning, retry behavior, timeouts, and dependency chains.

#### Syntax

```sql
CREATE [OR ALTER] JOB <job_name>
    SCHEDULE = { CRON '<cron_expression>'
               | AT '<timestamp>'
               | EVERY <duration> [<unit>] [STARTS '<timestamp>'] [ENDS '<timestamp>'] }
    [DEPENDS ON <job_name> [, ...]]
    [CLASS = <job_class>]
    [PARTITION BY { ALL_SHARDS | SINGLE_SHARD '<shard>' | SHARD_SET (<shard>, ...) | DYNAMIC (<expression>) }]
    [MAX_RETRIES = <integer>]
    [RETRY_BACKOFF = <duration> [<unit>]]
    [TIMEOUT = <duration> [<unit>]]
    [ON COMPLETION { PRESERVE | DROP }]
    [RUN AS <role_name>]
    [DESCRIPTION = '<description>']
    [STATE = { ENABLED | DISABLED | PAUSED }]
    { AS '<sql_statement>'
    | CALL <procedure_name> [()]
    | EXEC '<external_command>' }

RECREATE JOB <job_name> ...
```

Duration units: `S`/`SEC`/`SECOND(S)`, `M`/`MIN`/`MINUTE(S)`, `H`/`HOUR(S)`, `D`/`DAY(S)`

#### Examples

**Example 1: Cron-scheduled SQL job**
```sql
CREATE JOB nightly_cleanup
    SCHEDULE = CRON '0 2 * * *'
    DESCRIPTION = 'Clean up expired sessions'
    AS 'DELETE FROM sessions WHERE expires_at < CURRENT_TIMESTAMP';
```

**Example 2: Interval-based procedure call**
```sql
CREATE JOB sync_analytics
    SCHEDULE = EVERY 15 MINUTES
    STARTS '2026-01-01 00:00:00'
    MAX_RETRIES = 3
    RETRY_BACKOFF = 30 SECONDS
    TIMEOUT = 5 MINUTES
    CALL refresh_analytics_cache();
```

**Example 3: One-time execution**
```sql
CREATE JOB migration_task
    SCHEDULE = AT '2026-02-01 03:00:00'
    ON COMPLETION DROP
    AS 'CALL run_data_migration()';
```

**Example 4: Partitioned job with dependencies**
```sql
CREATE JOB shard_reindex
    SCHEDULE = CRON '0 4 * * 0'
    PARTITION BY ALL_SHARDS
    DEPENDS ON nightly_cleanup
    RUN AS maintenance_role
    STATE = ENABLED
    AS 'REINDEX ALL';
```

#### Implementation Status

- V2 parser: `parseCreateJob()` handles all options including SCHEDULE (CRON/AT/EVERY), DEPENDS ON, CLASS, PARTITION BY, MAX_RETRIES, RETRY_BACKOFF, TIMEOUT, ON COMPLETION, RUN AS, DESCRIPTION, STATE, and body types (AS/CALL/EXEC)
- RECREATE JOB is supported via `parseCreateJob(false, true)` in the statement dispatch
- CREATE OR ALTER JOB is supported via `parseCreateJob(true, false)`

---

### ALTER JOB

#### Description

Modifies properties of an existing scheduled job, including schedule, retry behavior, timeout, state, and job body.

#### Syntax

```sql
ALTER JOB <job_name>
    [SCHEDULE = { CRON '<cron_expression>'
                | AT '<timestamp>'
                | EVERY <duration> [<unit>] [STARTS '<timestamp>'] [ENDS '<timestamp>'] }]
    [MAX_RETRIES = <integer>]
    [RETRY_BACKOFF = <duration> [<unit>]]
    [TIMEOUT = <duration> [<unit>]]
    [ON COMPLETION { PRESERVE | DROP }]
    [RUN AS <role_name>]
    [DESCRIPTION = '<description>']
    [STATE = { ENABLED | DISABLED | PAUSED }]
    [{ AS '<sql_statement>' | CALL <procedure_name> [()] | EXEC '<external_command>' }]
```

#### Examples

```sql
ALTER JOB nightly_cleanup STATE = DISABLED;
ALTER JOB sync_analytics SCHEDULE = EVERY 30 MINUTES TIMEOUT = 10 MINUTES;
ALTER JOB migration_task AS 'CALL run_data_migration_v2()';
```

#### Implementation Status

- V2 parser: `parseAlterJob()` supports modifying all job properties

---

### DROP JOB

#### Description

Removes a scheduled job.

#### Syntax

```sql
DROP JOB <job_name> [KEEP HISTORY]
```

#### Examples

```sql
DROP JOB nightly_cleanup;
DROP JOB old_job KEEP HISTORY;
```

#### Implementation Status

- V2 parser: `parseDropJob()` supports DROP JOB with optional KEEP HISTORY

---

### EXECUTE JOB

#### Description

Immediately triggers execution of a scheduled job, regardless of its schedule.

#### Syntax

```sql
EXECUTE JOB <job_name>
```

#### Example

```sql
EXECUTE JOB nightly_cleanup;
```

#### Implementation Status

- V2 parser: `parseExecuteJob()` parses immediate job execution

---

### CANCEL JOB RUN

#### Description

Cancels a currently running job execution by its run UUID.

#### Syntax

```sql
CANCEL JOB RUN <job_run_uuid>
```

#### Example

```sql
CANCEL JOB RUN 'a1b2c3d4-e5f6-7890-abcd-ef1234567890';
```

#### Implementation Status

- V2 parser: `parseCancelJobRun()` parses cancellation by UUID (string literal or identifier)

---

## Known Limitations

### What Works

- **CREATE FUNCTION**: Full DDL parsing, bytecode generation, and catalog registration across all parsers (V2, Firebird, MySQL, PostgreSQL)
- **CREATE PROCEDURE**: Full DDL parsing, bytecode generation, and catalog registration across all parsers
- **CREATE TRIGGER**: Full DDL parsing, bytecode generation, and catalog registration across all parsers
- **EXECUTE BLOCK**: Parsed with input/output parameters, variable declarations, and body
- **EXECUTE PROCEDURE**: Parsed with arguments and RETURNING_VALUES
- **EXECUTE STATEMENT**: Parsed for dynamic SQL
- **CREATE/ALTER/DROP JOB**: Full DDL parsing with schedule, partition, retry, and dependency options
- **EXECUTE JOB / CANCEL JOB RUN**: Parsed for immediate execution and run cancellation
- **CALL**: Implemented in V2 parser via `parseCall()`

### Remaining Gaps

- **Procedural body runtime execution**: Function/procedure/trigger bodies are stored as source text. Complex procedural control flow (IF, WHILE, FOR, SUSPEND, exception handling) within bodies is not yet wired for runtime interpretation. Simple single-statement bodies work through the standard execution path.

### Spec References

- Functions: `/docs/specifications/ddl/DDL_FUNCTIONS.md`
- Procedures: `/docs/specifications/ddl/DDL_PROCEDURES.md`
- Triggers: `/docs/specifications/ddl/DDL_TRIGGERS.md`
- Transaction model: `/docs/specifications/sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
