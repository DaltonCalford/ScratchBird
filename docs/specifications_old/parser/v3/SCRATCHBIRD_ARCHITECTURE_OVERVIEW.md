# ScratchBird Architecture Overview

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**A visual guide to the complete ScratchBird server architecture**

**Version:** 1.0  
**Status:** Authoritative (V3)
**Last Updated:** February 2026  

---

## 1. Complete System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                    CLIENT ECOSYSTEM                                         │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                             │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
│   │  sb_isql    │  │  pgAdmin   │  │  MySQL      │  │  FlameRobin  │  │  Custom App │      │
│   │  (native)   │  │  (PG proto)│  │  Workbench  │  │  (FB proto)  │  │  (any proto)│      │
│   └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘      │
│          │                │                │                │                │              │
│          └────────────────┴────────────────┴────────────────┴────────────────┘              │
│                                         │                                                   │
└─────────────────────────────────────────┼───────────────────────────────────────────────────┘
                                          │ TCP / Unix Socket / TLS
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                SCRATCHBIRD SERVER INSTANCE                                  │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────────────────────┐   │
│  │                         NETWORK LAYER (Per-Protocol Listeners)                       │   │
│  │                                                                                      │   │
│  │   ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │   │
│  │   │   Native     │    │  PostgreSQL  │    │    MySQL     │    │   Firebird   │     │   │
│  │   │  Listener    │    │   Listener   │    │   Listener   │    │   Listener   │     │   │
│  │   │  sb_listener │    │  sb_listener │    │  sb_listener │    │  sb_listener │     │   │
│  │   │   _native    │    │     _pg      │    │   _mysql     │    │     _fb      │     │   │
│  │   │              │    │              │    │              │    │              │     │   │
│  │   │  Port: 3092  │    │  Port: 5432  │    │  Port: 3306  │    │  Port: 3050  │     │   │
│  │   │  TLS: Yes    │    │  TLS: Yes    │    │  TLS: Yes    │    │  TLS: Yes    │     │   │
│  │   └──────┬───────┘    └──────┬───────┘    └──────┬───────┘    └──────┬───────┘     │   │
│  │          │                   │                   │                   │              │   │
│  │          └───────────────────┴───────────────────┴───────────────────┘              │   │
│  │                                      │                                               │   │
│  │                          Control Plane (Unix sockets)                                │   │
│  │                                      │                                               │   │
│  └──────────────────────────────────────┼───────────────────────────────────────────────┘   │
│                                         │                                                   │
│  ┌──────────────────────────────────────┼───────────────────────────────────────────────┐   │
│  │                         PARSER LAYER (Per-Connection Processes)                      │   │
│  │                                      │                                               │   │
│  │   ┌──────────────────────────────────┼──────────────────────────────────────────┐    │   │
│  │   │                    Parser Pools (spawned by listeners)                       │    │   │
│  │   │                                                                              │    │   │
│  │   │   ┌────────────┐ ┌────────────┐ │ ┌────────────┐ ┌────────────┐            │    │   │
│  │   │   │  Native    │ │  Native    │ │ │  Native    │ │  Native    │   ...      │    │   │
│  │   │   │  Parser 1  │ │  Parser 2  │ │ │  Parser N  │ │ (idle)     │            │    │   │
│  │   │   │            │ │            │ │ │            │ │            │            │    │   │
│  │   │   │ Waiting    │ │ Waiting    │ │ │ Serving    │ │ Waiting    │            │    │   │
│  │   │   │ for        │ │ for        │ │ │ Client     │ │ for        │            │    │   │
│  │   │   │ handoff    │ │ handoff    │ │ │            │ │ handoff    │            │    │   │
│  │   │   └────────────┘ └────────────┘ │ └────────────┘ └────────────┘            │    │   │
│  │   │                                                                              │    │   │
│  │   │   Each parser:                                                               │    │   │
│  │   │   - Speaks wire protocol                                                     │    │   │
│  │   │   - Translates SQL → SBLR                                                    │    │   │
│  │   │   - Connects to engine ONLY when serving client                              │    │   │
│  │   └──────────────────────────────────────────────────────────────────────────────┘    │   │
│  │                                                                                       │   │
│  └───────────────────────────────────────────────────────────────────────────────────────┘   │
│                                         │                                                    │
│                                         │ IPC (Unix sockets / TCP fallback)                    │
│                                         ▼                                                    │
│  ┌───────────────────────────────────────────────────────────────────────────────────────┐   │
│  │                              ENGINE LAYER (sb_server main)                             │   │
│  │                                                                                        │   │
│  │  ┌─────────────────────────────────────────────────────────────────────────────────┐  │   │
│  │  │                         CORE ENGINE SERVICES                                     │  │   │
│  │  │                                                                                  │  │   │
│  │  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐            │  │   │
│  │  │  │  MGA        │  │  Buffer     │  │  Lock       │  │  Query      │            │  │   │
│  │  │  │  Engine     │  │  Pool       │  │  Manager    │  │  Optimizer  │            │  │   │
│  │  │  │             │  │             │  │             │  │             │            │  │   │
│  │  │  │ Multi-Gen   │  │ Page cache  │  │ Transaction │  │ SQL → Plan  │            │  │   │
│  │  │  │ Arch        │  │ Management  │  │ Coordination│  │             │            │  │   │
│  │  │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘            │  │   │
│  │  └─────────────────────────────────────────────────────────────────────────────────┘  │   │
│  │                                                                                        │   │
│  │  ┌─────────────────────────────────────────────────────────────────────────────────┐  │   │
│  │  │                      BACKGROUND SERVICE THREADS                                  │  │   │
│  │  │                                                                                  │  │   │
│  │  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │  │   │
│  │  │  │  Lock        │  │  MGA GC      │  │  Job         │  │  Stats       │        │  │   │
│  │  │  │  Manager     │  │  Thread      │  │  Scheduler   │  │  Collector   │        │  │   │
│  │  │  │  (per DB)    │  │  (per DB)    │  │              │  │              │        │  │   │
│  │  │  │              │  │              │  │              │  │              │        │  │   │
│  │  │  │ Write-write  │  │ Cleanup old  │  │ Run scheduled│  │ Aggregate    │        │  │   │
│  │  │  │ coordination │  │ versions     │  │ tasks/jobs   │  │ metrics      │        │  │   │
│  │  │  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘        │  │   │
│  │  │                                                                                  │  │   │
│  │  │  ┌──────────────┐  ┌──────────────┐                                              │  │   │
│  │  │  │  Cluster     │  │  Health      │  │  Note: Readers don't need locks (MGA)     │  │   │
│  │  │  │  Manager     │  │  Monitor     │  │        Only writers use lock manager      │  │   │
│  │  │  │  (optional)  │  │              │  │                                             │  │   │
│  │  │  └──────────────┘  └──────────────┘                                              │  │   │
│  │  └─────────────────────────────────────────────────────────────────────────────────┘  │   │
│  │                                                                                        │   │
│  │  ┌─────────────────────────────────────────────────────────────────────────────────┐  │   │
│  │  │                      DATABASE REGISTRY & SECURITY                                │  │   │
│  │  │                                                                                  │  │   │
│  │  │  ┌──────────────────────────┐    ┌──────────────────────────┐                  │  │   │
│  │  │  │   Database Registry      │    │   Security Database      │                  │  │   │
│  │  │  │   (registry.sb)          │    │   (security.db)          │                  │  │   │
│  │  │  │                          │    │                          │                  │  │   │
│  │  │  │ - List of databases      │    │ - Users & passwords      │                  │  │   │
│  │  │  │ - Database paths         │    │ - Roles & permissions    │                  │  │   │
│  │  │  │ - Per-db settings        │    │ - Auth methods           │                  │  │   │
│  │  │  │ - Tablespaces            │    │ - Session tokens         │                  │  │   │
│  │  │  └──────────────────────────┘    └──────────────────────────┘                  │  │   │
│  │  └─────────────────────────────────────────────────────────────────────────────────┘  │   │
│  │                                                                                        │   │
│  └───────────────────────────────────────────────────────────────────────────────────────┘   │
│                                         │                                                    │
└─────────────────────────────────────────┼────────────────────────────────────────────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                    STORAGE LAYER                                            │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────────────────────┐   │
│  │                         DATABASE FILES                                               │   │
│  │                                                                                      │   │
│  │   /var/lib/scratchbird/databases/                                                    │   │
│  │   ├── {db_uuid}/                                                                     │   │
│  │   │   ├── database.db          (Main database file)                                  │   │
│  │   │   ├── database.log         (Write-ahead log)                                     │   │
│  │   │   ├── temporary/           (Temp tablespace)                                     │   │
│  │   │   └── backup/              (Backup files)                                        │   │
│  │   └── ...                                                                          │   │
│  │                                                                                      │   │
│  └─────────────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────────────────────┐   │
│  │                         SYSTEM FILES                                                 │   │
│  │                                                                                      │   │
│  │   /var/lib/scratchbird/                                                              │   │
│  │   ├── registry.sb              (Database catalog)                                    │   │
│  │   ├── security.db              (User authentication)                                 │   │
│  │   ├── run/                     (PID files, sockets)                                  │   │
│  │   └── cache/                   (Query cache, plan cache)                             │   │
│  │                                                                                      │   │
│  └─────────────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────────────────────┐   │
│  │                         RESOURCE FILES                                               │   │
│  │                                                                                      │   │
│  │   /usr/share/scratchbird/resources/                                                  │   │
│  │   ├── languages/           (Collation, charset definitions)                          │   │
│  │   ├── timezones/           (IANA timezone database)                                  │   │
│  │   └── udr/                 (User-defined routine libraries)                          │   │
│  │                                                                                      │   │
│  └─────────────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                             │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Connection Flow

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                               CONNECTION LIFECYCLE                                          │
└─────────────────────────────────────────────────────────────────────────────────────────────┘

┌──────────┐         ┌──────────┐         ┌──────────┐         ┌──────────┐         ┌──────────┐
│  CLIENT  │────────▶│ LISTENER │────────▶│  PARSER  │────────▶│  ENGINE  │────────▶│ DATABASE │
└──────────┘         └──────────┘         └──────────┘         └──────────┘         └──────────┘
     │                    │                    │                    │                    │
     │ 1. TCP Connect     │                    │                    │                    │
     │───────────────────▶│                    │                    │                    │
     │                    │                    │                    │                    │
     │ 2. TLS Handshake   │                    │                    │                    │
     │◀══════════════════▶│                    │                    │                    │
     │                    │                    │                    │                    │
     │ 3. Protocol Hello  │                    │                    │                    │
     │───────────────────▶│                    │                    │                    │
     │                    │ 4. Socket Handoff  │                    │                    │
     │                    │───────────────────▶│                    │                    │
     │                    │                    │                    │                    │
     │                    │                    │ 5. Connect Engine  │                    │
     │                    │                    │───────────────────▶│                    │
     │                    │                    │                    │                    │
     │                    │                    │ 6. Authenticate    │                    │
     │                    │                    │◀══════════════════▶│                    │
     │                    │                    │                    │                    │
     │ 7. Auth Challenge  │                    │                    │                    │
     │◀───────────────────│◀───────────────────│◀───────────────────│                    │
     │                    │                    │                    │                    │
     │ 8. Auth Response   │                    │                    │                    │
     │───────────────────▶│───────────────────▶│───────────────────▶│                    │
     │                    │                    │                    │                    │
     │ 9. Session Ready   │                    │                    │                    │
     │◀───────────────────│◀───────────────────│◀───────────────────│                    │
     │                    │                    │                    │                    │
     │ 10. SQL Query      │                    │                    │                    │
     │───────────────────▶│───────────────────▶│───────────────────▶│                    │
     │                    │                    │ 11. Execute        │                    │
     │                    │                    │◀══════════════════▶│◀══════════════════▶│
     │                    │                    │                    │                    │
     │ 12. Results        │                    │                    │                    │
     │◀───────────────────│◀───────────────────│◀───────────────────│◀───────────────────│
     │                    │                    │                    │                    │
     │ 13. Disconnect     │                    │                    │                    │
     │───────────────────▶│───────────────────▶│───────────────────▶│───────────────────▶│
     │                    │                    │                    │                    │
```

---

## 3. Server Startup Sequence

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                              SERVER STARTUP PHASES                                          │
└─────────────────────────────────────────────────────────────────────────────────────────────┘

    ┌──────────────┐
    │    START     │
    └──────┬───────┘
           │
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│   PHASE 1: PRE-INIT      │     │  • Parse CLI arguments                                  │
│   (Single-threaded)      │     │  • Initialize console logging                           │
│                          │────▶│  • Acquire PID file lock                                │
│   Exit on failure        │     │  • Validate configuration                               │
└──────────┬───────────────┘     │  • Set up signal handlers                               │
           │                      └─────────────────────────────────────────────────────────┘
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│   PHASE 2: CORE INIT     │     │  • Initialize full logging system                       │
│                          │────▶│  • Initialize network subsystem                         │
│   Exit on failure        │     │  • Initialize security (OpenSSL)                        │
└──────────┬───────────────┘     │  • Open database registry                               │
           │                      │  • Open security database                               │
           ▼                      └─────────────────────────────────────────────────────────┘
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│  PHASE 3: STARTUP DBS    │     │  • Open configured startup databases                    │
│                          │────▶│  • Perform crash recovery (if needed)                   │
│   Optional               │     │  • Execute startup SQL scripts                          │
│   Config-driven          │     │  • Fire startup triggers                                │
└──────────┬───────────────┘     └─────────────────────────────────────────────────────────┘
           │
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│  PHASE 4: BACKGROUND     │     │  • Start Lock Manager threads (per DB, Firebird-style)  │
│       SERVICES           │────▶│  • Start MGA GC threads (per database)                  │
│                          │     │  • Start job scheduler thread                           │
│   Continue on error      │     │  • Start statistics collector                           │
└──────────┬───────────────┘     │  • Start cluster membership (if enabled)                │
           │                      │  • Start health monitor                                 │
           │                      │                                                         │
           │                      │  Note: Lock Manager coordinates write-write conflicts   │
           │                      │        Readers don't need locks (MGA provides snapshot) │
           │                      └─────────────────────────────────────────────────────────┘
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│   PHASE 5: LISTENERS     │     │  • Create IPC directory for control sockets             │
│                          │────▶│  • Spawn listener processes (native, pg, mysql, fb)     │
│   Log on failure         │     │  • Wait for listener ready signals                      │
│   Continue others        │     └─────────────────────────────────────────────────────────┘
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│   PHASE 6: PARSERS       │     │  • Accept parser HELLO messages                         │
│                          │────▶│  • Validate protocol matches                            │
│   Log on failure         │     │  • Send HELLO_ACK (parser now idle)                     │
│   Continue with reduced  │     │  • Register parser workers                              │
│   capacity               │     └─────────────────────────────────────────────────────────┘
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│   PHASE 7: READY         │     │  • Log ready state                                      │
│                          │────▶│  • Notify systemd (if applicable)                       │
│                          │     │  • Enter main event loop                                │
│   🟢 SERVER READY        │     │  • Accept client connections                            │
└──────────────────────────┘     └─────────────────────────────────────────────────────────┘
```

---

## 4. Parser State Machine

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                              PARSER LIFECYCLE                                               │
└─────────────────────────────────────────────────────────────────────────────────────────────┘

    ┌──────────────┐
    │    SPAWN     │  (fork()/exec() from listener)
    └──────┬───────┘
           │
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│      INITIALIZING        │────▶│  • Parse arguments from listener                        │
│                          │     │  • Initialize logging (stderr)                          │
│   Exit on failure        │     │  • Initialize network subsystem                         │
└──────────┬───────────────┘     │  • Load TLS context (if enabled)                        │
           │                      └─────────────────────────────────────────────────────────┘
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│    CONNECTING CONTROL    │────▶│  • Connect to listener control socket                   │
│          PLANE           │     │  • Send HELLO message                                   │
│                          │     │  • Wait for HELLO_ACK                                   │
│   Exit on failure        │     └─────────────────────────────────────────────────────────┘
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│         IDLE             │────▶│  • Block on control socket recvmsg()                    │
│                          │     │  • Wait for:                                            │
│   Can receive:           │     │    - HANDOFF_SOCKET (client connection)                 │
│   • HANDOFF_SOCKET       │     │    - HEALTH_CHECK (status request)                      │
│   • HEALTH_CHECK         │     │    - RECYCLE (clean exit)                               │
│   • RECYCLE              │     │    - SHUTDOWN (immediate exit)                          │
│   • SHUTDOWN             │     └─────────────────────────────────────────────────────────┘
└──────────┬───────────────┘
           │ HANDOFF_SOCKET
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│      HANDOFF RECEIVED    │────▶│  • Parse handoff payload                                │
│                          │     │  • Receive client socket fd via SCM_RIGHTS              │
│   Return to IDLE on fail │     │  • Send handoff ACK to listener                         │
└──────────┬───────────────┘     │  • Wrap socket in TLS (if enabled)                      │
           │                      └─────────────────────────────────────────────────────────┘
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│    CONNECTING ENGINE     │────▶│  • Connect to engine via IPC (Unix socket)              │
│                          │     │  • Fallback to localhost TCP if IPC fails               │
│   Return to IDLE on fail │     │  • Authenticate as internal connection                  │
└──────────┬───────────────┘     └─────────────────────────────────────────────────────────┘
           │
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│      SERVING CLIENT      │────▶│  • Perform protocol handshake                           │
│                          │     │  • Authenticate client                                  │
│   Until:                 │     │  • Process SQL queries                                  │
│   • Client disconnects   │     │  • Forward SBLR to engine                               │
│   • Error                │     │  • Return results to client                             │
│   • Max requests reached │     └─────────────────────────────────────────────────────────┘
└──────────┬───────────────┘
           │ Session ends
           ▼
┌──────────────────────────┐     ┌─────────────────────────────────────────────────────────┐
│    CLEANUP & DECIDE      │────▶│  • Disconnect from engine                               │
│                          │     │  • Close client socket                                  │
│   Decision:              │     │  • Check max_requests limit                             │
│   • Return to IDLE       │     │  • Check max_age limit                                  │
│   • Exit cleanly         │     │  • Return to IDLE or exit                               │
└──────────────────────────┘     └─────────────────────────────────────────────────────────┘
```

---

## 5. Configuration Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                            CONFIGURATION SOURCES                                            │
└─────────────────────────────────────────────────────────────────────────────────────────────┘

   Highest Priority  ┌─────────────────────────────────────────────────────────────────────┐
                     │  1. Command-Line Arguments                                          │
                     │     --bind 127.0.0.1 --port 3092 --config /path/to/custom.conf      │
                     └─────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
                     ┌─────────────────────────────────────────────────────────────────────┐
                     │  2. Environment Variables                                           │
                     │     SCRATCHBIRD_BIND=0.0.0.0                                        │
                     │     SCRATCHBIRD_NATIVE_PORT=13092                                   │
                     └─────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
                     ┌─────────────────────────────────────────────────────────────────────┐
                     │  3. Configuration File                                              │
                     │     /etc/scratchbird/sb_server.conf                                 │
                     │                                                                     │
                     │     [server]                                                        │
                     │     bind_address = 127.0.0.1                                        │
                     │                                                                     │
                     │     [network]                                                       │
                     │     native_port = 3092                                              │
                     │     postgresql_port = 5432                                          │
                     │                                                                     │
                     │     [ssl]                                                           │
                     │     enabled = true                                                  │
                     │     cert_file = /etc/scratchbird/ssl/server.crt                     │
                     └─────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
   Lowest Priority   ┌─────────────────────────────────────────────────────────────────────┐
                     │  4. Built-in Defaults                                               │
                     │     bind_address = "0.0.0.0"                                        │
                     │     native_port = 3092                                              │
                     │     buffer_pool_size = "128MB"                                      │
                     └─────────────────────────────────────────────────────────────────────┘
```

---

## 6. Directory Layout

```
/etc/scratchbird/                          # Configuration
├── sb_server.conf                         # Main server config
├── sb_hba.conf                            # Host-based auth rules
├── listeners.conf                         # Listener overrides
└── ssl/                                   # TLS certificates
    ├── server.crt
    ├── server.key
    └── ca.crt

/var/lib/scratchbird/                      # Runtime data
├── registry.sb                            # Database catalog (SQLite)
├── security.db                            # User accounts (ScratchBird format)
├── databases/                             # Database files
│   ├── {uuid-1}/                          # Database 1
│   │   ├── database.db
│   │   ├── database.log
│   │   └── temporary/
│   └── {uuid-2}/                          # Database 2
│       └── ...
├── run/                                   # PID files, sockets
│   ├── scratchbird.pid
│   └── listeners/
│       ├── native.sock
│       └── pg.sock
└── cache/                                 # Query/plan cache

/var/log/scratchbird/                      # Log files
├── server.log
├── listener_native.log
├── listener_pg.log
└── audit/                                 # Audit logs
    └── 2026/
        └── 02/
            └── 07/
                └── audit.log

/usr/share/scratchbird/                    # Read-only resources
└── resources/
    ├── languages/                         # Collation data
    ├── timezones/                         # IANA timezone DB
    └── udr/                               # UDR libraries

/usr/bin/                                  # Executables
├── sb_server                              # Main server
├── sb_isql                                # Interactive SQL
├── sb_admin                               # Administration
├── sb_backup                              # Backup tool
├── sb_security                            # Security management
├── sb_setup                               # Config wizard
├── sb_listener_native                     # Native listener
├── sb_listener_pg                         # PostgreSQL listener
├── sb_listener_mysql                      # MySQL listener
├── sb_listener_fb                         # Firebird listener
└── sb_parser_*                            # Parser agents
```

---

## 7. Related Specifications

| Document | Purpose |
|----------|---------|
| [SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md](SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md) | Complete server flow |
| [SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md](SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md) | Startup/shutdown phases |
| [DATABASE_REGISTRY_SPECIFICATION.md](DATABASE_REGISTRY_SPECIFICATION.md) | Database registry |
| [INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md](INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md) | Installation flow |
| [Network Listener and Parser Pool](network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md) | Listener details |
| [Control Plane Protocol](network/CONTROL_PLANE_PROTOCOL_SPEC.md) | Control messages |
| [Engine Parser IPC Contract](network/ENGINE_PARSER_IPC_CONTRACT.md) | Parser-engine IPC |
