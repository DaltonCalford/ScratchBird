# Dedicated ISQL Clients Requirement - Multi-Database Emulation Testing

**Finding Date:** 2025-12-28
**Status:** ⚠️ CRITICAL REQUIREMENT FOR COMPATIBILITY TESTING
**Impact:** HIGH - Blocks SQL compatibility test automation

---

## Executive Summary

**Problem:** Current `sb_isql` uses ScratchBird's native wire protocol (port 3092) and can only test the ScratchBird V2 parser. Testing Firebird, PostgreSQL, and MySQL emulation requires clients that speak their native wire protocols.

**Solution:** Create **four dedicated ISQL utilities**, one for each database emulation:

| Utility | Protocol | Port | Purpose |
|---------|----------|------|---------|
| `sb_isql` | ScratchBird Native | 3092 | ✅ EXISTS - ScratchBird V2 testing |
| `sb_fb_isql` | Firebird XDR | 3050 | ❌ NEEDED - Firebird emulation testing |
| `sb_my_isql` | MySQL Text Protocol | 3306 | ❌ NEEDED - MySQL emulation testing |
| `sb_pg_isql` | PostgreSQL Frontend/Backend | 5432 | ❌ NEEDED - PostgreSQL emulation testing |

**Effort Estimate:** 80-120 hours (3 clients × 27-40 hours each)

**Why This Is Necessary:**
1. **Wire Protocol Compatibility:** Each database has its own wire protocol. Firebird clients cannot talk to PostgreSQL servers and vice versa.
2. **Authentic Testing:** Official database test suites expect their native ISQL tools (isql, mysql, psql).
3. **Command-Line Compatibility:** Each database's ISQL has different flags, syntax, and behavior.
4. **Emulation Validation:** The only way to verify emulation correctness is to use standard clients that speak the native protocol.

---

## Background: Architecture Understanding

### Current ScratchBird Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        ScratchBird Server                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ┌─────────────────────────────────────────────────────────────┐        │
│  │              Wire Protocol Listeners                         │        │
│  ├──────────────┬──────────────┬──────────────┬─────────────────┤        │
│  │ ScratchBird  │   Firebird   │ PostgreSQL   │     MySQL       │        │
│  │ Native (3092)│  XDR (3050)  │   FE/BE      │  Text Protocol  │        │
│  │              │              │   (5432)     │    (3306)       │        │
│  └──────┬───────┴──────┬───────┴──────┬───────┴───────┬─────────┘        │
│         │              │              │               │                  │
│         ▼              ▼              ▼               ▼                  │
│  ┌─────────────────────────────────────────────────────────────┐        │
│  │                    Parser Layer                              │        │
│  ├──────────────┬──────────────┬──────────────┬─────────────────┤        │
│  │    V2        │   Firebird   │ PostgreSQL   │     MySQL       │        │
│  │   Parser     │    Parser    │    Parser    │    Parser       │        │
│  └──────┬───────┴──────┬───────┴──────┬───────┴───────┬─────────┘        │
│         │              │              │               │                  │
│         └──────────────┴──────────────┴───────────────┘                  │
│                                │                                          │
│                                ▼                                          │
│  ┌─────────────────────────────────────────────────────────────┐        │
│  │                    SBLR Bytecode                             │        │
│  └──────────────────────────┬───────────────────────────────────┘        │
│                             │                                            │
│                             ▼                                            │
│  ┌─────────────────────────────────────────────────────────────┐        │
│  │                    SBLR Executor                             │        │
│  └──────────────────────────┬───────────────────────────────────┘        │
│                             │                                            │
│                             ▼                                            │
│  ┌─────────────────────────────────────────────────────────────┐        │
│  │                 Storage Engine (MGA)                         │        │
│  └─────────────────────────────────────────────────────────────┘        │
│                                                                           │
└─────────────────────────────────────────────────────────────────────────┘
```

**Key Insight:** ScratchBird server **already implements** (or will implement) multiple wire protocol listeners. Each listener:
1. Accepts connections using that database's native protocol
2. Routes to the appropriate parser
3. Generates SBLR bytecode
4. Executes through the unified executor

### Client-Side Architecture (Current)

```
sb_isql (Client)
    │
    │ ScratchBird Native Wire Protocol (port 3092)
    │
    ▼
ScratchBird Server
    │
    ▼
V2 Parser Only
```

**Problem:** Can only test V2 parser, not emulation parsers.

### Client-Side Architecture (Needed)

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   sb_isql    │    │ sb_fb_isql   │    │ sb_pg_isql   │    │ sb_my_isql   │
│  (V2 Native) │    │  (Firebird)  │    │(PostgreSQL)  │    │   (MySQL)    │
└──────┬───────┘    └──────┬───────┘    └──────┬───────┘    └──────┬───────┘
       │                   │                   │                   │
       │ Native (3092)     │ XDR (3050)        │ FE/BE (5432)      │ Text (3306)
       │                   │                   │                   │
       └───────────────────┴───────────────────┴───────────────────┘
                                   │
                                   ▼
                          ScratchBird Server
                                   │
                   ┌───────────────┼───────────────┐
                   │               │               │
                   ▼               ▼               ▼
              V2 Parser    Firebird Parser  PostgreSQL Parser
                                               MySQL Parser
```

**Solution:** Each client speaks its database's native wire protocol, triggering the correct parser on the server.

---

## Why Separate ISQL Clients Are Required

### Reason 1: Wire Protocol Incompatibility

Each database has a completely different wire protocol:

**Firebird XDR Protocol:**
- XDR (External Data Representation) encoding
- Platform-independent binary format
- Message structure: opcode + parameters
- Connection: authentication via plugin system
- Example: `op_connect` message with version negotiation

**PostgreSQL Frontend/Backend Protocol:**
- Binary message protocol
- Messages: `StartupMessage`, `Query`, `DataRow`, etc.
- Length-prefixed messages
- COPY protocol for bulk data
- SSL/TLS negotiation

**MySQL Text Protocol:**
- Handshake with capability flags
- 4-byte packet headers
- COM_QUERY for text queries
- Result set packets
- OK, ERROR, EOF packets

**ScratchBird Native Protocol:**
- Custom 40-byte message header
- TLS 1.3 mandatory
- Message types: AUTH, QUERY, RESULT, etc.
- Session management

**Conclusion:** A client written for one protocol **cannot communicate** with a server expecting a different protocol.

### Reason 2: Official Test Suite Compatibility

Each database's official test suite expects its native ISQL tool:

**Firebird Test Suite (`fbt-repository`):**
- Tests written for `isql` (Firebird ISQL)
- Command: `isql -i test.sql -o output.txt`
- Firebird-specific flags: `-n` (no plan), `-z` (show execution plan)
- SET TERM support for PSQL
- Expects Firebird wire protocol responses

**PostgreSQL Test Suite (`src/test/regress`):**
- Tests written for `psql`
- Command: `psql -f test.sql -o output.txt`
- PostgreSQL-specific: `\copy`, `\d` commands
- Expects PostgreSQL protocol messages

**MySQL Test Suite (`mysql-test`):**
- Tests written for `mysql` client
- Command: `mysql < test.sql > output.txt 2>&1`
- MySQL-specific: `--execute`, `--batch` mode
- Expects MySQL protocol packets

**Problem:** We cannot run these tests without clients that match the expected interface.

### Reason 3: Command-Line Flag Compatibility

Each database's ISQL has different command-line flags:

| Flag | Firebird `isql` | PostgreSQL `psql` | MySQL `mysql` | ScratchBird `sb_isql` |
|------|-----------------|-------------------|---------------|----------------------|
| Input file | `-i <file>` | `-f <file>` | `< file` or `--execute` | `-f <file>` ✅ |
| Output file | `-o <file>` | `-o <file>` | `> file` redirect | `-o <file>` ✅ |
| Username | `-u <user>` | `-U <user>` | `-u <user>` | `-U <user>` ⚠️ |
| Password | `-p <pass>` | `-W` (prompt) | `-p[pass]` | `-P <pass>` ⚠️ |
| Database | `<db>` (last arg) | `-d <db>` | `-D <db>` or `<db>` | `<db>` (first arg) ✅ |
| Quiet mode | `-q` | `-q` | `-s` or `--silent` | `-q` ✅ |
| Echo input | `-e` | `-e` | `--verbose` | `-e` ✅ |
| No headers | N/A | `-t` | `-s` | `-t` ✅ |
| SQL dialect | `-sqldialect <n>` | N/A | N/A | `-s <n>` ✅ |

**Conclusion:** Tests written for each database expect its specific flag syntax.

### Reason 4: Session State and Behavior

Each database has different session behavior:

**Firebird:**
- SET TERM to change statement terminator
- SET SQL DIALECT to switch between dialects 1/2/3
- COMMIT RETAIN vs COMMIT (release)
- Transaction isolation levels

**PostgreSQL:**
- `\set` variables
- `\timing` for query timing
- `\x` for expanded display
- `BEGIN`/`COMMIT` transaction control

**MySQL:**
- `USE database` to switch databases
- `@@variables` and session variables
- `SET autocommit=0/1`
- `DELIMITER` to change delimiter

**ScratchBird:**
- Combination of Firebird SET commands + PostgreSQL meta-commands
- SET SQL DIALECT (Firebird)
- \timing (PostgreSQL-style)

**Conclusion:** Each ISQL must maintain session state compatible with its database.

---

## Detailed Requirements for Each ISQL Client

### 1. sb_isql (ScratchBird Native)

**Status:** ✅ ALREADY EXISTS

**Wire Protocol:** ScratchBird Native (port 3092)

**Purpose:** Test ScratchBird V2 native parser

**Features:**
- Already fully implemented (3,099 lines)
- Supports `-f`/`-o` redirection
- Firebird ISQL compatible SET commands
- PostgreSQL-style meta-commands (`\d`, `\i`, `\o`)
- TLS 1.3 connection

**No Changes Needed** (except optional `-par` flag discussed earlier, which is now NOT needed with dedicated clients)

---

### 2. sb_fb_isql (Firebird Emulation Client)

**Status:** ❌ DOES NOT EXIST - MUST BE CREATED

**Wire Protocol:** Firebird XDR (port 3050)

**Purpose:** Test Firebird SQL parser and emulation

**Requirements:**

#### Wire Protocol Implementation
- Implement Firebird XDR wire protocol client
- Connect to ScratchBird server on port 3050
- Support Firebird authentication (SRPS, Legacy, etc.)
- Handle op_connect, op_attach, op_compile, op_execute packets
- Parse Firebird result set format

#### Command-Line Flags (Firebird ISQL Compatible)
```
Usage: sb_fb_isql [options] [<database>]

Options:
  -i <file>               Execute commands from file
  -o <file>               Write output to file
  -u <user>               Username (default: SYSDBA)
  -p <password>           Password (prompted if not given)
  -r <role>               SQL role
  -sqldialect <1|2|3>     SQL dialect (default: 3)
  -e                      Echo commands
  -q                      Quiet mode (no banner)
  -x                      Extract DDL (no data)
  -a                      Extract all DDL
  -m                      Merge standard error
  -n                      No plan output
  -z                      Show execution plan
```

#### SET Commands (Firebird ISQL)
- `SET TERM <char>;` - Change statement terminator (critical for PSQL)
- `SET SQL DIALECT <n>;` - Change SQL dialect
- `SET NAMES <charset>;` - Client character set
- `SET BAIL [ON|OFF];` - Stop on first error
- `SET ECHO [ON|OFF];` - Echo commands
- `SET STATS [ON|OFF];` - Show performance statistics
- `SET PLAN [ON|OFF];` - Show query plan
- `SET COUNT [ON|OFF];` - Show row counts
- `SET LIST [ON|OFF];` - Vertical output format

#### Behavior Requirements
- **Must look like Firebird ISQL to test scripts**
- Statement terminator defaults to `;`
- Support `SET TERM ^;` for PSQL blocks
- Support Firebird system table queries (RDB$*, MON$*, SEC$*)
- Return Firebird-style error codes
- Format output like Firebird ISQL

#### Example Usage
```bash
# Run Firebird compatibility test
sb_fb_isql employee.fdb -u SYSDBA -p masterkey -i test.sql -o results.txt

# Interactive mode
sb_fb_isql employee.fdb
SQL> SET TERM ^;
SQL> CREATE PROCEDURE test AS BEGIN EXIT; END^
SQL> SET TERM ;^
SQL> SELECT * FROM RDB$PROCEDURES;
```

---

### 3. sb_pg_isql (PostgreSQL Emulation Client)

**Status:** ❌ DOES NOT EXIST - MUST BE CREATED

**Wire Protocol:** PostgreSQL Frontend/Backend (port 5432)

**Purpose:** Test PostgreSQL SQL parser and emulation

**Requirements:**

#### Wire Protocol Implementation
- Implement PostgreSQL Frontend/Backend protocol v3.0
- Connect to ScratchBird server on port 5432
- Support StartupMessage with protocol version
- Handle Query, Parse, Bind, Execute, Sync messages
- Parse RowDescription, DataRow, CommandComplete messages
- Support SSL/TLS negotiation (PostgreSQL-style)

#### Command-Line Flags (psql Compatible)
```
Usage: sb_pg_isql [OPTION]... [DBNAME [USERNAME]]

General options:
  -f <file>               Execute commands from file
  -o <file>               Send query results to file
  -c <command>            Execute single command
  -d <dbname>             Database name
  -U <user>               Username
  -W                      Force password prompt
  -w                      Never prompt for password

Input/Output:
  -a                      Echo all input
  -e                      Echo commands sent to server
  -E                      Display queries generated by backslash commands
  -q                      Run quietly (no welcome)
  -t                      Print tuples only
  -A                      Unaligned table output
  -F <sep>                Field separator

Connection:
  -h <host>               Database host (default: localhost)
  -p <port>               Database port (default: 5432)
```

#### Meta-Commands (psql Backslash Commands)
- `\d [table]` - Describe table or list tables
- `\dt` - List tables
- `\di` - List indexes
- `\dv` - List views
- `\df` - List functions
- `\du` - List users/roles
- `\l` - List databases
- `\c <database>` - Connect to database
- `\i <file>` - Execute commands from file
- `\o [file]` - Send output to file
- `\timing [on|off]` - Toggle timing
- `\x [on|off|auto]` - Toggle expanded output
- `\q` - Quit
- `\!` - Execute shell command
- `\? ` - Show help

#### SET Commands (PostgreSQL)
- `SET search_path TO ...` - Schema search path
- `SET TimeZone TO ...` - Session timezone
- `SET statement_timeout TO ...` - Query timeout
- `\set <var> <value>` - Set psql variable

#### Behavior Requirements
- **Must look like psql to test scripts**
- Statement terminator is `;`
- Support PostgreSQL system catalogs (pg_catalog.pg_*)
- Support information_schema views
- Return PostgreSQL-style SQLSTATE codes
- Format output like psql (ASCII art tables)

#### Example Usage
```bash
# Run PostgreSQL compatibility test
sb_pg_isql -d testdb -U postgres -f with.sql -o with.out -q

# Interactive mode
sb_pg_isql -d testdb
testdb=# \dt
testdb=# SELECT version();
testdb=# \timing on
testdb=# WITH RECURSIVE t(n) AS (
  SELECT 1 UNION ALL SELECT n+1 FROM t WHERE n < 5
) SELECT * FROM t;
```

---

### 4. sb_my_isql (MySQL Emulation Client)

**Status:** ❌ DOES NOT EXIST - MUST BE CREATED

**Wire Protocol:** MySQL Text Protocol (port 3306)

**Purpose:** Test MySQL SQL parser and emulation

**Requirements:**

#### Wire Protocol Implementation
- Implement MySQL Client/Server Text Protocol v10
- Connect to ScratchBird server on port 3306
- Support handshake with capability flags
- Handle COM_QUERY commands
- Parse OK, ERROR, EOF, Result Set packets
- Support compressed protocol (optional)

#### Command-Line Flags (mysql Compatible)
```
Usage: sb_my_isql [OPTIONS] [database]

General options:
  -e <statement>          Execute statement and quit
  -f <file>               Read SQL from file
  --execute=<statement>   Same as -e

Connection:
  -u <user>               Username
  -p[password]            Password (no space if inline)
  -h <host>               Host (default: localhost)
  -P <port>               Port (default: 3306)
  -D <database>           Database name
  -S <socket>             Socket file

Output control:
  -s, --silent            Silent mode (no table formatting)
  -t                      Display table output
  -v, --verbose           Verbose mode
  -E, --vertical          Vertical output format

Execution:
  -B, --batch             Batch mode (no interactive prompts)
  -n, --unbuffered        Flush buffer after each query
  --delimiter=<str>       Set delimiter (default: ;)
```

#### Commands (MySQL Client)
- `USE <database>` - Switch database
- `SHOW DATABASES` - List databases
- `SHOW TABLES` - List tables
- `SHOW COLUMNS FROM <table>` - Describe table
- `DESCRIBE <table>` - Same as SHOW COLUMNS
- `SOURCE <file>` - Execute SQL file
- `DELIMITER <str>` - Change statement delimiter
- `\q` or `quit` - Exit
- `\h` or `help` - Show help

#### SET Commands (MySQL Session)
- `SET autocommit = 0/1` - Transaction mode
- `SET NAMES <charset>` - Character set
- `SET @@session.var = value` - Session variables
- `SET sql_mode = '...'` - SQL mode flags

#### Behavior Requirements
- **Must look like mysql client to test scripts**
- Default delimiter is `;`
- Support DELIMITER command for stored procedures
- Support MySQL system schemas (mysql.*, information_schema.*, performance_schema.*)
- Return MySQL-style error numbers (1064, etc.)
- Format output like mysql client

#### Example Usage
```bash
# Run MySQL compatibility test
sb_my_isql -u root -p -D testdb < test.sql > results.txt 2>&1

# Using -e flag
sb_my_isql -u root -p -D testdb -e "SELECT @@version"

# Interactive mode
sb_my_isql -u root -p
mysql> USE testdb;
mysql> DELIMITER //
mysql> CREATE PROCEDURE test() BEGIN SELECT 1; END//
mysql> DELIMITER ;
mysql> CALL test();
```

---

## Comparison Matrix

| Feature | sb_isql | sb_fb_isql | sb_pg_isql | sb_my_isql |
|---------|---------|------------|------------|------------|
| **Wire Protocol** | ScratchBird Native | Firebird XDR | PostgreSQL FE/BE | MySQL Text |
| **Default Port** | 3092 | 3050 | 5432 | 3306 |
| **Parser Triggered** | V2 | Firebird | PostgreSQL | MySQL |
| **Input Flag** | `-f` | `-i` | `-f` | Stdin `<` or `-e` |
| **Output Flag** | `-o` | `-o` | `-o` | Stdout `>` |
| **Username Flag** | `-U` | `-u` | `-U` | `-u` |
| **Password Flag** | `-P` | `-p` | `-W` or `-w` | `-p` |
| **Database Arg** | First | Last | `-d` or last | `-D` or last |
| **Statement Term** | `;` (changeable) | `;` (SET TERM) | `;` | `;` (DELIMITER) |
| **Meta-Commands** | `\` prefix | SET commands | `\` prefix | Commands |
| **Primary Use** | ScratchBird testing | Firebird tests | PostgreSQL tests | MySQL tests |

---

## Implementation Implications

### Server-Side Requirements

**ScratchBird server MUST implement or already implements:**

1. **Wire Protocol Listeners**
   - ✅ ScratchBird Native (port 3092) - EXISTS
   - ⚠️ Firebird XDR (port 3050) - STATUS UNKNOWN
   - ⚠️ PostgreSQL FE/BE (port 5432) - STATUS UNKNOWN
   - ⚠️ MySQL Text (port 3306) - STATUS UNKNOWN

2. **Protocol-to-Parser Routing**
   - Connection on port 3050 → Firebird parser
   - Connection on port 5432 → PostgreSQL parser
   - Connection on port 3306 → MySQL parser
   - Connection on port 3092 → V2 parser

3. **Emulation Schema Structure**
   - `/remote/emulated/firebird/` - Firebird databases
   - `/remote/emulated/postgresql/` - PostgreSQL databases
   - `/remote/emulated/mysql/` - MySQL databases

**If server-side wire protocol listeners do not exist, they MUST be implemented BEFORE the ISQL clients.**

### Client-Side Requirements

Each ISQL client MUST:

1. **Implement Wire Protocol**
   - Full client-side implementation of the database's wire protocol
   - Authentication
   - Query execution
   - Result set parsing
   - Error handling

2. **Command-Line Parsing**
   - Match the native database's ISQL command-line syntax
   - Support all common flags used in test scripts

3. **Session Management**
   - Maintain connection state
   - Handle transactions
   - Support database-specific SET commands

4. **Output Formatting**
   - Format results to match native ISQL tool
   - Support various output modes (aligned, unaligned, vertical, etc.)

---

## Testing Impact

### Without Dedicated ISQL Clients

**What We CAN'T Do:**
- ❌ Run Firebird test suite tests (they expect `isql -i`)
- ❌ Run PostgreSQL test suite tests (they expect `psql -f`)
- ❌ Run MySQL test suite tests (they expect `mysql <`)
- ❌ Verify wire protocol emulation is working
- ❌ Test that Firebird clients can connect to ScratchBird
- ❌ Validate parser compatibility with real-world SQL

**What We CAN Do:**
- ✅ Test ScratchBird V2 parser only
- ⚠️ Manually convert test scripts to ScratchBird format (time-consuming, error-prone)

### With Dedicated ISQL Clients

**What We CAN Do:**
- ✅ Run official Firebird test suite against sb_fb_isql
- ✅ Run official PostgreSQL test suite against sb_pg_isql
- ✅ Run official MySQL test suite against sb_my_isql
- ✅ Verify wire protocol compatibility
- ✅ Test emulation accuracy with real SQL scripts
- ✅ Automate regression testing for all parsers
- ✅ Demonstrate compatibility to users

---

## Dependencies and Blockers

### Server-Side Dependencies

Before implementing ISQL clients, verify:

1. **Wire Protocol Listeners Status**
   - [ ] Firebird XDR listener on port 3050 - EXISTS?
   - [ ] PostgreSQL FE/BE listener on port 5432 - EXISTS?
   - [ ] MySQL Text listener on port 3306 - EXISTS?

2. **Parser Routing**
   - [ ] Connection on 3050 routes to Firebird parser?
   - [ ] Connection on 5432 routes to PostgreSQL parser?
   - [ ] Connection on 3306 routes to MySQL parser?

3. **Emulation Schema Support**
   - [ ] Can create schemas under `/remote/emulated/firebird/`?
   - [ ] Can create schemas under `/remote/emulated/postgresql/`?
   - [ ] Can create schemas under `/remote/emulated/mysql/`?

**If ANY of these are not implemented, they are BLOCKERS for the ISQL clients.**

### Client-Side Dependencies

Each ISQL client requires:

1. **Wire Protocol Library**
   - Firebird: XDR encoding/decoding library
   - PostgreSQL: libpq or custom FE/BE implementation
   - MySQL: MySQL client library or custom implementation

2. **Build System**
   - CMake configuration for each client
   - Link with appropriate protocol libraries
   - Optional: static linking for distribution

---

## Recommendations

### Priority Order

1. **PRIORITY 1: Verify Server-Side Infrastructure** (IMMEDIATE)
   - Check if wire protocol listeners exist
   - Check if parser routing works
   - Document current server capabilities

2. **PRIORITY 2: Implement sb_fb_isql** (HIGH - Firebird is most critical)
   - Firebird emulation is most mature
   - Firebird tests are most readily available
   - Firebird wire protocol is well-documented

3. **PRIORITY 3: Implement sb_pg_isql** (MEDIUM - PostgreSQL next)
   - PostgreSQL has excellent test suite
   - Wide industry adoption
   - Many SQL features to test

4. **PRIORITY 4: Implement sb_my_isql** (LOWER - MySQL last)
   - MySQL compatibility is nice-to-have
   - Fewer advanced SQL features than PostgreSQL
   - Can defer until Firebird/PostgreSQL working

### Alternative Approach: Use Existing Native Clients

**Could we use native `isql`, `psql`, `mysql` clients instead?**

**Answer: NO, for testing automation**

**Why Not:**
1. **Wire Protocol Mismatch:** Native `isql` expects Firebird server on port 3050, not ScratchBird
2. **Missing Libraries:** Users may not have Firebird/MySQL/PostgreSQL clients installed
3. **Version Compatibility:** Different versions of clients may behave differently
4. **Automation Control:** We can't control behavior of third-party tools
5. **Test Integration:** We need to embed testing in ScratchBird's build system

**However, native clients CAN be used for manual testing:**
- If ScratchBird's Firebird listener is perfect, `isql` should work
- If ScratchBird's PostgreSQL listener is perfect, `psql` should work
- If ScratchBird's MySQL listener is perfect, `mysql` should work

**Conclusion:** We need sb_fb_isql, sb_pg_isql, sb_my_isql for **automated testing**, but successful connection from native clients would be the **ultimate validation**.

---

## Success Criteria

### For Each ISQL Client

A successful implementation means:

1. **Connectivity**
   - Client can connect to ScratchBird server on correct port
   - Client authenticates successfully
   - Client maintains persistent connection

2. **Query Execution**
   - Client can send SQL queries
   - Client receives results correctly
   - Client handles errors appropriately

3. **Test Suite Compatibility**
   - Official database test scripts run without modification (or minimal modification)
   - Output format matches expected format (or close enough for diff)
   - Tests pass at same rate as native database

4. **Command-Line Compatibility**
   - All common flags work as documented
   - Input/output redirection works
   - Batch mode works for automation

5. **Session State**
   - SET commands work
   - Meta-commands work
   - Transaction control works

---

## Next Steps

### Immediate Actions

1. **Document Server Capabilities** (1-2 hours)
   - Check if wire protocol listeners exist
   - Test manual connection on ports 3050, 5432, 3306
   - Document current server emulation support

2. **Create Implementation Plan** (2-4 hours)
   - Detailed specification for each ISQL client
   - Architecture diagrams
   - API requirements
   - Test plan

3. **Assign Development** (Organizational)
   - Assign sb_fb_isql to AI team A
   - Assign sb_pg_isql to AI team B
   - Assign sb_my_isql to AI team C
   - Coordinate on shared code patterns

4. **Setup Test Infrastructure** (In parallel with client development)
   - Pull down database test suites
   - Convert tests to local format
   - Create test runner scripts
   - Setup CI/CD integration

---

## Conclusion

**Creating dedicated ISQL clients (sb_fb_isql, sb_pg_isql, sb_my_isql) is MANDATORY for:**
- Running official database test suites
- Verifying wire protocol emulation
- Demonstrating compatibility
- Automating regression testing

**Without these clients, we cannot:**
- Prove Firebird compatibility
- Prove PostgreSQL compatibility
- Prove MySQL compatibility
- Run automated compatibility tests

**This is a CRITICAL requirement for the emulation architecture to be validated.**

---

**Document Status:** ✅ Complete - Ready for Planning Phase
**Next Document:** Implementation plan with detailed specifications
**Estimated Total Effort:** 80-120 hours (3 clients)
**Blocking:** Test suite integration, emulation validation

---

**Created:** 2025-12-28
**Author:** Claude Code
**Review Required:** Server-side wire protocol status verification
