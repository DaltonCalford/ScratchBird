# ScratchBird Architecture Clarifications

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](transaction/FIREBIRD_CONSTANTS_REFERENCE.md)



**Critical Design Decisions That Differ from Other Databases**

**Version:** 1.0  
**Status:** Authoritative Reference  
**Last Updated:** February 2026  

---

## 1. The Separated Parser Architecture

### 1.1 Why This Confuses AI Assistants

Most database servers (PostgreSQL, MySQL, SQL Server) have parsers **inside** the main server process:

```
PostgreSQL/MySQL Architecture (Monolithic):
┌─────────────────────────────────────────┐
│            Server Process               │
│  ┌─────────────────────────────────┐   │
│  │  Parser (inside server)         │   │
│  │  SQL → Query Plan               │   │
│  └─────────────────────────────────┘   │
│              │                          │
│              ▼                          │
│  ┌─────────────────────────────────┐   │
│  │  Executor                       │   │
│  │  Run query plan                 │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

**ScratchBird's Architecture (Separated):**
```
┌─────────────────────┐     ┌─────────────────────┐
│   Listener Process  │────▶│   Parser Process    │
│   (accepts socket)  │     │   (per connection)  │
└─────────────────────┘     └──────────┬──────────┘
                                       │
                                       │ SQL → SBLR
                                       ▼
                              ┌─────────────────────┐
                              │   Engine Process    │
                              │   (sb_server main)  │
                              │   SBLR → Execution  │
                              └─────────────────────┘
```

### 1.2 Key Differences

| Aspect | PostgreSQL/MySQL | ScratchBird |
|--------|------------------|-------------|
| **Parser Location** | Inside server process | Separate child process |
| **Memory Isolation** | Shared address space | Process isolation |
| **Crash Impact** | Can crash server | Parser crash = one connection |
| **SQL Injection** | Server vulnerable | Parser breach doesn't reach engine |
| **Protocol Support** | Built into server | Each protocol = separate parser |
| **Resource Limits** | Hard to enforce | OS-level per-parser limits |

### 1.3 Parser Lifecycle (Correct Understanding)

```
┌────────────────────────────────────────────────────────────────────────┐
│                     SCRATCHBIRD PARSER LIFECYCLE                        │
└────────────────────────────────────────────────────────────────────────┘

PHASE 1: STARTUP (Before any client connects)
─────────────────────────────────────────────────────────────────────────
  Server starts
       │
       ├──▶ Spawns Listener processes
       │         │
       │         └──▶ Each listener forks Parser pool
       │                   │
       │                   ├──▶ Parser initializes
       │                   │         - Load protocol code
       │                   │         - Load TLS context
       │                   │         - Open control socket to listener
       │                   │         - Send HELLO
       │                   │         - Wait for HELLO_ACK
       │                   │
       │                   └──▶ Parser enters IDLE state
       │                             - NOT connected to engine
       │                             - Waiting for socket handoff
       │
       └──▶ Server starts background threads
                 - Lock Manager
                 - Garbage Collector  
                 - Job Scheduler

PHASE 2: CLIENT CONNECTION (Socket handoff)
─────────────────────────────────────────────────────────────────────────
  Client connects to Listener
       │
       ├──▶ Listener accepts socket
       │
       ├──▶ Listener selects idle Parser from pool
       │
       ├──▶ Listener sends HANDOFF_SOCKET message
       │         - Includes: connection_id, client_addr, tls_active flag
       │         - Includes: received socket fd via SCM_RIGHTS
       │
       ├──▶ Parser receives socket
       │         - Now owns client connection
       │         - Performs TLS handshake (if needed)
       │
       ├──▶ Parser connects to Engine via IPC
       │         - Unix socket (preferred)
       │         - TCP localhost (fallback)
       │
       └──▶ Parser begins protocol handshake
                 - Native: SBWP handshake
                 - PostgreSQL: PG wire protocol handshake  
                 - MySQL: MySQL handshake
                 - Firebird: XDR handshake

PHASE 3: AUTHENTICATION
─────────────────────────────────────────────────────────────────────────
  Parser extracts credentials from protocol handshake
       │
       ├──▶ Parser sends AuthRequest to Engine
       │         - Protocol: SBWP over IPC
       │
       ├──▶ Engine validates against security database
       │
       └──▶ Engine returns AuthResult
                 - Success: session established
                 - Failure: error returned to client

PHASE 4: QUERY EXECUTION
─────────────────────────────────────────────────────────────────────────
  Client sends SQL query
       │
       ├──▶ Parser receives protocol-specific message
       │         - Native: SBWP Query message
       │         - PostgreSQL: PG Extended Query protocol
       │         - MySQL: COM_QUERY
       │
       ├──▶ Parser parses SQL to AST
       │
       ├──▶ Parser generates SBLR (ScratchBird Bytecode)
       │
       ├──▶ Parser sends SBLR to Engine via IPC
       │
       ├──▶ Engine executes SBLR
       │         - Uses MGA for transaction isolation
       │         - Lock Manager for write coordination
       │         - Buffer Pool for data access
       │
       ├──▶ Engine returns results to Parser
       │
       └──▶ Parser formats results per protocol
                 - Native: SBWP result format
                 - PostgreSQL: PG row format
                 - MySQL: MySQL result set

PHASE 5: DISCONNECTION
─────────────────────────────────────────────────────────────────────────
  Client disconnects OR error occurs
       │
       ├──▶ Parser closes engine connection
       │
       ├──▶ Parser returns to IDLE state
       │         - Does NOT exit
       │         - Re-registers with listener
       │         - Waits for next handoff
       │
       └──▶ OR: Parser exits if:
                 - max_requests reached
                 - max_age exceeded
                 - recycle requested by listener
```

### 1.4 What AI Often Gets Wrong

**❌ WRONG:** Parser connects to engine on startup  
**✅ CORRECT:** Parser connects to engine ONLY when handling a client

**❌ WRONG:** Parser parses SQL then sends to listener  
**✅ CORRECT:** Parser receives socket from listener, then talks directly to engine

**❌ WRONG:** One parser handles multiple connections  
**✅ CORRECT:** One parser = one connection, then returns to idle or exits

**❌ WRONG:** Emulated protocols (PG/MySQL) use different flow  
**✅ CORRECT:** ALL protocols use same flow: Listener → Parser → Engine via IPC

---

## 2. Emulated Protocol Flow

### 2.1 Common Misconception

AI assistants often assume:
- PostgreSQL emulation = PostgreSQL code paths
- MySQL emulation = MySQL code paths

**This is WRONG.**

### 2.2 Correct Architecture

```
ALL protocols (native AND emulated) use the same architecture:

┌────────────────────────────────────────────────────────────────────────┐
│                     EMULATED PROTOCOL FLOW                              │
└────────────────────────────────────────────────────────────────────────┘

Client (pgAdmin/mysql)
       │
       │ PostgreSQL Wire Protocol
       ▼
┌─────────────────┐
│ sb_listener_pg  │  ◄── PostgreSQL-specific listener
│ Port 5432       │      - Accepts PG protocol connections
└────────┬────────┘      - Hands off to PG parser pool
         │
         │ Socket Handoff
         ▼
┌─────────────────┐
│ sb_parser_pg    │  ◄── PostgreSQL-specific parser
│                 │      - Speaks PG wire protocol to client
│                 │      - Parses PG dialect SQL
│                 │      - Generates SBLR
│                 │      - Talks to engine via SBWP/IPC
└────────┬────────┘
         │
         │ SBWP over IPC (Unix socket or TCP)
         ▼
┌─────────────────┐
│ sb_server       │  ◄── Engine (same for ALL protocols)
│ (main process)  │      - Receives SBLR from any parser
│                 │      - Executes SBLR
│                 │      - Uses MGA for MVCC
│                 │      - Returns results
└─────────────────┘

Key Point:
- The ENGINE is the same regardless of client protocol
- PostgreSQL parser is just translating PG SQL → SBLR
- MySQL parser is just translating MySQL SQL → SBLR
- The engine executes SBLR, not SQL
```

### 2.3 Protocol Translation Example

```sql
-- Client sends (PostgreSQL protocol):
SELECT id, name FROM users WHERE active = true;

-- sb_parser_pg:
-- 1. Receives PG Extended Query protocol message
-- 2. Parses SQL
-- 3. Generates SBLR bytecode:

SBLR:
  LOAD_TABLE users
  FILTER active = true
  PROJECT id, name
  RETURN

-- 4. Sends SBLR to engine via IPC
-- 5. Receives results from engine
-- 6. Formats as PostgreSQL result set
-- 7. Sends to client
```

---

## 3. Lock Manager (Firebird-Style)

### 3.1 AI Confusion Point

AI assistants often try to implement PostgreSQL-style locking:
- Row-level locks for readers
- Multiple lock modes (ACCESS SHARE, ROW EXCLUSIVE, etc.)
- Complex lock acquisition ordering

**This is WRONG for MGA.**

### 3.2 Correct Lock Manager Design (Firebird)

```
┌────────────────────────────────────────────────────────────────────────┐
│                     MGA LOCK PHILOSOPHY                                 │
└────────────────────────────────────────────────────────────────────────┘

Core Principle:
┌────────────────────────────────────────────────────────────────────┐
│  READERS NEVER NEED LOCKS                                            │
│                                                                      │
│  MGA provides snapshot isolation - readers see consistent view       │
│  without locking. Old versions remain until no longer needed.        │
└────────────────────────────────────────────────────────────────────┘

Lock Manager Purpose:
┌────────────────────────────────────────────────────────────────────┐
│  COORDINATE WRITE-WRITE CONFLICTS ONLY                               │
│                                                                      │
│  - Multiple writers to same row need coordination                    │
│  - DDL operations need exclusive access                              │
│  - Deadlock detection for waiting writers                            │
└────────────────────────────────────────────────────────────────────┘
```

### 3.3 Lock Types (Firebird-Style)

| Lock Type | Purpose | Duration |
|-----------|---------|----------|
| **Relation Lock** | DDL operations on table | Statement/Transaction |
| **Record Lock** | Write to specific row | Transaction |
| **Transaction Lock** | Wait for another TX to end | Until TX ends |
| **Index Lock** | Index modifications | Statement |
| **Database Lock** | Exclusive database access | Connection |

**NO READ LOCKS** - Readers use MGA snapshot visibility, not locks.

### 3.4 Lock Manager Startup

The lock manager is started as a **background thread** (NOT a separate process):

```cpp
// In Server::startBackgroundServices()

// 4.1: Lock Manager (Firebird-style, per-database)
// Lock manager coordinates write-write conflicts in MGA
// Readers don't need locks (MGA provides snapshot isolation)
for (const auto& [db_id, database] : startup_databases_) {
    auto lock_mgr = std::make_unique<LockManager>(database.get());
    lock_mgr->setDeadlockCheckInterval(config.lock_deadlock_check_interval_ms);
    lock_mgr->setLockTimeout(config.lock_timeout_ms);
    lock_mgr->start();  // <-- Background thread
    lock_managers_[db_id] = std::move(lock_mgr);
    LOG_INFO("Lock manager started for database: {}", db_id);
}
```

### 3.5 Lock Manager Thread

```
Lock Manager Thread (one per open database):
┌────────────────────────────────────────────────────────────────────┐
│                                                                    │
│  ┌────────────────────────────────────────────────────────────┐   │
│  │  Responsibilities:                                           │   │
│  │                                                              │   │
│  │  1. Maintain lock hash table                                 │   │
│  │     - Which transactions hold which locks                    │   │
│  │     - Wait queue for blocked transactions                    │   │
│  │                                                              │   │
│  │  2. Deadlock detection                                       │   │
│  │     - Periodically check for wait cycles                     │   │
│  │     - Victim selection and abort                             │   │
│  │                                                              │   │
│  │  3. Lock acquisition/release                                 │   │
│  │     - Grant compatible locks immediately                     │   │
│  │     - Queue incompatible lock requests                       │   │
│  │                                                              │   │
│  │  4. Post-block cleanup                                       │   │
│  │     - Notify waiting transactions on unlock                  │   │
│  │                                                              │   │
│  └────────────────────────────────────────────────────────────┘   │
│                                                                    │
│  NOT responsible for:                                              │
│  - Read operations (no locks needed)                               │
│  - Version visibility (MGA handles this)                           │
│  - Transaction state (TIP handles this)                            │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

### 3.6 When Locks Are Acquired

```
Transaction Flow with Lock Manager:

T1: BEGIN TRANSACTION
       │
       ├──▶ No locks acquired
       │
T1: SELECT * FROM accounts  ◄── READ - No lock needed!
       │
       ├──▶ MGA visibility check
       │         - Check transaction state in TIP
       │         - Read appropriate version
       │
T1: UPDATE accounts SET balance = 100 WHERE id = 1
       │
       ├──▶ WRITE - Lock required!
       │         │
       │         ├──▶ Request lock on record (id=1)
       │         │         - Compatible? Grant immediately
       │         │         - Incompatible? Queue, wait
       │         │
       ├──▶ Create new version with rhd_transaction = T1
       │
T1: COMMIT
       │
       ├──▶ Release all locks
       │
       └──▶ Mark transaction as committed in TIP
```

---

## 4. Database Startup Timing

### 4.1 Startup Databases vs On-Demand

```
┌────────────────────────────────────────────────────────────────────────┐
│                     DATABASE OPEN MODES                                 │
└────────────────────────────────────────────────────────────────────────┘

MODE 1: Startup Database (Pre-Opened)
────────────────────────────────────────
Configuration:
  [server]
  startup_database.sales = {"required": true, "recovery": true}

Server Startup:
  1. Open registry
  2. Open sales.db  ◄── DATABASE OPENED HERE
  3. Run recovery (if needed)
  4. Execute startup scripts
  5. Fire startup triggers
  6. Start lock manager thread for sales.db
  7. Start GC thread for sales.db
  8. Start listeners

Client Connection:
  - Fast attach (database already open)
  - Lock manager ready
  - GC already running

MODE 2: On-Demand (First Connection)
────────────────────────────────────────
Configuration:
  [server]
  startup_database.sales = null  (or not specified)

Server Startup:
  1. Open registry
  2. Start listeners  ◄── NO DATABASE OPENED YET
  
Client Connection:
  1. Client: "ATTACH sales"
  2. Server: Open sales.db  ◄── DATABASE OPENED NOW
  3. Server: Initialize lock manager
  4. Server: Initialize GC thread
  5. Server: Complete attach
  
  - Slower first connection
  - Resources allocated on demand
  - Same functionality once open

MODE 3: CREATE DATABASE (Runtime)
────────────────────────────────────────
Client Connection:
  1. Client: "CREATE DATABASE newdb"
  2. Server: Create database files
  3. Server: Register in registry
  4. Server: Open database
  5. Server: Initialize lock manager
  6. Server: Initialize GC thread
  7. Server: Return success
  
  - Database created on-demand
  - Immediately available for connections
```

### 4.2 Resource Allocation Timing

| Resource | Startup DB | On-Demand DB |
|----------|------------|--------------|
| **File Handles** | Server start | First attach |
| **Buffer Pool Pages** | Server start | First attach |
| **Lock Manager Thread** | Server start | First attach |
| **GC Thread** | Server start | First attach |
| **Connection Time** | Fast | Slower (initialization) |

---

## 5. Background Threads Summary

### 5.1 Threads Started During Phase 4

```cpp
void Server::startBackgroundServices(const StartupConfig& config) {
    // For EACH open database:
    for (const auto& [db_id, database] : startup_databases_) {
        
        // 1. Lock Manager Thread (per database)
        //    - Firebird-style write-write coordination
        //    - Deadlock detection
        auto lock_mgr = std::make_unique<LockManager>(database.get());
        lock_mgr->start();
        
        // 2. MGA Garbage Collector Thread (per database)
        //    - Remove old record versions
        //    - Triggered by OST-OIT gap or schedule
        auto gc = std::make_unique<MgaGarbageCollector>(database.get());
        gc->start();
    }
    
    // Global threads (one per server):
    
    // 3. Job Scheduler Thread
    //    - Run scheduled tasks
    //    - Cron-like functionality
    job_scheduler_->start();
    
    // 4. Statistics Collector Thread
    //    - Aggregate query stats
    //    - Update monitoring tables
    stats_collector_->start();
    
    // 5. Cluster Manager Thread (optional)
    //    - Distributed coordination
    //    - Only if cluster_enabled
    cluster_manager_->start();
    
    // 6. Health Monitor Thread
    //    - Watchdog for other threads
    //    - Report metrics
    health_monitor_->start();
}
```

### 5.2 Thread Relationship Diagram

```
┌────────────────────────────────────────────────────────────────────────┐
│                     SERVER THREAD MODEL                                 │
└────────────────────────────────────────────────────────────────────────┘

Main Thread (sb_server)
├──▶ Startup Phase (single-threaded)
│
├──▶ Background Threads (Phase 4)
│    │
│    ├──▶ Lock Manager Thread ──────┐
│    │    (per database)            │
│    │                               │
│    ├──▶ GC Thread ────────────────┤──▶ Each database gets:
│    │    (per database)            │    Lock Manager + GC
│    │                               │
│    ├──▶ Job Scheduler Thread      │
│    │                               │
│    ├──▶ Statistics Thread         │
│    │                               │
│    ├──▶ Cluster Thread (optional) │
│    │                               │
│    └──▶ Health Monitor Thread     │
│
├──▶ Listener Processes (Phase 5)
│    │
│    ├──▶ fork() sb_listener_native
│    ├──▶ fork() sb_listener_pg
│    ├──▶ fork() sb_listener_mysql
│    └──▶ fork() sb_listener_fb
│
└──▶ Event Loop (accept signals, IPC messages)

Listener Processes (forked, NOT threads)
├──▶ Parser Pool Management
│    │
│    ├──▶ fork() sb_parser_native
│    ├──▶ fork() sb_parser_native
│    └──▶ fork() sb_parser_native ... (pool size)
│
└──▶ Accept Loop (wait for connections)

Parser Processes (forked from listener)
├──▶ Idle Loop (wait for handoff)
│
└──▶ Active Session (when handling client)
     ├──▶ Parse SQL
     ├──▶ Generate SBLR
     ├──▶ IPC to Engine
     └──▶ Return results

IMPORTANT DISTINCTIONS:
- Threads = Background services INSIDE sb_server
- Processes = Listeners and Parsers (forked, separate)
- Parser NEVER runs as thread (always separate process)
```

---

## 6. Parser Isolation and Connection Termination

### 6.1 The "Kill Connection" Problem in Other Databases

Traditional databases (PostgreSQL, MySQL) face challenges when terminating connections:

```
PostgreSQL/MySQL (Thread-based):
┌─────────────────────────────────────────┐
│            Server Process               │
│  ┌─────────────────────────────────┐   │
│  │  Connection Handler Thread      │   │
│  │  - Shares memory with server    │   │
│  │  - Cleanup is complex           │   │
│  │  - May leave locks dangling     │   │
│  │  - Memory leaks possible        │   │
│  │  - SIGTERM requires cooperation │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘

Problems:
- Cannot just kill thread (corrupts shared state)
- Must request graceful shutdown
- Bad connection may ignore request
- Cleanup code is complex and error-prone
- Locks may not be released properly
```

### 6.2 ScratchBird's Process-Based Solution

```
ScratchBird (Process-based):
┌─────────────────────────────────────────┐
│            Server Process               │
│  ┌─────────────────────────────────┐   │
│  │  Connection Handler = PROCESS   │   │  ◄── SEPARATE PID
│  │  - Own memory space             │   │      Full isolation
│  │  - Kill = instant cleanup       │   │      OS reclaims all
│  │  - No shared state corruption   │   │
│  │  - Locks released automatically │   │
│  │  - Memory freed by OS           │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘

Benefits:
- kill(pid, SIGKILL) = immediate termination
- OS cleans up all resources
- No memory leaks possible
- No shared state corruption
- Parser bugs/problems are contained
- Easy to implement connection limits
```

### 6.3 Connection Termination Flow

```
┌────────────────────────────────────────────────────────────────────────┐
│                     KILLING A CONNECTION                                │
└────────────────────────────────────────────────────────────────────────┘

Administrator detects problem (infinite loop, memory leak, etc.):

Option 1: KILL CONNECTION
─────────────────────────
  KILL CONNECTION 'conn-12345';
       │
       ▼
  ┌────────────────────────────────────────────────────────────────┐
  │  1. Server looks up connection table                           │
  │     - Finds: connection_id = 'conn-12345'                      │
  │     - Finds: parser_pid = 12345                                │
  │                                                                  │
  │  2. Server sends signal to parser process                      │
  │     kill(12345, SIGTERM)  -- Graceful first                    │
  │                                                                  │
  │  3. Wait up to 5 seconds for parser to exit                    │
  │                                                                  │
  │  4. If still running:                                          │
  │     kill(12345, SIGKILL)  -- Force kill                        │
  │                                                                  │
  │  5. OS terminates process, reclaims ALL memory                 │
  │                                                                  │
  │  6. Server cleanup (even if parser crashed):                   │
  │     - Release any engine locks held by connection              │
  │     - Rollback any active transaction                          │
  │     - Update connection statistics                             │
  │     - Free connection slot                                     │
  │                                                                  │
  │  Result: Clean termination, guaranteed cleanup                 │
  └────────────────────────────────────────────────────────────────┘

Option 2: Parser Self-Termination
─────────────────────────────────
  Parser detects problem (memory limit, query timeout):
       │
       ▼
  ┌────────────────────────────────────────────────────────────────┐
  │  1. Parser catches signal (SIGALRM, SIGXCPU, etc.)             │
  │                                                                  │
  │  2. Parser sends ErrorResponse to client                       │
  │     "Query cancelled due to timeout"                           │
  │                                                                  │
  │  3. Parser closes engine connection                            │
  │                                                                  │
  │  4. Parser exits cleanly (exit code 0)                         │
  │                                                                  │
  │  5. Listener detects process death (SIGCHLD)                   │
  │                                                                  │
  │  6. Listener spawns replacement parser                         │
  │                                                                  │
  │  Result: Clean exit, fresh parser for next connection          │
  └────────────────────────────────────────────────────────────────┘
```

### 6.4 Memory Isolation Benefits

```
┌────────────────────────────────────────────────────────────────────────┐
│                     MEMORY ISOLATION                                    │
└────────────────────────────────────────────────────────────────────────┘

Without Isolation (Thread-based - PostgreSQL/MySQL):
┌────────────────────────────────────────────────────────────────────────┐
│  Server Process (Shared Memory)                                        │
│  ┌──────────┬──────────┬──────────┬──────────────────────────────┐    │
│  │ Thread 1 │ Thread 2 │ Thread 3 │  Global State                │    │
│  │ Memory   │ Memory   │ Memory   │  - Buffer pool               │    │
│  │          │          │          │  - Lock table                │    │
│  │  [BUG]   │          │          │  - Connection table          │    │
│  │  corrupts│◄─────────┼──────────┼───other thread's data        │    │
│  │  shared  │          │          │                              │    │
│  │  state!  │          │          │                              │    │
│  └──────────┴──────────┴──────────┴──────────────────────────────┘    │
│                                                                         │
│  Problem: Memory bug in one thread can corrupt entire server           │
│                                                                         │
└────────────────────────────────────────────────────────────────────────┘

With Isolation (Process-based - ScratchBird):
┌────────────────────────────────────────────────────────────────────────┐
│  Parser Process 1    Parser Process 2    Parser Process 3              │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐             │
│  │  Own Memory  │    │  Own Memory  │    │  Own Memory  │             │
│  │  Space       │    │  Space       │    │  Space       │             │
│  │              │    │              │    │              │             │
│  │   [BUG]      │    │              │    │              │             │
│  │   corrupts   │    │              │    │              │             │
│  │   own state  │    │              │    │              │             │
│  │       │      │    │              │    │              │             │
│  │       ▼      │    │              │    │              │             │
│  │  [CRASH]     │    │  [UNAFFECTED]│    │  [UNAFFECTED]│             │
│  │  Process dies│    │  Continues   │    │  Continues   │             │
│  └──────────────┘    └──────────────┘    └──────────────┘             │
│                                                                         │
│  Engine Server (separate process):                                      │
│  ┌────────────────────────────────────────────────────────────────┐    │
│  │  Shared State (IPC only):                                       │    │
│  │  - Buffer Pool (protected)                                      │    │
│  │  - Lock Manager (protected)                                     │    │
│  │  - No direct memory access from parsers                         │    │
│  └────────────────────────────────────────────────────────────────┘    │
│                                                                         │
│  Benefit: Parser crash affects ONLY that connection                    │
│                                                                         │
└────────────────────────────────────────────────────────────────────────┘
```

### 6.5 What AI Often Gets Wrong

**❌ WRONG:** Kill connection requires complex state cleanup  
**✅ CORRECT:** kill(parser_pid) = instant clean termination, OS handles cleanup

**❌ WRONG:** Parser memory leaks can accumulate  
**✅ CORRECT:** Parser exits = OS reclaims ALL memory, no leaks possible

**❌ WRONG:** Parser bugs can corrupt server  
**✅ CORRECT:** Parser is separate process, cannot corrupt engine memory

**❌ WRONG:** Connection termination must be cooperative  
**✅ CORRECT:** SIGKILL works instantly, no cooperation needed

**❌ WRONG:** Connection lost = work lost  
**✅ CORRECT:** Connection context persists; new parser can resume work

**❌ WRONG:** Parser crash aborts transaction  
**✅ CORRECT:** Transaction stays active; triage on reconnect lets client decide

---

## 7. Connection Recovery: The Server Maintains State

### 7.1 The Key Insight

Most databases tie connection state to the client handler thread/process:
- Thread/process dies = connection state lost
- Work in progress is aborted
- Transaction is rolled back

**ScratchBird separates connection state from parser:**
- Connection context stored in server's ConnectionManager
- Parser is just a translator (expendable)
- Parser dies = assign new parser to existing context
- Transaction continues, session variables preserved

### 7.2 Connection State Ownership

```
┌────────────────────────────────────────────────────────────────────────┐
│                    STATE OWNERSHIP                                      │
└────────────────────────────────────────────────────────────────────────┘

OWNED BY SERVER (Persistent):
┌────────────────────────────────────────────────────────────────────────┐
│  ConnectionManager in sb_server                                        │
│  ┌────────────────────────────────────────────────────────────────┐   │
│  │  Connection Context                                             │   │
│  │  - connection_id (UUID)                                         │   │
│  │  - session_token (secret for reconnection)                      │   │
│  │  - user_id, database_id                                         │   │
│  │  - transaction_id, tx_state                                     │   │
│  │  - session_variables                                            │   │
│  │  - prepared_statements                                          │   │
│  │  - cursor_positions                                             │   │
│  │  - temporary_table metadata                                     │   │
│  └────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Survives: Parser death, network disconnect, client crash              │
│  Timeout: 5 minutes (configurable) before cleanup                      │
└────────────────────────────────────────────────────────────────────────┘

OWNED BY PARSER (Transient):
┌────────────────────────────────────────────────────────────────────────┐
│  Parser Process (per connection)                                       │
│  ┌────────────────────────────────────────────────────────────────┐   │
│  │  Protocol State                                                 │   │
│  │  - Socket fd                                                    │   │
│  │  - Protocol parser state machine                                │   │
│  │  - Current query parse tree                                     │   │
│  │  - Protocol buffers                                             │   │
│  └────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Dies with: Parser process termination                                 │
│  Recreated: New parser assigned on reconnect                           │
└────────────────────────────────────────────────────────────────────────┘
```

### 7.3 Recovery Flow

```
Network Disconnect or Parser Crash:
       │
       ▼
┌─────────────────┐
│ Client loses    │
│ connection      │
└────────┬────────┘
         │
         ▼
┌────────────────────────────────────────────────────────────────┐
│ Server ConnectionManager                                       │
│  1. Detect disconnect (socket close)                           │
│  2. Mark connection as DISCONNECTED                            │
│  3. KEEP context alive (don't cleanup)                         │
│  4. KEEP transaction active                                    │
│  5. Start 5-minute reconnect timer                             │
└────────────────────────────────────────────────────────────────┘
         │
         │ Client reconnects within timeout
         ▼
┌────────────────────────────────────────────────────────────────┐
│ Reconnection Protocol                                          │
│  1. Client sends: connection_id + session_token                │
│  2. Server validates context exists                            │
│  3. New parser assigned from pool                              │
│  4. Context attached to new parser                             │
│  5. Transaction state verified                                 │
│  6. "Connection resumed" sent to client                        │
└────────────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────┐
│ Client continues│
│ where they left │
│ off             │
└─────────────────┘
```

### 7.4 Triage for Ambiguous States

When parser crashes mid-query:

```
Parser crashes during UPDATE users SET status='active' WHERE id=5
       │
       ▼
Did the UPDATE complete?
    ┌─────────┴─────────┐
    │                   │
  YES                  NO
    │                   │
    ▼                   ▼
┌─────────┐      ┌─────────────┐
│Transaction│     │ Transaction │
│committed │      │ still active│
└─────────┘      │ (no changes)│
                 └─────────────┘

Client reconnects:
- Server: "Transaction still active"
- Server: "Last query status: UNKNOWN (server error)"
- Client decides: COMMIT, ROLLBACK, or VERIFY
```

---

## 8. Common AI Misconceptions - Quick Reference

| Misconception | Reality |
|---------------|---------|
| **Parser connects to engine on startup** | Parser connects to engine ONLY when handling a client |
| **Parser sends SQL to listener** | Parser receives socket FROM listener, then talks to engine |
| **One parser handles multiple connections** | One parser = one connection, then idle or exit |
| **Emulated protocols use different code paths** | ALL protocols use same IPC flow to engine |
| **Parser threads share memory with server** | Parser is SEPARATE PROCESS with full isolation |
| **Killing connection requires complex cleanup** | Kill parser process = instant, clean termination |
| **Connection lost = work lost** | Connection context persists; new parser can resume |
| **Parser crash = transaction abort** | Transaction stays active; triage on reconnect |
| **Readers need locks** | Readers NEVER need locks in MGA |
| **Lock manager is separate process** | Lock manager is background thread in server |
| **Database opens on first SQL** | Database can open at startup OR on first attach |
| **GC runs in parser** | GC runs as server background thread |
| **Listeners are threads** | Listeners are separate processes |
| **sb_server handles network connections** | sb_server NEVER handles network directly |

---

## 9. Related Specifications

- [SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md](SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md) - Complete startup flow
- [SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md](SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md) - Architecture overview
- [TRANSACTION_LOCK_MANAGER.md](transaction/TRANSACTION_LOCK_MANAGER.md) - Lock manager details
- [FIREBIRD_TRANSACTION_MODEL_SPEC.md](sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md) - Firebird MGA
- [/docs/specifications/parser/v3/MGA_RULES.md](/docs/specifications/parser/v3/MGA_RULES.md) - MGA architecture rules

**Terminology note:** ScratchBird uses Firebird MGA. Any MGA references in this file are legacy shorthand and must be interpreted as MGA per the authoritative references above.
