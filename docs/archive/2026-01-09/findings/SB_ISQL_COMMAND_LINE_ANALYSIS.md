# sb_isql Command-Line Analysis and Enhancement Recommendations

**Analysis Date:** 2025-12-28
**File Analyzed:** `/src/cli/sb_isql.cpp` (3,099 lines)
**Purpose:** Review sb_isql command-line support for SQL compatibility testing

---

## Executive Summary

**Current Status:**
- ✅ **Output redirection** (`-o`/`--output`) is FULLY SUPPORTED
- ✅ **Input file execution** (`-f`/`--file`) is FULLY SUPPORTED
- ⚠️ **Input redirection** (`-i`) is NOT SUPPORTED (uses `-f` instead)
- ❌ **Parser selection** (`-par`/`--parser`) is NOT SUPPORTED

**Architecture:** sb_isql is a **client-only** CLI tool that connects to `sb_server` via TCP/socket. SQL parsing occurs on the **server side**, not in the CLI.

**Recommendation:** Add `-par`/`--parser` flag to sb_isql that sends parser selection to the server via a `SET PARSER` command or connection parameter.

---

## Current Command-Line Interface

### Supported Flags (24 options)

| Flag | Long Form | Description | Status |
|------|-----------|-------------|--------|
| `-U` | `--user` | Username for authentication | ✅ Implemented |
| `-P` | `--password` | Password | ✅ Implemented |
| `-p` | `--port` | TCP port (default: 3092) | ✅ Implemented |
| `-H` | `--host` | Host (default: localhost) | ✅ Implemented |
| `-c` | `--command` | Execute single command and exit | ✅ Implemented |
| `-f` | `--file` | Execute commands from file | ✅ Implemented |
| `-o` | `--output` | Write output to file | ✅ Implemented |
| `-t` | `--tuples-only` | Print tuples only (no headers) | ✅ Implemented |
| `-A` | `--no-align` | Unaligned output mode | ✅ Implemented |
| `-F` | `--field-separator` | Field separator (default: \|) | ✅ Implemented |
| `-q` | `--quiet` | Quiet mode (no welcome) | ✅ Implemented |
| `-e` | `--echo` | Echo commands before execution | ✅ Implemented |
| `-b` | `--bail` | Stop on first error | ✅ Implemented |
| `-v` | `--verbose` | Verbose mode | ✅ Implemented |
| `-a` | `--extract-all` | Extract DDL for all objects | ✅ Implemented |
| `-x` | `--extract` | Extract DDL (no data) | ✅ Implemented |
| `-ex` | `--extract-db` | Extract DDL with CREATE DATABASE | ✅ Implemented |
| `-s` | `--dialect` | SQL dialect (1, 2, or 3) | ✅ Implemented |
| `-h` | `--help` | Show help | ✅ Implemented |
|  | `--version` | Show version | ✅ Implemented |

### Meta-Commands (Interactive Mode)

| Meta-Command | Description | Status |
|--------------|-------------|--------|
| `\?` | Show help for meta-commands | ✅ Implemented |
| `\q` | Quit | ✅ Implemented |
| `\d` | List tables | ✅ Implemented |
| `\d <table>` | Describe table | ✅ Implemented |
| `\dt` | List tables | ✅ Implemented |
| `\di` | List indexes | ✅ Implemented |
| `\du` | List users | ✅ Implemented |
| `\l` | List databases | ✅ Implemented |
| `\c <database>` | Connect to database | ✅ Implemented |
| `\i <file>` | Execute commands from file | ✅ Implemented |
| `\o <file>` | Write output to file | ✅ Implemented |
| `\timing [on\|off]` | Toggle timing display | ✅ Implemented |
| `\pset <option> <value>` | Set output formatting | ✅ Implemented |
| `\x [on\|off]` | Toggle expanded display | ✅ Implemented |
| `\! <command>` | Execute shell command | ✅ Implemented |

### SET Commands (Firebird ISQL Compatible)

| SET Command | Description | Status |
|-------------|-------------|--------|
| `SET SQL DIALECT N` | Set SQL dialect (1, 2, or 3) | ✅ Implemented |
| `SET BAIL [ON\|OFF]` | Stop on first error | ✅ Implemented |
| `SET TERM <char>` | Change statement terminator | ✅ Implemented |
| `SET COUNT [ON\|OFF]` | Display row counts | ✅ Implemented |
| `SET HEADING [ON\|OFF]` | Show column headings | ✅ Implemented |
| `SET ECHO [ON\|OFF]` | Echo commands | ✅ Implemented |
| `SET LIST [ON\|OFF]` | Vertical display mode | ✅ Implemented |
| `SET NULL <string>` | String for NULL values | ✅ Implemented |
| `SET WIDTH <col> <n>` | Set column display width | ✅ Implemented |
| `SET STATS [ON\|OFF]` | Show timing statistics | ✅ Implemented |
| `SET PLAN [ON\|OFF]` | Show query execution plan | ✅ Implemented |
| `SET PLANONLY [ON\|OFF]` | Show plan only, don't execute | ✅ Implemented |
| `SET EXPLAIN [ON\|OFF]` | Show detailed plan analysis | ✅ Implemented |

---

## Current Support for Testing Requirements

### ✅ Output Redirection Support

**Command-line:**
```bash
sb_isql mydb.sbdb -f test.sql -o results.txt
```

**Code Location:** `src/cli/sb_isql.cpp:2938-2941`
```cpp
} else if (arg == "-o" && i + 1 < argc) {
    g_config.output_file = argv[++i];
} else if (arg.find("--output=") == 0) {
    g_config.output_file = arg.substr(9);
```

**Implementation:**
- Opens `std::ofstream` at startup (line 3017-3023)
- All output goes through `getOutput()` which returns either `std::cout` or the file stream
- Closes on exit (line 3084-3087)

**Status:** ✅ **FULLY FUNCTIONAL** - Ready for test automation

### ✅ Input File Support

**Command-line:**
```bash
sb_isql mydb.sbdb -f test.sql
```

**Code Location:** `src/cli/sb_isql.cpp:2934-2937`
```cpp
} else if (arg == "-f" && i + 1 < argc) {
    g_config.input_file = argv[++i];
} else if (arg.find("--file=") == 0) {
    g_config.input_file = arg.substr(7);
```

**Implementation:**
- Uses `\i` meta-command internally (line 3070-3072)
- Reads file line-by-line
- Respects statement terminator (`SET TERM`)
- Supports `--bail` to stop on first error
- Skips comments (lines starting with `#` or `--`)

**Meta-command Alternative:**
```bash
sb_isql mydb.sbdb
\i test.sql
```

**Status:** ✅ **FULLY FUNCTIONAL** - Ready for test automation

### ⚠️ Alternative Input Flag (`-i`)

**Requested:** `-i <file>` as alternative to `-f <file>`

**Current Status:** ❌ NOT SUPPORTED

**Analysis:**
- Only `-f`/`--file` is supported for input files
- `\i` meta-command exists for interactive use
- No `-i` command-line flag

**Compatibility:**
- **Firebird ISQL:** Uses `-i <file>`
- **PostgreSQL psql:** Uses `-f <file>` (same as sb_isql)
- **MySQL mysql:** Uses `--execute` or `<` redirection

**Recommendation:** Add `-i` as an alias to `-f` for Firebird compatibility

**Effort:** Low (5-10 minutes)

**Implementation:**
```cpp
// In parseArgs() function (around line 2934):
} else if (arg == "-i" && i + 1 < argc) {  // NEW
    g_config.input_file = argv[++i];
} else if (arg == "-f" && i + 1 < argc) {
    g_config.input_file = argv[++i];
```

---

## ❌ Parser Selection Support

### Requested Feature

**Flag:** `-par <parser>` or `--parser=<parser>`

**Values:**
- `ScratchBird` or `V2` - ScratchBird native V2 parser
- `FirebirdSQL` or `Firebird` - Firebird SQL parser
- `PostgreSQL` or `Postgres` or `PG` - PostgreSQL parser
- `MySQL` - MySQL parser

**Purpose:** Allow test scripts to specify which SQL parser to use

**Example Usage:**
```bash
# Test with Firebird parser
sb_isql mydb.sbdb -par FirebirdSQL -f firebird_tests.sql -o fb_results.txt

# Test with PostgreSQL parser
sb_isql mydb.sbdb -par PostgreSQL -f postgres_tests.sql -o pg_results.txt

# Test with MySQL parser
sb_isql mydb.sbdb -par MySQL -f mysql_tests.sql -o mysql_results.txt
```

### Current Status: ❌ NOT SUPPORTED

**Why Not Supported:**

sb_isql is a **client-only** CLI tool with a client-server architecture:

```
sb_isql (CLIENT)           sb_server (SERVER)
    |                          |
    |  TCP/Socket Connection   |
    |------------------------->|
    |   Send SQL string        |
    |------------------------->|
    |                          | Parse SQL (V2/Firebird/PostgreSQL/MySQL)
    |                          | Execute query
    |                          | Return results
    |<-------------------------|
    |   Receive results        |
```

**Parser selection happens on the SERVER side, not in the CLI.**

### Architecture Analysis

**Connection Establishment (line 3031-3047):**
```cpp
Connection conn;
g_connection = &conn;

ConnectionConfig conn_config;
conn_config.database_name = g_config.database_path;
conn_config.username = g_config.username;
conn_config.password = g_config.password;
conn_config.tcp_port = g_config.port;
conn_config.ipc_method = server::IPCMethod::TCP_LOCALHOST;

core::Status status = conn.connect(conn_config, &ctx);
```

**ConnectionConfig Structure (`include/scratchbird/client/connection.h:54-74`):**
```cpp
struct ConnectionConfig {
    std::string database_name;
    std::string username;
    std::string password;
    uint32_t connect_timeout_ms = 5000;
    uint32_t query_timeout_ms = 30000;
    // ... other settings ...
    server::IPCMethod ipc_method = server::IPCMethod::AUTO;
    uint16_t tcp_port = 3092;
    std::string socket_path;

    // ❌ NO parser selection field
};
```

**SQL Execution (line 1528):**
```cpp
core::Status status = g_connection->executeQuery(processed_sql, &results, &ctx);
```

The client simply sends SQL strings to the server. The **server** decides which parser to use.

---

## Implementation Options for Parser Selection

### Option 1: Add Parser Field to ConnectionConfig (Recommended)

**Pros:**
- Cleanest architecture
- Parser set once per connection
- Survives reconnections
- Can be stored in connection string

**Cons:**
- Requires modifying `ConnectionConfig` struct
- Requires server-side changes to honor parser selection

**Implementation:**

**Step 1:** Add parser field to `ConnectionConfig`
```cpp
// include/scratchbird/client/connection.h
struct ConnectionConfig {
    // ... existing fields ...

    // Parser selection
    enum class ParserType {
        AUTO,          // Let server decide (default)
        V2,            // ScratchBird V2 native parser
        FIREBIRD,      // Firebird SQL parser
        POSTGRESQL,    // PostgreSQL parser
        MYSQL          // MySQL parser
    };
    ParserType parser = ParserType::AUTO;
};
```

**Step 2:** Add `-par` flag to sb_isql
```cpp
// src/cli/sb_isql.cpp - add to IsqlConfig struct
struct IsqlConfig {
    // ... existing fields ...
    std::string parser_name;  // "V2", "Firebird", "PostgreSQL", "MySQL"
};

// In parseArgs() function:
} else if (arg == "-par" && i + 1 < argc) {
    g_config.parser_name = argv[++i];
} else if (arg.find("--parser=") == 0) {
    g_config.parser_name = arg.substr(9);
```

**Step 3:** Set parser in connection config
```cpp
// In main() before conn.connect():
if (!g_config.parser_name.empty()) {
    if (g_config.parser_name == "V2" || g_config.parser_name == "ScratchBird") {
        conn_config.parser = ConnectionConfig::ParserType::V2;
    } else if (g_config.parser_name == "Firebird" || g_config.parser_name == "FirebirdSQL") {
        conn_config.parser = ConnectionConfig::ParserType::FIREBIRD;
    } else if (g_config.parser_name == "PostgreSQL" || g_config.parser_name == "Postgres" || g_config.parser_name == "PG") {
        conn_config.parser = ConnectionConfig::ParserType::POSTGRESQL;
    } else if (g_config.parser_name == "MySQL") {
        conn_config.parser = ConnectionConfig::ParserType::MYSQL;
    } else {
        std::cerr << "Error: Unknown parser: " << g_config.parser_name << "\n";
        return 1;
    }
}
```

**Step 4:** Server-side implementation
- Server reads `parser` field from `ConnectionConfig`
- Sets session parser for this connection
- All SQL executed uses the selected parser

**Effort:** 4-8 hours (including server-side changes)

---

### Option 2: Use SET PARSER Command

**Pros:**
- No protocol changes
- Can switch parsers mid-session
- Works with existing infrastructure

**Cons:**
- Parser can be changed mid-test (less predictable)
- Requires remembering to set parser at start of scripts
- Not automatic

**Implementation:**

**Step 1:** Add `-par` flag to sb_isql (same as Option 1)

**Step 2:** Send `SET PARSER` command after connection
```cpp
// In main() after conn.connect():
if (!g_config.parser_name.empty()) {
    std::string set_parser_cmd = "SET PARSER " + g_config.parser_name;
    if (!executeSQL(set_parser_cmd)) {
        std::cerr << "Warning: Could not set parser to " << g_config.parser_name << "\n";
    }
}
```

**Step 3:** Server-side implementation
- Add `SET PARSER` to server's SET command handler
- Update session state with selected parser
- All subsequent SQL uses the selected parser

**Effort:** 2-4 hours (mostly server-side)

---

### Option 3: Use Schema-Based Parser Selection

**Pros:**
- Parser automatically selected based on schema prefix
- No explicit selection needed
- Matches ScratchBird's emulation architecture

**Cons:**
- Requires schema qualification in all SQL
- Less explicit control
- May not work for cross-schema queries

**Implementation:**

Use existing schema prefixes:
```sql
-- Automatically uses Firebird parser
USE emulation.firebird.mydb;
SELECT * FROM employees;

-- Automatically uses PostgreSQL parser
USE emulation.postgresql.mydb;
SELECT * FROM employees;

-- Automatically uses MySQL parser
USE emulation.mysql.mydb;
SELECT * FROM employees;
```

**For sb_isql:**
```bash
# Connect to Firebird emulated database
sb_isql "emulation.firebird.mydb" -f firebird_tests.sql

# Connect to PostgreSQL emulated database
sb_isql "emulation.postgresql.mydb" -f postgres_tests.sql
```

**Effort:** 1-2 hours (if server already supports this)

---

## Recommended Implementation Plan

### Phase 1: Add `-i` Alias (Quick Win)

**Effort:** 10 minutes
**Benefit:** Firebird ISQL compatibility

```cpp
// Add to parseArgs():
} else if (arg == "-i" && i + 1 < argc) {
    g_config.input_file = argv[++i];
```

### Phase 2: Add `-par` with SET PARSER (Medium Priority)

**Effort:** 2-4 hours
**Benefit:** Explicit parser control for testing

**Steps:**
1. Add `-par`/`--parser` flag to sb_isql (30 min)
2. Implement `SET PARSER` server command (1-2 hours)
3. Send `SET PARSER` after connection (15 min)
4. Test all 4 parsers (1 hour)
5. Update documentation (30 min)

### Phase 3: Add Parser to ConnectionConfig (Long-term)

**Effort:** 4-8 hours
**Benefit:** Cleaner architecture, parser set at connection time

**Steps:**
1. Add `parser` field to `ConnectionConfig` (30 min)
2. Modify wire protocol to send parser (1-2 hours)
3. Server reads parser from connection (1 hour)
4. Update sb_isql to set parser in config (30 min)
5. Test all parsers (1-2 hours)
6. Update documentation (1 hour)

---

## SQL Dialect vs Parser Selection

### Current: SQL Dialect (`-s`/`--dialect`)

**Flag:** `-s <1|2|3>` or `--dialect=<1|2|3>`

**Purpose:** Firebird SQL dialect selection
- **Dialect 1:** Legacy Firebird 1.0 (deprecated)
- **Dialect 2:** Transitional (deprecated)
- **Dialect 3:** Modern Firebird (default)

**Usage:**
```bash
sb_isql mydb.sbdb -s 3  # Use Firebird SQL dialect 3
```

**Code Location:** `src/cli/sb_isql.cpp:2965-2978`

**Note:** This is **NOT** parser selection. This is a Firebird-specific setting for SQL syntax variations within the Firebird parser.

### Proposed: Parser Selection (`-par`/`--parser`)

**Flag:** `-par <parser>` or `--parser=<parser>`

**Purpose:** Select which SQL parser to use
- **V2/ScratchBird:** ScratchBird native parser
- **Firebird:** Firebird SQL parser
- **PostgreSQL:** PostgreSQL parser
- **MySQL:** MySQL parser

**Usage:**
```bash
sb_isql mydb.sbdb -par Firebird -s 3  # Firebird parser with dialect 3
```

**Difference:**
- `-s/--dialect` = Firebird syntax variant (1/2/3)
- `-par/--parser` = Which database's SQL parser to use

---

## Testing Use Cases

### Use Case 1: Run Firebird Compatibility Tests

```bash
#!/bin/bash
# Test ScratchBird's Firebird parser against official Firebird test suite

for test in tests/compatibility/firebird/selected_tests/*.sql; do
    output="${test%.sql}.out"
    expected="${test%.sql}.expected"

    sb_isql testdb.sbdb \
        -par Firebird \
        -s 3 \
        -f "$test" \
        -o "$output" \
        -q

    diff "$output" "$expected"
    if [ $? -eq 0 ]; then
        echo "✅ PASS: $test"
    else
        echo "❌ FAIL: $test"
    fi
done
```

### Use Case 2: Run PostgreSQL Recursive CTE Tests

```bash
#!/bin/bash
# Test ScratchBird's PostgreSQL parser with PostgreSQL's with.sql tests

sb_isql testdb.sbdb \
    -par PostgreSQL \
    -f tests/compatibility/postgresql/with.sql \
    -o tests/compatibility/postgresql/with.out \
    -q \
    -b  # Bail on first error

diff tests/compatibility/postgresql/with.out \
     tests/compatibility/postgresql/with.expected
```

### Use Case 3: Cross-Parser Comparison

```bash
#!/bin/bash
# Run same test with all parsers and compare results

TEST="tests/compatibility/common/basic_select.sql"

for PARSER in V2 Firebird PostgreSQL MySQL; do
    sb_isql testdb.sbdb \
        -par $PARSER \
        -f "$TEST" \
        -o "results_${PARSER}.out" \
        -q
done

# Compare results
diff results_V2.out results_Firebird.out
diff results_V2.out results_PostgreSQL.out
diff results_V2.out results_MySQL.out
```

### Use Case 4: Automated Test Suite

```bash
#!/bin/bash
# Run all compatibility tests for all parsers

PARSERS=("Firebird" "PostgreSQL" "MySQL")
TOTAL=0
PASSED=0
FAILED=0

for PARSER in "${PARSERS[@]}"; do
    echo "Testing $PARSER parser..."

    for TEST in tests/compatibility/${PARSER,,}/*.sql; do
        TOTAL=$((TOTAL + 1))

        OUTPUT="${TEST%.sql}.out"
        EXPECTED="${TEST%.sql}.expected"

        sb_isql testdb.sbdb \
            -par $PARSER \
            -f "$TEST" \
            -o "$OUTPUT" \
            -q -b

        if diff -q "$OUTPUT" "$EXPECTED" > /dev/null 2>&1; then
            PASSED=$((PASSED + 1))
            echo "  ✅ $TEST"
        else
            FAILED=$((FAILED + 1))
            echo "  ❌ $TEST"
        fi
    done
done

echo ""
echo "Results: $PASSED passed, $FAILED failed out of $TOTAL total"
```

---

## Summary of Findings

### Currently Supported ✅

| Feature | Flag | Status | Notes |
|---------|------|--------|-------|
| Output redirection | `-o <file>` | ✅ Working | Fully functional, ready for testing |
| Input file | `-f <file>` | ✅ Working | Fully functional, ready for testing |
| Input meta-command | `\i <file>` | ✅ Working | Interactive alternative to `-f` |
| Output meta-command | `\o <file>` | ✅ Working | Interactive alternative to `-o` |
| SQL Dialect | `-s <1\|2\|3>` | ✅ Working | Firebird dialect selection (NOT parser) |

### Not Currently Supported ❌

| Feature | Requested | Status | Effort | Priority |
|---------|-----------|--------|--------|----------|
| Input alias | `-i <file>` | ❌ Missing | 10 min | Low |
| Parser selection | `-par <parser>` | ❌ Missing | 2-8 hours | **HIGH** |

### Implementation Priority

1. **HIGH:** `-par`/`--parser` flag (2-4 hours via SET PARSER)
2. **LOW:** `-i` alias to `-f` (10 minutes)

---

## Recommended Next Steps

### Step 1: Implement SET PARSER Command (Server-Side)

**File:** `src/sblr/executor.cpp` or similar

**Add SET PARSER handler:**
```cpp
if (upper_cmd == "SET PARSER") {
    std::string parser_name = /* extract from command */;

    if (parser_name == "V2" || parser_name == "SCRATCHBIRD") {
        session->setParser(ParserType::V2);
    } else if (parser_name == "FIREBIRD" || parser_name == "FIREBIRDSQL") {
        session->setParser(ParserType::FIREBIRD);
    } else if (parser_name == "POSTGRESQL" || parser_name == "POSTGRES" || parser_name == "PG") {
        session->setParser(ParserType::POSTGRESQL);
    } else if (parser_name == "MYSQL") {
        session->setParser(ParserType::MYSQL);
    } else {
        return Status::ERROR("Unknown parser: " + parser_name);
    }

    return Status::OK;
}
```

### Step 2: Add `-par` Flag to sb_isql

**File:** `src/cli/sb_isql.cpp`

**Add to IsqlConfig:**
```cpp
struct IsqlConfig {
    // ... existing fields ...
    std::string parser_name;  // "V2", "Firebird", "PostgreSQL", "MySQL"
};
```

**Add to parseArgs():**
```cpp
} else if (arg == "-par" && i + 1 < argc) {
    g_config.parser_name = argv[++i];
} else if (arg.find("--parser=") == 0) {
    g_config.parser_name = arg.substr(9);
```

**Add to main() after connect:**
```cpp
if (!g_config.parser_name.empty()) {
    std::string set_parser = "SET PARSER " + g_config.parser_name;
    if (!executeSQL(set_parser)) {
        std::cerr << "Warning: Could not set parser to " << g_config.parser_name << "\n";
    } else if (g_config.verbose) {
        std::cout << "Parser set to: " << g_config.parser_name << "\n";
    }
}
```

**Add to help text:**
```cpp
std::cout << "  -par, --parser=<parser>   SQL parser (V2, Firebird, PostgreSQL, MySQL)\n";
```

### Step 3: Add `-i` Alias (Quick Win)

**File:** `src/cli/sb_isql.cpp`

**Add to parseArgs():**
```cpp
} else if (arg == "-i" && i + 1 < argc) {
    g_config.input_file = argv[++i];
```

**Add to help text:**
```cpp
std::cout << "  -i, --input=<file>        Execute commands from file (alias for -f)\n";
```

### Step 4: Test Implementation

```bash
# Test parser selection
sb_isql testdb.sbdb -par Firebird -c "SELECT 1 FROM RDB\$DATABASE"
sb_isql testdb.sbdb -par PostgreSQL -c "SELECT version()"
sb_isql testdb.sbdb -par MySQL -c "SELECT @@version"

# Test input/output redirection
sb_isql testdb.sbdb -par Firebird -i test.sql -o results.txt

# Test all together
sb_isql testdb.sbdb -par PostgreSQL -f with_recursive.sql -o with.out -q -b
```

---

## Conclusion

sb_isql has **excellent support** for output redirection (`-o`) and input file execution (`-f`), making it ready for automated testing **TODAY**.

The main gap is **parser selection** (`-par`), which requires:
1. Server-side `SET PARSER` command implementation (1-2 hours)
2. Client-side `-par` flag (30 minutes)
3. Testing (1 hour)

**Total effort: 2-4 hours**

Once `-par` is implemented, sb_isql will be fully equipped to run the FirebirdSQL, MySQL, and PostgreSQL compatibility test suites identified in `SQL_COMPATIBILITY_TEST_REPOSITORIES.md`.

---

**Document Created:** 2025-12-28
**Author:** Claude Code
**Status:** ✅ Ready for implementation
**Next Steps:** Implement `-par` flag and SET PARSER command
