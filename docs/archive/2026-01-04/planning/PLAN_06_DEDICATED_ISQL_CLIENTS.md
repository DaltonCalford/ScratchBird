# Plan 06: Dedicated ISQL Clients for Multi-Database Emulation Testing

**Plan Status:** 📋 SPECIFICATION PHASE
**Version:** 1.0
**Created:** 2025-12-28
**Dependencies:** Server-side wire protocol listeners (verification needed)
**Estimated Effort:** 80-120 hours total (3 clients)

---

## Document Purpose

This plan provides **complete specifications** for implementing three dedicated ISQL clients:
1. `sb_fb_isql` - Firebird emulation client
2. `sb_pg_isql` - PostgreSQL emulation client
3. `sb_my_isql` - MySQL emulation client

**This specification is designed so that:**
- Another AI can implement each client independently
- Test infrastructure can be built in parallel
- All clients share common patterns for consistency

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Shared Component Library](#shared-component-library)
3. [sb_fb_isql Specification](#sb_fb_isql-specification)
4. [sb_pg_isql Specification](#sb_pg_isql-specification)
5. [sb_my_isql Specification](#sb_my_isql-specification)
6. [Build System Integration](#build-system-integration)
7. [Testing Approach](#testing-approach)
8. [Implementation Timeline](#implementation-timeline)

---

## Architecture Overview

### Design Principles

1. **Wire Protocol Purity:** Each client speaks ONLY its database's native wire protocol
2. **Command-Line Compatibility:** Match native ISQL tools' command-line syntax
3. **Code Sharing:** Use shared library for common functionality (arg parsing, I/O, output formatting)
4. **Independent Development:** Each client can be developed independently
5. **Test Automation:** All clients must support batch mode with input/output redirection

### High-Level Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          Client Applications                              │
├──────────────────┬──────────────────┬──────────────────┬─────────────────┤
│   sb_fb_isql     │   sb_pg_isql     │   sb_my_isql     │    sb_isql      │
│  (Firebird CLI)  │ (PostgreSQL CLI) │   (MySQL CLI)    │ (ScratchBird)   │
└────────┬─────────┴────────┬─────────┴────────┬─────────┴────────┬────────┘
         │                  │                  │                  │
         │                  │                  │                  │
         ▼                  ▼                  ▼                  ▼
┌──────────────────┐┌──────────────────┐┌──────────────────┐┌──────────────┐
│  Firebird Wire   ││ PostgreSQL Wire  ││   MySQL Wire     ││ScratchBird   │
│  Protocol Client ││ Protocol Client  ││ Protocol Client  ││Native Client │
└────────┬─────────┘└────────┬─────────┘└────────┬─────────┘└──────┬───────┘
         │                   │                   │                 │
         │ Port 3050         │ Port 5432         │ Port 3306       │ Port 5433
         │ XDR Protocol      │ FE/BE Protocol    │ Text Protocol   │ Native
         │                   │                   │                 │
         └───────────────────┴───────────────────┴─────────────────┘
                                     │
                                     ▼
                        ┌────────────────────────┐
                        │  ScratchBird Server    │
                        │  (Multi-Protocol)      │
                        └────────────────────────┘
```

### Shared Component Library

Create a shared library `libsb_isql_common` for code reuse:

**Location:** `src/cli/isql_common/`

**Components:**
```cpp
// Command-line argument parsing utilities
class ArgParser {
public:
    void addFlag(const std::string& short_form, const std::string& long_form,
                 const std::string& description, bool requires_value = false);
    bool parse(int argc, char** argv);
    std::string getValue(const std::string& flag);
    bool hasFlag(const std::string& flag);
};

// Input/Output management
class IOManager {
public:
    // Input from file or stdin
    std::istream& getInput();
    void setInputFile(const std::string& path);

    // Output to file or stdout
    std::ostream& getOutput();
    void setOutputFile(const std::string& path);

    // Error output
    std::ostream& getError();
    void setErrorFile(const std::string& path);
};

// Output formatting (tables, aligned, unaligned, etc.)
class OutputFormatter {
public:
    enum class Format {
        ALIGNED,        // ASCII art tables
        UNALIGNED,      // Tab/delimiter separated
        VERTICAL,       // One field per line
        HTML,           // HTML tables
        CSV             // Comma-separated
    };

    void setFormat(Format format);
    void setFieldSeparator(const std::string& sep);
    void formatResultSet(const ResultSet& results, std::ostream& out);
};

// Connection management base
class ConnectionBase {
public:
    virtual ~ConnectionBase() = default;
    virtual bool connect(const ConnectionParams& params) = 0;
    virtual bool disconnect() = 0;
    virtual bool executeQuery(const std::string& sql, ResultSet& results) = 0;
    virtual std::string getLastError() = 0;
};

// Result set abstraction
class ResultSet {
public:
    size_t getColumnCount() const;
    std::string getColumnName(size_t index) const;
    std::string getColumnType(size_t index) const;

    bool nextRow();
    std::string getValue(size_t column) const;
    bool isNull(size_t column) const;

    size_t getRowCount() const;
};
```

**Estimated Effort:** 16-24 hours (implement once, use for all clients)

---

## sb_fb_isql Specification

### Overview

**Purpose:** Firebird-compatible ISQL client for testing Firebird emulation

**Wire Protocol:** Firebird XDR (External Data Representation)

**Target Compatibility:** Firebird 5.0 isql

**Estimated Effort:** 27-40 hours

### Command-Line Syntax

```
Usage: sb_fb_isql [options] [<database>]

Connection Options:
  -user <username>          Username (can also use -u)
  -password <password>      Password (can also use -p)
  -role <rolename>          SQL role name (can also use -r)

Input/Output Options:
  -input <filename>         Read SQL from file (can also use -i)
  -output <filename>        Write output to file (can also use -o)
  -echo                     Echo commands (can also use -e)
  -quiet                    Quiet mode - no banner (can also use -q)

SQL Dialect:
  -sqldialect <1|2|3>       SQL dialect (default: 3)

Extraction:
  -extract                  Extract DDL (can also use -x)
  -extract_all              Extract all DDL (can also use -a)

Execution Control:
  -nod                      No plan output (can also use -n)
  -z                        Show execution plan

Other:
  -merge                    Merge stderr to stdout (can also use -m)
  -bail                     Bail on first error (can also use -b)
  -help                     Show this help (can also use -h)

Examples:
  sb_fb_isql employee.fdb -u SYSDBA -p masterkey
  sb_fb_isql -i test.sql -o results.txt employee.fdb -user SYSDBA
  sb_fb_isql employee.fdb -extract > schema.sql
```

### Firebird Wire Protocol Implementation

**Protocol Specification:** Based on Firebird 5.0 protocol (version 13)

**Protocol Documentation:** See `/docs/specifications/wire_protocols/firebird_wire_protocol.md`

#### Connection Sequence

```
1. Client → Server: op_connect
   - Protocol version (13)
   - Architecture (1 = little endian)
   - Client description string
   - User authentication plugin (SRP or Legacy)

2. Server → Client: op_accept or op_cond_accept
   - Accepted protocol version
   - Architecture
   - Auth plugin to use

3. Client → Server: op_attach (attach to database)
   - Database path
   - DPB (Database Parameter Buffer):
     - isc_dpb_user_name
     - isc_dpb_password (or auth data)
     - isc_dpb_sql_dialect
     - isc_dpb_lc_ctype (character set)

4. Server → Client: op_response
   - Database handle
   - Attachment success/failure

5. For each SQL statement:
   Client → Server: op_allocate_statement
   Server → Client: op_response (statement handle)

   Client → Server: op_prepare_statement
   Server → Client: op_response (prepared statement info)

   Client → Server: op_info_sql (get result set metadata)
   Server → Client: op_response (column info)

   Client → Server: op_execute or op_execute2
   Server → Client: op_response (execution result)

   Client → Server: op_fetch (get result rows)
   Server → Client: op_fetch_response (row data)
   (Repeat until op_response with status)

6. Client → Server: op_commit or op_rollback
   Server → Client: op_response

7. Client → Server: op_detach
   Server → Client: op_response
```

#### XDR Encoding

All data is XDR-encoded (platform-independent binary format):

```cpp
// Example: Encode 32-bit integer
void encodeXdrInt32(std::vector<uint8_t>& buffer, int32_t value) {
    buffer.push_back((value >> 24) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

// Example: Decode 32-bit integer
int32_t decodeXdrInt32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

// Strings are length-prefixed and padded to 4-byte boundaries
void encodeXdrString(std::vector<uint8_t>& buffer, const std::string& str) {
    uint32_t len = str.length();
    encodeXdrInt32(buffer, len);
    buffer.insert(buffer.end(), str.begin(), str.end());

    // Pad to 4-byte boundary
    while (buffer.size() % 4 != 0) {
        buffer.push_back(0);
    }
}
```

#### Operation Codes (Opcodes)

```cpp
enum FirebirdOpcode {
    op_connect = 1,
    op_attach = 19,
    op_create = 20,
    op_detach = 21,
    op_compile = 22,
    op_start_and_send = 24,
    op_send = 25,
    op_receive = 26,
    op_allocate_statement = 62,
    op_execute = 63,
    op_fetch = 65,
    op_prepare_statement = 70,
    op_info_sql = 71,
    op_execute2 = 76,
    op_commit = 35,
    op_rollback = 37,
    op_response = 9,
    op_accept = 11,
    op_cond_accept = 15,
    op_fetch_response = 14,
    // ... more opcodes
};
```

### SET Commands

Implement these Firebird ISQL SET commands:

```cpp
class FirebirdSessionState {
public:
    // SET TERM <char>; - Change statement terminator
    std::string terminator = ";";
    void setTerm(const std::string& term) { terminator = term; }

    // SET SQL DIALECT <1|2|3>;
    int sql_dialect = 3;
    void setSqlDialect(int dialect) {
        if (dialect >= 1 && dialect <= 3) {
            sql_dialect = dialect;
        }
    }

    // SET NAMES <charset>;
    std::string charset = "UTF8";
    void setNames(const std::string& cs) { charset = cs; }

    // SET BAIL [ON|OFF];
    bool bail_on_error = false;

    // SET ECHO [ON|OFF];
    bool echo_commands = false;

    // SET STATS [ON|OFF];
    bool show_stats = false;

    // SET PLAN [ON|OFF];
    bool show_plan = false;

    // SET COUNT [ON|OFF];
    bool show_count = true;

    // SET LIST [ON|OFF];
    bool vertical_format = false;
};
```

### Statement Parsing

```cpp
class StatementParser {
public:
    struct Statement {
        std::string sql;
        bool is_set_command;
        bool is_commit;
        bool is_rollback;
    };

    // Parse input considering custom terminator
    std::vector<Statement> parseStatements(const std::string& input,
                                          const std::string& terminator);

    // Handle SET commands locally
    bool handleSetCommand(const std::string& sql, FirebirdSessionState& state);
};
```

### File Structure

```
src/cli/sb_fb_isql/
├── main.cpp                    # Entry point, arg parsing
├── firebird_connection.h       # Firebird wire protocol connection
├── firebird_connection.cpp
├── firebird_protocol.h         # XDR encoding/decoding
├── firebird_protocol.cpp
├── firebird_session.h          # Session state, SET commands
├── firebird_session.cpp
├── statement_parser.h          # SQL statement parsing
├── statement_parser.cpp
├── output_formatter.h          # Result formatting
├── output_formatter.cpp
└── CMakeLists.txt

include/scratchbird/cli/firebird/
├── connection.h
└── protocol.h
```

### Implementation Checklist

**Phase 1: Basic Connection (6-8 hours)**
- [ ] Implement XDR encoding/decoding utilities
- [ ] Implement op_connect handshake
- [ ] Implement op_attach database connection
- [ ] Implement op_detach disconnect
- [ ] Test basic connection to ScratchBird server on port 3050

**Phase 2: Query Execution (8-12 hours)**
- [ ] Implement op_allocate_statement
- [ ] Implement op_prepare_statement
- [ ] Implement op_info_sql (metadata)
- [ ] Implement op_execute / op_execute2
- [ ] Implement op_fetch result set retrieval
- [ ] Test simple SELECT queries

**Phase 3: Command-Line Interface (4-6 hours)**
- [ ] Implement argument parsing (use shared ArgParser)
- [ ] Implement input file reading
- [ ] Implement output file writing
- [ ] Implement interactive mode (REPL)
- [ ] Test with -i and -o flags

**Phase 4: SET Commands (3-4 hours)**
- [ ] Implement SET TERM parsing
- [ ] Implement SET SQL DIALECT
- [ ] Implement SET BAIL/ECHO/STATS/PLAN/COUNT/LIST
- [ ] Test statement parsing with custom terminator

**Phase 5: Transaction Control (2-3 hours)**
- [ ] Implement COMMIT (op_commit)
- [ ] Implement ROLLBACK (op_rollback)
- [ ] Handle autocommit vs explicit transactions

**Phase 6: Output Formatting (4-6 hours)**
- [ ] Implement aligned table output (like Firebird ISQL)
- [ ] Implement unaligned output
- [ ] Implement vertical output (SET LIST)
- [ ] Handle NULL values
- [ ] Test output formatting

---

## sb_pg_isql Specification

### Overview

**Purpose:** PostgreSQL-compatible psql client for testing PostgreSQL emulation

**Wire Protocol:** PostgreSQL Frontend/Backend Protocol v3.0

**Target Compatibility:** PostgreSQL 15+ psql

**Estimated Effort:** 27-40 hours

### Command-Line Syntax

```
Usage: sb_pg_isql [OPTION]... [DBNAME [USERNAME]]

General options:
  -c, --command=COMMAND       Execute single command and exit
  -f, --file=FILENAME         Execute commands from file
  -l, --list                  List available databases
  -v, --version               Show version
  --help                      Show this help

Input and output options:
  -a, --echo-all              Echo all input
  -e, --echo-queries          Echo commands sent to server
  -E, --echo-hidden           Display hidden queries (\d commands)
  -o, --output=FILENAME       Send query results to file
  -q, --quiet                 Run quietly (no welcome)
  -t, --tuples-only           Print tuples only
  -A, --no-align              Unaligned table output
  -F, --field-separator=SEP   Field separator (default: |)
  -R, --record-separator=SEP  Record separator (default: newline)

Connection options:
  -h, --host=HOSTNAME         Database host (default: localhost)
  -p, --port=PORT             Database port (default: 5432)
  -U, --username=USERNAME     Database username
  -d, --dbname=DBNAME         Database name
  -W, --password              Force password prompt
  -w, --no-password           Never prompt for password

Examples:
  sb_pg_isql testdb
  sb_pg_isql -U postgres -d testdb -f test.sql -o results.txt
  sb_pg_isql -c "SELECT version()" -t -A
```

### PostgreSQL Wire Protocol Implementation

**Protocol Specification:** Frontend/Backend Protocol 3.0

**Protocol Documentation:** See `/docs/specifications/wire_protocols/postgresql_wire_protocol.md`

#### Connection Sequence

```
1. Optional: SSL Request
   Client → Server: SSLRequest (4-byte: 0x00000008, 0x04d2162f)
   Server → Client: 'S' (SSL supported) or 'N' (no SSL)
   If 'S', upgrade to TLS

2. Startup
   Client → Server: StartupMessage
   - Protocol version: 196608 (3.0)
   - Parameters:
     - user: <username>
     - database: <dbname>
     - application_name: sb_pg_isql
     - client_encoding: UTF8

3. Authentication
   Server → Client: AuthenticationX message
   - AuthenticationOk (type 0)
   - AuthenticationCleartextPassword (type 3)
   - AuthenticationMD5Password (type 5)
   - AuthenticationSASL (type 10) - SCRAM-SHA-256

   Client → Server: PasswordMessage (or SASL responses)

4. Server → Client: AuthenticationOk (if successful)

5. Server → Client: BackendKeyData (process ID, secret key for cancel)

6. Server → Client: ParameterStatus messages (server settings)

7. Server → Client: ReadyForQuery (transaction status)

8. For each query:
   Client → Server: Query message
   - SQL string (null-terminated)

   Server → Client: RowDescription (column metadata)
   Server → Client: DataRow messages (result rows)
   Server → Client: CommandComplete (rows affected)
   Server → Client: ReadyForQuery

9. Client → Server: Terminate
```

#### Message Format

All messages (except StartupMessage and SSLRequest):

```
[Type: 1 byte] [Length: 4 bytes] [Data: Length-4 bytes]
```

Example messages:

```cpp
// Query message (type 'Q')
struct QueryMessage {
    char type = 'Q';
    int32_t length;      // Including itself
    std::string sql;     // Null-terminated
};

void sendQuery(int socket, const std::string& sql) {
    std::vector<uint8_t> buffer;
    buffer.push_back('Q');

    // Length = 4 (length field) + sql.length() + 1 (null terminator)
    int32_t length = htonl(4 + sql.length() + 1);
    buffer.insert(buffer.end(), (uint8_t*)&length, (uint8_t*)&length + 4);

    buffer.insert(buffer.end(), sql.begin(), sql.end());
    buffer.push_back(0);  // Null terminator

    send(socket, buffer.data(), buffer.size(), 0);
}

// RowDescription message (type 'T')
struct RowDescription {
    char type = 'T';
    int32_t length;
    int16_t field_count;

    struct Field {
        std::string name;  // Null-terminated
        int32_t table_oid;
        int16_t column_attr_number;
        int32_t type_oid;
        int16_t type_size;
        int32_t type_modifier;
        int16_t format_code;  // 0 = text, 1 = binary
    };

    std::vector<Field> fields;
};

// DataRow message (type 'D')
struct DataRow {
    char type = 'D';
    int32_t length;
    int16_t field_count;

    struct FieldValue {
        int32_t length;  // -1 if NULL
        std::vector<uint8_t> data;
    };

    std::vector<FieldValue> values;
};
```

### Meta-Commands (Backslash Commands)

Implement these psql meta-commands:

```cpp
class PostgresqlMetaCommands {
public:
    // \d [table] - Describe table or list tables
    bool describeTable(const std::string& table_name);

    // \dt - List tables
    bool listTables();

    // \di - List indexes
    bool listIndexes();

    // \dv - List views
    bool listViews();

    // \df [pattern] - List functions
    bool listFunctions(const std::string& pattern = "");

    // \du - List users/roles
    bool listUsers();

    // \l - List databases
    bool listDatabases();

    // \c <database> - Connect to database
    bool connectTo(const std::string& dbname);

    // \i <file> - Execute file
    bool executeFile(const std::string& filename);

    // \o [file] - Redirect output
    bool setOutputFile(const std::string& filename);

    // \timing [on|off] - Toggle timing
    bool setTiming(bool enable);

    // \x [on|off|auto] - Toggle expanded output
    bool setExpanded(const std::string& mode);

    // \! <command> - Execute shell command
    bool executeShell(const std::string& command);
};
```

### File Structure

```
src/cli/sb_pg_isql/
├── main.cpp                    # Entry point, arg parsing
├── postgresql_connection.h     # PostgreSQL FE/BE protocol connection
├── postgresql_connection.cpp
├── postgresql_protocol.h       # Message encoding/decoding
├── postgresql_protocol.cpp
├── postgresql_session.h        # Session state, meta-commands
├── postgresql_session.cpp
├── meta_commands.h             # Backslash command handlers
├── meta_commands.cpp
├── output_formatter.h          # Result formatting (psql-style)
├── output_formatter.cpp
└── CMakeLists.txt
```

### Implementation Checklist

**Phase 1: Basic Connection (6-8 hours)**
- [ ] Implement StartupMessage
- [ ] Implement authentication (cleartext, MD5, SCRAM-SHA-256)
- [ ] Handle BackendKeyData
- [ ] Handle ParameterStatus messages
- [ ] Handle ReadyForQuery
- [ ] Test connection to ScratchBird server on port 5432

**Phase 2: Query Execution (8-12 hours)**
- [ ] Implement Query message
- [ ] Parse RowDescription
- [ ] Parse DataRow
- [ ] Handle CommandComplete
- [ ] Handle ErrorResponse
- [ ] Test simple SELECT queries

**Phase 3: Command-Line Interface (4-6 hours)**
- [ ] Implement argument parsing
- [ ] Implement input file reading
- [ ] Implement output file writing
- [ ] Implement interactive mode (REPL with readline)
- [ ] Test with -f and -o flags

**Phase 4: Meta-Commands (6-8 hours)**
- [ ] Implement \\d (describe)
- [ ] Implement \\dt (tables)
- [ ] Implement \\di (indexes)
- [ ] Implement \\dv (views)
- [ ] Implement \\i (include file)
- [ ] Implement \\o (output file)
- [ ] Implement \\timing
- [ ] Implement \\x (expanded)

**Phase 5: Transaction Control (2-3 hours)**
- [ ] Send BEGIN/COMMIT/ROLLBACK
- [ ] Track transaction state from ReadyForQuery
- [ ] Display transaction status in prompt

**Phase 6: Output Formatting (4-6 hours)**
- [ ] Implement psql-style aligned table output
- [ ] Implement unaligned output (-A flag)
- [ ] Implement tuples-only output (-t flag)
- [ ] Implement expanded output (\\x)
- [ ] Handle NULL values

---

## sb_my_isql Specification

### Overview

**Purpose:** MySQL-compatible mysql client for testing MySQL emulation

**Wire Protocol:** MySQL Client/Server Text Protocol

**Target Compatibility:** MySQL 8.0+ mysql client

**Estimated Effort:** 27-40 hours

### Command-Line Syntax

```
Usage: sb_my_isql [OPTIONS] [database]

General:
  -?, --help                Display this help
  -V, --version             Show version
  -v, --verbose             Verbose mode

Connection:
  -h, --host=HOST           Host (default: localhost)
  -P, --port=PORT           Port (default: 3306)
  -u, --user=USER           Username
  -p[password]              Password (prompted if not inline)
  -D, --database=DB         Database name
  -S, --socket=SOCKET       Socket file

Execution:
  -e, --execute=STATEMENT   Execute statement and quit
  -B, --batch               Batch mode (no table formatting)
  -n, --unbuffered          Flush buffer after each query
  -N, --skip-column-names   No column names in results

Output:
  -s, --silent              Silent mode (no table, only data)
  -t                        Display table output
  -E, --vertical            Vertical output format
  -H, --html                HTML output
  --delimiter=STR           Set delimiter (default: ;)

Other:
  --default-character-set=CHARSET  Character set

Examples:
  sb_my_isql -u root -p mydb
  sb_my_isql -u root -p -D mydb < test.sql > results.txt
  sb_my_isql -u root -p -e "SELECT @@version"
```

### MySQL Wire Protocol Implementation

**Protocol Specification:** MySQL Client/Server Protocol

**Protocol Documentation:** See `/docs/specifications/wire_protocols/mysql_wire_protocol.md`

#### Connection Sequence

```
1. Server → Client: Initial Handshake Packet
   - Protocol version (10)
   - Server version string
   - Connection ID
   - Auth plugin data (salt)
   - Capability flags
   - Character set
   - Status flags
   - Auth plugin name

2. Client → Server: Handshake Response Packet
   - Capability flags (what client supports)
   - Max packet size
   - Character set
   - Username
   - Auth response (password hash)
   - Database name (optional)
   - Auth plugin name

3. Server → Client: OK Packet or ERR Packet
   - If OK: connected successfully
   - If ERR: authentication failed

4. For each query:
   Client → Server: COM_QUERY packet
   - Command byte: 0x03 (COM_QUERY)
   - SQL string

   Server → Client: Result Set or OK/ERR
   - Column Count packet (if result set)
   - Column Definition packets (one per column)
   - EOF packet (end of column definitions)
   - Row Data packets (result rows)
   - EOF packet (end of rows) or OK packet

5. Client → Server: COM_QUIT (0x01)
```

#### Packet Format

All packets have this structure:

```
[Payload Length: 3 bytes] [Sequence ID: 1 byte] [Payload]
```

Example:

```cpp
struct MySQLPacket {
    uint32_t payload_length:24;  // 3 bytes
    uint8_t sequence_id;
    std::vector<uint8_t> payload;
};

void sendPacket(int socket, uint8_t seq_id, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> packet;

    // Length (3 bytes, little endian)
    uint32_t len = payload.size();
    packet.push_back(len & 0xFF);
    packet.push_back((len >> 8) & 0xFF);
    packet.push_back((len >> 16) & 0xFF);

    // Sequence ID
    packet.push_back(seq_id);

    // Payload
    packet.insert(packet.end(), payload.begin(), payload.end());

    send(socket, packet.data(), packet.size(), 0);
}

// COM_QUERY packet
void sendQuery(int socket, const std::string& sql) {
    std::vector<uint8_t> payload;
    payload.push_back(0x03);  // COM_QUERY
    payload.insert(payload.end(), sql.begin(), sql.end());

    sendPacket(socket, 0, payload);  // Sequence starts at 0
}
```

#### Response Packets

```cpp
// OK Packet (first byte: 0x00)
struct OKPacket {
    uint8_t header = 0x00;
    uint64_t affected_rows;  // Length-encoded integer
    uint64_t last_insert_id; // Length-encoded integer
    uint16_t status_flags;
    uint16_t warnings;
    std::string info;        // Human-readable status
};

// ERR Packet (first byte: 0xFF)
struct ERRPacket {
    uint8_t header = 0xFF;
    uint16_t error_code;
    char sql_state_marker = '#';
    char sql_state[5];
    std::string error_message;
};

// EOF Packet (first byte: 0xFE, length < 9)
struct EOFPacket {
    uint8_t header = 0xFE;
    uint16_t warnings;
    uint16_t status_flags;
};

// Result Set:
// 1. Column Count packet
struct ColumnCountPacket {
    uint64_t column_count;  // Length-encoded integer
};

// 2. Column Definition packet (one per column)
struct ColumnDefinitionPacket {
    std::string catalog;     // Length-encoded string (always "def")
    std::string schema;      // Database name
    std::string table;       // Virtual table name
    std::string org_table;   // Physical table name
    std::string name;        // Virtual column name
    std::string org_name;    // Physical column name
    uint8_t length_fixed_fields = 0x0C;
    uint16_t character_set;
    uint32_t column_length;
    uint8_t column_type;
    uint16_t flags;
    uint8_t decimals;
    uint16_t filler = 0x0000;
};

// 3. Row Data packet
struct RowDataPacket {
    std::vector<std::string> values;  // Length-encoded strings (NULL = 0xFB)
};
```

### MySQL Client Commands

```cpp
enum MySQLCommand {
    COM_QUIT = 0x01,
    COM_INIT_DB = 0x02,         // USE database
    COM_QUERY = 0x03,
    COM_FIELD_LIST = 0x04,      // SHOW COLUMNS
    COM_CREATE_DB = 0x05,
    COM_DROP_DB = 0x06,
    COM_REFRESH = 0x07,
    COM_SHUTDOWN = 0x08,
    COM_STATISTICS = 0x09,
    COM_PROCESS_INFO = 0x0A,
    COM_CONNECT = 0x0B,
    COM_PROCESS_KILL = 0x0C,
    COM_DEBUG = 0x0D,
    COM_PING = 0x0E,
    COM_CHANGE_USER = 0x11,
    COM_RESET_CONNECTION = 0x1F,
    COM_SET_OPTION = 0x1B,
};
```

### MySQL Client Session

```cpp
class MySQLSession {
public:
    // Delimiter handling
    std::string delimiter = ";";
    void setDelimiter(const std::string& delim) { delimiter = delim; }

    // USE database
    bool selectDatabase(const std::string& dbname);

    // Session variables
    std::map<std::string, std::string> variables;
    void setVariable(const std::string& name, const std::string& value);

    // Batch mode vs interactive
    bool batch_mode = false;

    // Output mode
    bool silent_mode = false;
    bool skip_column_names = false;
    bool vertical_format = false;
};
```

### File Structure

```
src/cli/sb_my_isql/
├── main.cpp                    # Entry point, arg parsing
├── mysql_connection.h          # MySQL protocol connection
├── mysql_connection.cpp
├── mysql_protocol.h            # Packet encoding/decoding
├── mysql_protocol.cpp
├── mysql_session.h             # Session state, commands
├── mysql_session.cpp
├── output_formatter.h          # Result formatting (mysql-style)
├── output_formatter.cpp
└── CMakeLists.txt
```

### Implementation Checklist

**Phase 1: Basic Connection (6-8 hours)**
- [ ] Parse initial handshake packet
- [ ] Implement handshake response
- [ ] Implement authentication (mysql_native_password, sha256_password)
- [ ] Handle OK/ERR packets
- [ ] Test connection to ScratchBird server on port 3306

**Phase 2: Query Execution (8-12 hours)**
- [ ] Implement COM_QUERY
- [ ] Parse column count packet
- [ ] Parse column definition packets
- [ ] Parse EOF packet
- [ ] Parse row data packets
- [ ] Handle OK/ERR responses
- [ ] Test simple SELECT queries

**Phase 3: Command-Line Interface (4-6 hours)**
- [ ] Implement argument parsing
- [ ] Implement stdin input (`<` redirection)
- [ ] Implement stdout output (`>` redirection)
- [ ] Implement -e (execute) mode
- [ ] Implement interactive mode

**Phase 4: MySQL Commands (4-6 hours)**
- [ ] Implement USE database (COM_INIT_DB)
- [ ] Implement SOURCE file (read and execute)
- [ ] Implement DELIMITER command
- [ ] Implement SHOW commands
- [ ] Implement DESCRIBE command

**Phase 5: Transaction Control (2-3 hours)**
- [ ] Send BEGIN/COMMIT/ROLLBACK via COM_QUERY
- [ ] Track transaction state
- [ ] Handle autocommit

**Phase 6: Output Formatting (4-6 hours)**
- [ ] Implement mysql table output format
- [ ] Implement batch mode output (tab-separated)
- [ ] Implement silent mode (-s flag)
- [ ] Implement vertical format (-E flag)
- [ ] Handle NULL values

---

## Build System Integration

### CMakeLists.txt Structure

```cmake
# src/cli/CMakeLists.txt

# Shared library for common ISQL functionality
add_library(sb_isql_common STATIC
    isql_common/arg_parser.cpp
    isql_common/io_manager.cpp
    isql_common/output_formatter.cpp
    isql_common/connection_base.cpp
)

target_include_directories(sb_isql_common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# sb_fb_isql (Firebird client)
add_executable(sb_fb_isql
    sb_fb_isql/main.cpp
    sb_fb_isql/firebird_connection.cpp
    sb_fb_isql/firebird_protocol.cpp
    sb_fb_isql/firebird_session.cpp
    sb_fb_isql/statement_parser.cpp
    sb_fb_isql/output_formatter.cpp
)

target_link_libraries(sb_fb_isql PRIVATE
    sb_isql_common
    pthread
)

target_include_directories(sb_fb_isql PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

install(TARGETS sb_fb_isql
    RUNTIME DESTINATION bin
)

# sb_pg_isql (PostgreSQL client)
add_executable(sb_pg_isql
    sb_pg_isql/main.cpp
    sb_pg_isql/postgresql_connection.cpp
    sb_pg_isql/postgresql_protocol.cpp
    sb_pg_isql/postgresql_session.cpp
    sb_pg_isql/meta_commands.cpp
    sb_pg_isql/output_formatter.cpp
)

target_link_libraries(sb_pg_isql PRIVATE
    sb_isql_common
    pthread
    readline  # For interactive line editing
)

target_include_directories(sb_pg_isql PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

install(TARGETS sb_pg_isql
    RUNTIME DESTINATION bin
)

# sb_my_isql (MySQL client)
add_executable(sb_my_isql
    sb_my_isql/main.cpp
    sb_my_isql/mysql_connection.cpp
    sb_my_isql/mysql_protocol.cpp
    sb_my_isql/mysql_session.cpp
    sb_my_isql/output_formatter.cpp
)

target_link_libraries(sb_my_isql PRIVATE
    sb_isql_common
    pthread
)

target_include_directories(sb_my_isql PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

install(TARGETS sb_my_isql
    RUNTIME DESTINATION bin
)
```

---

## Testing Approach

### Unit Testing

For each client, create unit tests:

```cpp
// tests/unit/test_firebird_protocol.cpp
TEST(FirebirdProtocol, XdrInt32Encoding) {
    std::vector<uint8_t> buffer;
    encodeXdrInt32(buffer, 0x12345678);

    EXPECT_EQ(buffer.size(), 4);
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    EXPECT_EQ(buffer[2], 0x56);
    EXPECT_EQ(buffer[3], 0x78);

    int32_t decoded = decodeXdrInt32(buffer.data());
    EXPECT_EQ(decoded, 0x12345678);
}

TEST(FirebirdProtocol, XdrStringEncoding) {
    std::vector<uint8_t> buffer;
    encodeXdrString(buffer, "test");

    // Length (4 bytes) + "test" (4 bytes) + padding (0 bytes) = 8 bytes
    EXPECT_EQ(buffer.size(), 8);

    // Length = 4
    EXPECT_EQ(decodeXdrInt32(buffer.data()), 4);

    // String = "test"
    std::string decoded(buffer.begin() + 4, buffer.begin() + 8);
    EXPECT_EQ(decoded, "test");
}
```

### Integration Testing

Test against actual ScratchBird server:

```bash
#!/bin/bash
# tests/integration/test_firebird_client.sh

# Start ScratchBird server (if not running)
./build/src/sb_server &
SERVER_PID=$!
sleep 2

# Test basic connection
./build/src/cli/sb_fb_isql test.fdb -u SYSDBA -p masterkey \
    -c "SELECT 1 FROM RDB\$DATABASE" \
    -o /tmp/fb_test_output.txt

if grep -q "1" /tmp/fb_test_output.txt; then
    echo "✅ Basic connection test passed"
else
    echo "❌ Basic connection test failed"
    exit 1
fi

# Test input/output redirection
cat > /tmp/fb_test_input.sql <<EOF
SELECT 1 AS one FROM RDB\$DATABASE;
SELECT 2 AS two FROM RDB\$DATABASE;
EOF

./build/src/cli/sb_fb_isql test.fdb -u SYSDBA -p masterkey \
    -i /tmp/fb_test_input.sql \
    -o /tmp/fb_test_output.txt

if grep -q "one" /tmp/fb_test_output.txt && grep -q "two" /tmp/fb_test_output.txt; then
    echo "✅ Input/output redirection test passed"
else
    echo "❌ Input/output redirection test failed"
    exit 1
fi

# Cleanup
kill $SERVER_PID
```

### Compatibility Testing

Test with official database test suites:

```bash
# tests/compatibility/firebird/run_tests.sh

FIREBIRD_TESTS="/path/to/fbt-repository/tests"
RESULTS_DIR="test_results/firebird"

mkdir -p $RESULTS_DIR

# Run Firebird compatibility tests
for test in $FIREBIRD_TESTS/functional/basic/*.fbt; do
    # Extract .fbt test to .sql
    python3 scripts/convert_fbt_to_sql.py $test > /tmp/test.sql

    # Run with sb_fb_isql
    ./build/src/cli/sb_fb_isql test.fdb -u SYSDBA -p masterkey \
        -i /tmp/test.sql \
        -o $RESULTS_DIR/$(basename $test .fbt).out

    # Compare with expected output (if available)
    # ...
done
```

---

## Implementation Timeline

### Effort Estimates

| Component | Estimated Hours |
|-----------|----------------|
| **Shared Library (libsb_isql_common)** | 16-24 hours |
| **sb_fb_isql (Firebird client)** | 27-40 hours |
| **sb_pg_isql (PostgreSQL client)** | 27-40 hours |
| **sb_my_isql (MySQL client)** | 27-40 hours |
| **Build System Integration** | 4-6 hours |
| **Unit Tests (all clients)** | 8-12 hours |
| **Integration Tests** | 8-12 hours |
| **Documentation** | 4-6 hours |
| **TOTAL** | **121-180 hours** |

### Parallel Development Strategy

**Phase 1: Foundation (16-24 hours)**
- One developer implements libsb_isql_common
- Defines interfaces for all clients

**Phase 2: Client Development (Parallel, 27-40 hours each)**
- Developer A implements sb_fb_isql
- Developer B implements sb_pg_isql
- Developer C implements sb_my_isql

**Phase 3: Integration (12-18 hours)**
- Integrate all clients into build system
- Create combined test suite
- Document usage

**Total Timeline:**
- **Sequential:** 121-180 hours (15-23 days)
- **Parallel (3 developers):** 55-82 hours (7-10 days)

### Milestones

**Milestone 1:** libsb_isql_common complete
- ✅ Shared arg parsing works
- ✅ Shared I/O works
- ✅ Shared output formatting works

**Milestone 2:** sb_fb_isql connection works
- ✅ Can connect to ScratchBird on port 3050
- ✅ Can execute simple SELECT

**Milestone 3:** sb_pg_isql connection works
- ✅ Can connect to ScratchBird on port 5432
- ✅ Can execute simple SELECT

**Milestone 4:** sb_my_isql connection works
- ✅ Can connect to ScratchBird on port 3306
- ✅ Can execute simple SELECT

**Milestone 5:** All clients feature-complete
- ✅ Input/output redirection works
- ✅ SET commands work
- ✅ Transaction control works
- ✅ Output formatting matches native clients

**Milestone 6:** Test automation working
- ✅ Can run official test suites
- ✅ Tests pass at acceptable rate

---

## Dependencies and Prerequisites

### Server-Side Requirements

**CRITICAL: Verify these exist before starting client development**

1. **Wire Protocol Listeners**
   - [ ] Firebird XDR listener on port 3050
   - [ ] PostgreSQL FE/BE listener on port 5432
   - [ ] MySQL Text listener on port 3306

2. **Parser Routing**
   - [ ] Port 3050 → Firebird parser
   - [ ] Port 5432 → PostgreSQL parser
   - [ ] Port 3306 → MySQL parser

3. **Emulation Schema Support**
   - [ ] Can create schemas for emulation
   - [ ] Firebird system tables (RDB$) mapped to ScratchBird catalog
   - [ ] PostgreSQL system catalogs (pg_catalog) mapped
   - [ ] MySQL system schemas (information_schema) mapped

**If any of these do NOT exist, they MUST be implemented BEFORE client development.**

### Client-Side Dependencies

1. **Libraries**
   - pthread (thread support)
   - readline (for sb_pg_isql interactive mode)
   - Optional: OpenSSL/TLS for encrypted connections

2. **Build Tools**
   - CMake 3.15+
   - C++17 compiler
   - Git (for test repository updates if needed)

3. **Testing Tools**
   - Python 3.x (for test conversion scripts)
   - Bash (for test runner scripts)

---

## Risk Management

### Risks and Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Server wire protocol listeners don't exist | HIGH | BLOCKING | Verify first, implement server-side if needed |
| Wire protocol complexity underestimated | MEDIUM | DELAY | Start with simple cases, iterate |
| Output format doesn't match native tools | MEDIUM | TEST FAILURE | Use native tools for comparison testing |
| Authentication protocols complex | MEDIUM | DELAY | Implement simplest auth first (cleartext) |
| Test conversion scripts fail | LOW | MANUAL WORK | Have manual test conversion fallback |

### Contingency Plans

**If server wire protocols don't exist:**
- BLOCK client development
- Implement server-side listeners first
- Timeline extends by 40-60 hours

**If wire protocol too complex:**
- Start with subset (read-only queries)
- Add features incrementally
- Defer advanced features (prepared statements, binary protocol)

**If output formatting doesn't match:**
- Accept "close enough" for test automation
- Document differences
- Focus on data correctness over format perfection

---

## Success Criteria

### For Each Client

1. **Can connect to ScratchBird server**
2. **Can execute SELECT queries**
3. **Can execute DDL (CREATE TABLE, etc.)**
4. **Can execute DML (INSERT, UPDATE, DELETE)**
5. **Input redirection works (-i/-f flag)**
6. **Output redirection works (-o flag)**
7. **Transaction control works (COMMIT/ROLLBACK)**
8. **Can run at least 50% of official database tests**

### For Overall Plan

1. **All three clients (sb_fb_isql, sb_pg_isql, sb_my_isql) implemented**
2. **Test automation infrastructure in place**
3. **CI/CD integration complete**
4. **Documentation complete**
5. **Compatibility reports generated**

---

## Conclusion

This plan provides complete specifications for implementing three dedicated ISQL clients for ScratchBird's multi-database emulation testing.

**Key Deliverables:**
1. `sb_fb_isql` - Firebird-compatible ISQL client
2. `sb_pg_isql` - PostgreSQL-compatible psql client
3. `sb_my_isql` - MySQL-compatible mysql client
4. `libsb_isql_common` - Shared component library
5. Test automation infrastructure
6. CI/CD integration

**Estimated Effort:** 121-180 hours (7-23 days depending on parallelization)

**Next Steps:**
1. **VERIFY** server-side wire protocol support exists
2. **IMPLEMENT** libsb_isql_common foundation
3. **DEVELOP** three clients in parallel
4. **INTEGRATE** into build system and CI/CD
5. **TEST** with official database test suites

---

**Plan Status:** ✅ COMPLETE - Ready for Implementation
**Created:** 2025-12-28
**Author:** Claude Code
**Implementation:** Assign to AI development teams
