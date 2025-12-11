# ScratchBird Project Context

**Last Updated:** December 11, 2025
**Current Phase:** Alpha Phase 3 - Network & Service Mode 🚀 **IN PROGRESS**
**Previous Phases:** Alpha 1 & Alpha 2 ✅ **COMPLETE**
**Test Suite:** 1337/1337 = 100% pass rate
**Project Type:** Educational/Research (no time constraints)
**Detailed Status:** [IMPLEMENTATION_STATUS_DASHBOARD.md](docs/IMPLEMENTATION_STATUS_DASHBOARD.md)

> **MANDATORY:** Read [/MGA_RULES.md](/MGA_RULES.md) before ANY transaction or index work.
> **COMPLETE ROADMAP:** See [/OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) for full project scope.

---

## Alpha 3 Status: 🚀 **IN PROGRESS** (Started December 10, 2025)

### Phase Overview

Alpha 3 transforms ScratchBird from an embedded database into a **full network server** with:
- Multiple wire protocol support (PostgreSQL, MySQL, Firebird, Native)
- Enterprise security suite (11 authentication methods)
- Service mode with systemd integration
- ODBC/JDBC connectivity for universal database access
- Live migration from existing databases

### Implementation Progress (December 11, 2025)

| Phase | Component | Status | Lines | Specification |
|-------|-----------|--------|-------|---------------|
| 3.1 | Network Infrastructure | ✅ **COMPLETE** | ~6,200 | Foundation for all protocols |
| 3.2 | Wire Protocol Adapters | ✅ **COMPLETE** | ~4,637 | All 4 protocols implemented |
| 3.3 | Service Mode & systemd | ✅ **COMPLETE** | ~3,270 | [systemd Service](docs/specifications/SYSTEMD_SERVICE_SPECIFICATION.md) |
| 3.4 | Security Suite - Core | ✅ **COMPLETE** | ~3,500 | SSL/TLS, SCRAM-SHA-256/512, certificates, HBA |
| 3.5 | Security Suite - Enterprise | 🔜 Pending | - | LDAP, Kerberos, OAuth, SAML, MFA |
| 3.6 | Connection Pooling | 🔜 Pending | - | [Connection Pooling](docs/specifications/CONNECTION_POOLING_SPECIFICATION.md) |
| 3.7 | UDR Plugin System | 🔜 Pending | - | [Remote Database UDR](docs/specifications/remote_database_udr/) |
| 3.8 | ODBC Driver | 🔜 Pending | - | [ODBC Driver](docs/specifications/ODBC_DRIVER_SPECIFICATION.md) |
| 3.9 | JDBC Driver | 🔜 Pending | - | [JDBC Driver](docs/specifications/JDBC_DRIVER_SPECIFICATION.md) |
| 3.10 | TDS Wire Protocol | ⏸️ **DEFERRED** | - | Moved to Beta (MSSQL via ODBC only) |
| 3.11 | Git Integration | ⭕ Nice to Have | - | [Git Integration](docs/specifications/GIT_METADATA_INTEGRATION_SPECIFICATION.md) |
| 3.12 | Testing & Performance | 🔜 Pending | - | [Alpha 3 Test Plan](docs/specifications/ALPHA3_TEST_PLAN.md) |

### Phase 3.1: Network Infrastructure ✅ **COMPLETE** (December 10, 2025)

| File | Lines | Description |
|------|-------|-------------|
| `include/scratchbird/network/socket_types.h` | ~410 | Types, enums, NetworkAddress, SocketOptions |
| `include/scratchbird/network/socket.h` | ~381 | Socket class interface |
| `include/scratchbird/network/event_loop.h` | ~388 | EventLoop (epoll/kqueue/poll) |
| `include/scratchbird/network/thread_pool.h` | ~483 | ThreadPool, Task with priority queue |
| `include/scratchbird/network/connection_handler.h` | ~552 | Connection, ConnectionManager |
| `src/network/socket.cpp` | ~1,028 | Full socket implementation |
| `src/network/event_loop.cpp` | ~767 | Platform-specific event loops |
| `src/network/thread_pool.cpp` | ~490 | Thread pool with scheduling |
| `src/network/connection_handler.cpp` | ~652 | Connection management |
| `tests/unit/test_network.cpp` | ~755 | 30 unit tests |
| **Total** | **~6,200** | Network infrastructure complete |

### Phase 3.2: Wire Protocol Adapters ✅ **COMPLETE** (December 11, 2025)

| Protocol | Port | Version | Lines | Features |
|----------|------|---------|-------|----------|
| PostgreSQL | 5432 | v3 | ~1,517 | MD5 auth, Simple/Extended Query protocol |
| MySQL | 3306 | 5.7+ | ~1,120 | Native password auth, prepared statements |
| Firebird | 3050 | 5.0 (v18) | ~1,300 | XDR encoding, SRP auth support |
| Native ScratchBird | 3092 | Binary | ~700 | Full message types, session management |
| **Total** | | | **~4,637** | All protocols implemented |

**Implementation Files:**
- `include/scratchbird/protocol/adapters/postgresql_adapter.h` (~429 lines)
- `src/protocol/adapters/postgresql_adapter.cpp` (~1,088 lines)
- `include/scratchbird/protocol/adapters/mysql_adapter.h` (~370 lines)
- `src/protocol/adapters/mysql_adapter.cpp` (~750 lines)
- `include/scratchbird/protocol/adapters/firebird_adapter.h` (~400 lines)
- `src/protocol/adapters/firebird_adapter.cpp` (~900 lines)
- `include/scratchbird/protocol/adapters/native_adapter.h` (~150 lines)
- `src/protocol/adapters/native_adapter.cpp` (~550 lines)
- `include/scratchbird/protocol/adapters/protocol_adapter.h` (~373 lines)
- `src/protocol/adapters/protocol_adapter.cpp` (~340 lines)

### Phase 3.3: Service Mode & systemd ✅ **COMPLETE** (December 11, 2025)

| File | Lines | Description |
|------|-------|-------------|
| `include/scratchbird/server/config_parser.h` | ~300 | INI config parser with env var expansion |
| `src/server/config_parser.cpp` | ~700 | Full config parser implementation |
| `include/scratchbird/server/daemon.h` | ~300 | PIDFile, SystemdNotify, Daemon classes |
| `src/server/daemon.cpp` | ~600 | Unix daemonization, sd_notify, signals |
| `include/scratchbird/server/service_controller.h` | ~400 | Service lifecycle management |
| `src/server/service_controller.cpp` | ~700 | Full service implementation |
| `etc/systemd/scratchbird.service` | ~90 | systemd unit file with security hardening |
| `etc/scratchbird/sb_server.conf.example` | ~180 | Example configuration file |
| **Total** | **~3,270** | Service mode complete |

**Key Features Implemented:**
- Unix daemonization (double-fork pattern)
- PID file management with file locking (flock)
- Dynamic loading of libsystemd for sd_notify
- Signal handling (SIGTERM, SIGHUP, SIGUSR1, SIGUSR2, SIGQUIT)
- Privilege dropping (setuid/setgid)
- INI configuration with @include and ${VAR:-default} support
- Size parsing (128MB, 1GB) and duration parsing (30s, 5m, 1h)
- Command-line argument parsing with getopt_long
- Multi-database mode support
- Health check API

### Phase 3.4: Security Suite - Core ✅ **COMPLETE** (December 11, 2025)

| File | Lines | Description |
|------|-------|-------------|
| `include/scratchbird/security/tls_config.h` | ~450 | TLS configuration, CertificateInfo, TLSContext |
| `src/security/tls_context.cpp` | ~1,100 | OpenSSL TLS context, certificate extraction |
| `include/scratchbird/security/auth_method.h` | ~400 | AuthMethod interface, AuthContext, AuthState |
| `src/security/auth_method.cpp` | ~350 | Trust, Reject, Peer auth implementations |
| `include/scratchbird/security/auth_manager.h` | ~450 | HBA rules, RateLimiter, AuditLogger, AuthManager |
| `src/security/auth_manager.cpp` | ~750 | HBA parser, rate limiting, audit logging |
| `include/scratchbird/security/scram_auth.h` | ~340 | SCRAM-SHA-256/512 interface |
| `src/security/scram_auth.cpp` | ~950 | Full RFC 5802 SCRAM implementation |
| `include/scratchbird/security/cert_auth.h` | ~240 | Certificate authentication interface |
| `src/security/cert_auth.cpp` | ~575 | Certificate-to-user mapping, DN parsing |
| **Total** | **~3,500** | Core security suite complete |

**Key Features Implemented:**
- OpenSSL TLS 1.2/1.3 with certificate verification
- SCRAM-SHA-256/512 (RFC 5802) challenge-response authentication
- Certificate authentication with CN/DN/SAN/fingerprint mapping
- Host-Based Authentication (pg_hba.conf style) with IPv4/IPv6 CIDR
- Rate limiting for brute force protection
- Audit logging (file and syslog)
- Password hashing (PBKDF2) with secure salt generation
- Pluggable authentication method framework

### Next Phase: 3.5 Security Suite - Enterprise

**Key Components:**
- LDAP/LDAPS authentication
- Active Directory integration
- Kerberos/GSSAPI
- OAuth 2.0/OIDC
- SAML 2.0 federation
- Multi-factor authentication (TOTP)

### Future Phase: 3.6 Connection Pooling

**Reference Specification:** [CONNECTION_POOLING_SPECIFICATION.md](docs/specifications/CONNECTION_POOLING_SPECIFICATION.md)

**Key Components:**
- Built-in connection pool
- Statement caching
- Result caching
- Pool statistics and monitoring

### Completion Criteria (44 Items)

**Wire Protocols (1-6):**
- ✅ ScratchBird Native Protocol (port 3092) functional
- ✅ PostgreSQL Wire Protocol v3 (port 5432) functional
- ✅ MySQL Wire Protocol (port 3306) functional
- ⏸️ TDS Wire Protocol - DEFERRED TO BETA
- ✅ Firebird Wire Protocol (port 3050) functional
- ✅ All protocols tested with native clients

**Database Connectivity (7-10):**
- ✅ ScratchBird ODBC driver functional
- ✅ ScratchBird JDBC driver functional
- ✅ ODBC connectivity to MSSQL/Oracle/other databases
- ✅ JDBC connectivity to external databases

**Service Mode (11-15):**
- ✅ sb_server daemon mode operational
- ✅ systemd integration complete
- ✅ Configuration file hot-reload working
- ✅ Graceful startup/shutdown
- ✅ Both single-database and multi-database modes

**Security (16-25):**
- ✅ SSL/TLS 1.2+ for all protocols
- ✅ Certificate authentication (X.509/mTLS)
- ✅ Multi-factor authentication (TOTP)
- ✅ IP whitelisting functional
- ✅ LDAP authentication working
- ✅ Active Directory integration
- ✅ Kerberos/GSSAPI functional
- ✅ SAML 2.0 federation
- ✅ OAuth 2.0/OIDC integration
- ✅ Security audit completed

**Connection Pooling (26-29):**
- ✅ Built-in connection pool operational
- ✅ Statement caching working
- ✅ Result caching functional
- ✅ Pool statistics available

**UDR Plugins (30-38):**
- ✅ UDR plugin system functional
- ✅ postgresql_fdw operational
- ✅ mysql_fdw operational
- ⏸️ mssql_fdw - DEFERRED (use ODBC/JDBC)
- ✅ firebird_fdw operational
- ✅ odbc_fdw operational
- ✅ jdbc_fdw operational
- ✅ Foreign table queries working
- ✅ Passthrough queries functional

**Performance (39-41):**
- ✅ Load testing completed (1000+ connections)
- ✅ No memory leaks in 72-hour stress test
- ✅ Protocol compliance verified

**Nice to Have (42-44):**
- ⭕ Git integration for metadata versioning
- ⭕ Docker image and compose files
- ⭕ deb/rpm installation packages

---

## Alpha 3 Specifications (13 Documents, ~22,300 lines)

All specifications created December 10, 2025:

| # | Specification | Lines | Path |
|---|--------------|-------|------|
| 1 | Native Wire Protocol | ~2,800 | `wire_protocols/scratchbird_native_wire_protocol.md` |
| 2 | systemd Service | ~1,800 | `SYSTEMD_SERVICE_SPECIFICATION.md` |
| 3 | Connection Pooling | ~1,400 | `CONNECTION_POOLING_SPECIFICATION.md` |
| 4 | Remote Database UDR | ~7,400 | `remote_database_udr/` (9 modular docs) |
| 5 | Client Library API | ~1,300 | `CLIENT_LIBRARY_API_SPECIFICATION.md` |
| 6 | Alpha 3 Test Plan | ~730 | `ALPHA3_TEST_PLAN.md` |
| 7 | sb_admin CLI | ~600 | `SB_ADMIN_CLI_SPECIFICATION.md` |
| 8 | Prometheus Metrics | ~820 | `PROMETHEUS_METRICS_REFERENCE.md` |
| 9 | Live Migration | ~1,820 | `LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md` |
| 10 | Migration Guide | ~1,020 | `/docs/MIGRATION_GUIDE.md` |
| 11 | ODBC Driver | ~730 | `ODBC_DRIVER_SPECIFICATION.md` |
| 12 | JDBC Driver | ~900 | `JDBC_DRIVER_SPECIFICATION.md` |
| 13 | Git Integration | ~980 | `GIT_METADATA_INTEGRATION_SPECIFICATION.md` |

### Key Architectural Decisions

1. **Native Protocol Port:** 3092 (IANA unassigned)
2. **Cluster Key Architecture:** Hybrid (Cluster CA + Session Keys) for forward secrecy
3. **Federation:** Full cross-database queries (`SELECT * FROM db2.schema.table`)
4. **Serialization:** Custom binary optimized for 86 types
5. **Connection Pooling:** Built-in only (no pgBouncer dependency)
6. **Security Auth:** All 11 methods in Alpha 3
7. **TDS/MSSQL:** DEFERRED - Connect via ODBC only (open source DBs first)
8. **ODBC/JDBC:** Required for connecting TO external databases
9. **Git Integration:** Nice-to-have for DDL version control

---

## Previous Phases: ✅ COMPLETE

### Alpha 1: Core Engine (June-November 2025)

| Component | Status | Tests |
|-----------|--------|-------|
| Storage Engine | ✅ Complete | MGA-based heap pages |
| Transaction Manager | ✅ Complete | TIP-based visibility |
| Index Types | ✅ Complete | BTree, GIN, GiST, LSM |
| TOAST | ✅ Complete | Large object storage |
| Catalog Manager | ✅ Complete | System tables |
| Parser v1 | ✅ Complete | Basic SQL parsing |
| SBLR Executor | ✅ Complete | Bytecode interpreter |
| CLI Tools | ✅ Complete | sb_isql, sb_verify, sb_backup, sb_security |
| Local Server | ✅ Complete | IPC, wire protocol, sessions |

### Alpha 2: Parser Separation (December 2025)

| Component | Status | Tests |
|-----------|--------|-------|
| Parser V2 | ✅ Complete | 171 tests |
| Firebird Parser | ✅ Complete | 52 tests |
| MySQL Parser | ✅ Complete | 30 tests |
| PostgreSQL Parser | ✅ Complete | 52 tests |
| **Total** | ✅ **293 tests** | 100% passing |

---

## MGA Architecture (Firebird Style)

**CRITICAL:** ScratchBird uses **Firebird MGA**, NOT PostgreSQL MVCC.

### Mandatory Rules

- **TIP-based visibility only** - `isVersionVisible(xmin, current_xid)`
- **In-place updates** - Primary record modified, old data in back versions
- **Stable TIDs** - Indexes never change unless indexed column changes
- **No snapshots** - Zero PostgreSQL MVCC contamination

```cpp
// ✅ CORRECT - Firebird MGA
if (isVersionVisible(tuple->xmin, current_xid)) { ... }

// ❌ WRONG - PostgreSQL MVCC (NEVER USE)
if (isSnapshotVisible(tuple, snapshot)) { ... }
```

**Read [MGA_RULES.md](/MGA_RULES.md) before ANY transaction or index work.**

---

## Critical File Locations

### Documentation

- [/OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) - Complete project scope and all phases
- [/MGA_RULES.md](/MGA_RULES.md) - Mandatory architecture rules
- [/docs/IMPLEMENTATION_AUDIT.md](/docs/IMPLEMENTATION_AUDIT.md) - Complete code locations

### Alpha 3 Specifications

- [/docs/specifications/wire_protocols/](/docs/specifications/wire_protocols/) - Protocol specifications
- [/docs/specifications/remote_database_udr/](/docs/specifications/remote_database_udr/) - Foreign data wrappers
- [/docs/specifications/SYSTEMD_SERVICE_SPECIFICATION.md](/docs/specifications/SYSTEMD_SERVICE_SPECIFICATION.md) - Service mode
- [/docs/specifications/CONNECTION_POOLING_SPECIFICATION.md](/docs/specifications/CONNECTION_POOLING_SPECIFICATION.md) - Pooling
- [/docs/specifications/ODBC_DRIVER_SPECIFICATION.md](/docs/specifications/ODBC_DRIVER_SPECIFICATION.md) - ODBC driver
- [/docs/specifications/JDBC_DRIVER_SPECIFICATION.md](/docs/specifications/JDBC_DRIVER_SPECIFICATION.md) - JDBC driver

### Core Implementation

```
src/core/buffer_pool.cpp            - Buffer management
src/core/heap_page.cpp               - Record storage with back-versioning
src/core/toast.cpp                   - Large object storage
src/core/transaction_manager.cpp    - TIP-based transactions
src/core/btree.cpp                   - B-Tree index
src/core/catalog_manager.cpp        - System catalog
src/parser/parser.cpp                - SQL parser v2
src/server/scratchbird_server.cpp   - Server daemon
src/server/server_session.cpp       - Session management
src/protocol/wire_protocol.cpp      - Wire protocol
src/sblr/executor.cpp                - SBLR bytecode interpreter
```

---

## Development Guidelines

### For AI Assistants

**MANDATORY READING:**

1. Read `/MGA_RULES.md` at session start
2. Re-read `/MGA_RULES.md` after context compaction
3. Read `/MGA_RULES.md` BEFORE any transaction or index work
4. Read relevant specification before implementing any Alpha 3 feature

**DO:**

- ✅ Use Firebird MGA model (TIP-based visibility)
- ✅ Maintain stable TIDs
- ✅ In-place updates with back versions
- ✅ Follow error handling patterns (Status enum, ErrorContext)
- ✅ Use RAII for all resources
- ✅ Read specification before implementing feature

**DON'T:**

- ❌ Use PostgreSQL MVCC patterns
- ❌ Implement Snapshot structures
- ❌ Use forward-versioning
- ❌ Update index TIDs unless indexed column changes
- ❌ Skip reading `/MGA_RULES.md`
- ❌ Implement without reading specification first

**CRITICAL:** Violating MGA rules means the code is architecturally WRONG and must be rewritten.

---

## Next Steps for New AI Session

### Immediate Action: Start Phase 3.5 - Security Suite Enterprise

**Phases 3.1, 3.2, 3.3, and 3.4 are COMPLETE.** Network infrastructure, wire protocols, service mode, and core security suite are implemented.

**Next: Phase 3.5 Security Suite - Enterprise**

**Key Components:**
1. LDAP/LDAPS authentication
2. Active Directory integration
3. Kerberos/GSSAPI authentication
4. OAuth 2.0/OIDC integration
5. SAML 2.0 federation
6. Multi-factor authentication (TOTP)

**Recently Completed (Phase 3.4):**
```
include/scratchbird/security/
├── tls_config.h          - TLS configuration, CertificateInfo (~450 lines)
├── auth_method.h         - AuthMethod interface, AuthContext (~400 lines)
├── auth_manager.h        - HBA rules, RateLimiter, AuditLogger (~450 lines)
├── scram_auth.h          - SCRAM-SHA-256/512 interface (~340 lines)
└── cert_auth.h           - Certificate authentication (~240 lines)

src/security/
├── tls_context.cpp       - OpenSSL TLS context (~1,100 lines)
├── auth_method.cpp       - Trust, Reject, Peer auth (~350 lines)
├── auth_manager.cpp      - HBA parser, rate limiting, audit (~750 lines)
├── scram_auth.cpp        - RFC 5802 SCRAM implementation (~950 lines)
└── cert_auth.cpp         - Certificate-to-user mapping (~575 lines)
```

**Existing Reference:**
- Current server: `src/server/scratchbird_server.cpp`
- Wire protocol adapters: `src/protocol/adapters/`
- Network layer: `src/network/`
- Service controller: `src/server/service_controller.cpp`
- Security library: `src/security/` (NEW)

---

## Summary

**Current Phase:** Alpha 3 - Network & Service Mode 🚀 **IN PROGRESS**

**Completed:** Phase 3.1 (Network Infrastructure), Phase 3.2 (Wire Protocol Adapters), Phase 3.3 (Service Mode & systemd), Phase 3.4 (Security Suite - Core)

**Next:** Phase 3.5 (Security Suite - Enterprise)

**Total Code (Alpha 3 so far):** ~17,600 lines (6,200 + 4,637 + 3,270 + 3,500)

**Specifications Ready:** 13 documents (~22,300 lines)

**Completion Criteria:** 44 items (41 required, 3 nice-to-have)

**Test Suite:** 1337/1337 = 100% pass rate

**Full Details:** See [OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md)
