# ScratchBird Security Architecture Specification

**Document ID**: SBSEC-01  
**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  
**Scope**: All deployment modes, all security levels  

---

## 1. Introduction

### 1.1 Purpose

This document defines the security architecture for the ScratchBird database engine. It establishes the trust boundaries, component responsibilities, and security invariants that all other security specifications build upon.

### 1.2 Scope

This specification applies to:

- All deployment modes (Embedded, Shared Local, Full Server, Cluster)
- All security levels (0 through 6)
- All supported wire protocols (Native V2, MySQL, PostgreSQL, FirebirdSQL)

### 1.3 Normative Language

The keywords MUST, MUST NOT, REQUIRED, SHALL, SHALL NOT, SHOULD, SHOULD NOT, RECOMMENDED, MAY, and OPTIONAL in this document are to be interpreted as described in RFC 2119.

### 1.4 Related Documents

| Document ID | Title                         |
| ----------- | ----------------------------- |
| SBSEC-02    | Identity and Authentication   |
| SBSEC-03    | Authorization Model           |
| SBSEC-04    | Encryption and Key Management |
| SBSEC-05    | IPC Security                  |
| SBSEC-06    | Cluster Security              |
| SBSEC-07    | Network Presence Binding      |
| SBSEC-08    | Audit and Compliance          |
| SBSEC-09    | Security Levels               |

---

## 2. Security Design Principles

### 2.1 Core Principles

The ScratchBird security architecture is founded on five core principles:

#### 2.1.1 Engine Authority

The Engine Core is the sole authority for all security decisions. No other component (parser, network listener, client, plugin) may bypass, override, or influence security enforcement.

**Rationale**: Centralizing security decisions in a single component eliminates the possibility of inconsistent enforcement and simplifies security auditing.

#### 2.1.2 UUID-Based Identity

All entities in ScratchBird are identified by UUID v7. This includes:

- Users, roles, and groups
- Sessions and transactions
- Database objects (tables, views, indexes, functions, etc.)
- Databases themselves

UUIDs are never reused. When an object is dropped and recreated, it receives a new UUID.

**Rationale**: UUID-based identity eliminates name-based confusion attacks, provides natural audit correlation, and enables database-specific object isolation.

#### 2.1.3 Complete Session Isolation

Sessions are completely isolated from each other. Two sessions—even if mapped to the same user identity—operate as independent security contexts with:

- Independent permission resolution
- Independent transaction state
- No shared memory or working state
- No visibility into each other's operations

**Rationale**: Complete isolation prevents session confusion attacks and side-channel information leakage between sessions.

#### 2.1.4 Transaction-Bound Security Context

Security context (effective user, roles, groups, permissions) is bound to the transaction and is immutable for the transaction's lifetime. Changes to security context require transaction commit or rollback.

**Rationale**: Immutable security context prevents mid-operation privilege escalation and provides clear audit boundaries.

#### 2.1.5 Defense in Depth

Multiple security layers operate independently:

- Wire protocol authentication
- IPC channel protection
- SBLR bytecode validation
- Engine authorization enforcement
- Audit logging

Compromise of one layer does not automatically compromise others.

### 2.2 Trust Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                    TRUST LEVEL: UNTRUSTED                        │
│  External clients, network traffic, wire protocol data           │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    TRUST LEVEL: CONSTRAINED                      │
│  Network Listener, Parser Pool                                   │
│  • Can process untrusted input                                   │
│  • Cannot make security decisions                                │
│  • Cannot access database directly                               │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    TRUST LEVEL: TRUSTED                          │
│  Engine Core                                                     │
│  • Makes all security decisions                                  │
│  • Validates all inputs from constrained layer                   │
│  • Sole access to database storage                               │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    TRUST LEVEL: PROTECTED                        │
│  Security Catalog, Encryption Keys, Audit Logs                   │
│  • Accessible only through Engine Core                           │
│  • Subject to additional access controls                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Deployment Modes

ScratchBird supports four deployment modes. The security architecture adapts to each while maintaining the core principles.

### 3.1 Deployment Mode Overview

| Mode         | Description                               | Components             | Security Levels |
| ------------ | ----------------------------------------- | ---------------------- | --------------- |
| Embedded     | Library linked directly into application  | Engine only            | 0-3             |
| Shared Local | On-demand server via IPC                  | Engine + IPC           | 0-4             |
| Full Server  | Persistent service with network listeners | All components         | 0-5             |
| Cluster      | Multi-node with quorum                    | All + cluster services | 0-6             |

### 3.2 Embedded Mode

In embedded mode, the application links the ScratchBird engine as a library and accesses it via direct API calls.

```
┌─────────────────────────────────────────┐
│            Host Application              │
│  ┌───────────────────────────────────┐  │
│  │         ScratchBird Engine        │  │
│  │  • Direct API access              │  │
│  │  • No network/IPC                 │  │
│  │  • Single process                 │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

**Security Characteristics**:

- No wire protocol attack surface
- No IPC attack surface
- Application and engine share process memory
- Security enforcement still applies (if enabled)
- Trust boundary is at the API level

**Limitations**:

- Security Level 4+ features requiring external audit storage are limited
- Security Level 5+ features requiring network hardening are not applicable
- Application compromise implies engine compromise

### 3.3 Shared Local Mode

In shared local mode, if no server is running for a requested database, one is started automatically. Clients communicate via IPC (named pipes).

```
┌──────────────┐     ┌──────────────┐
│   Client A   │     │   Client B   │
└──────┬───────┘     └──────┬───────┘
       │                    │
       │    Named Pipes     │
       └────────┬───────────┘
                │
                ▼
┌─────────────────────────────────────────┐
│         ScratchBird Server              │
│  ┌───────────────────────────────────┐  │
│  │         Engine Core               │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

**Security Characteristics**:

- IPC provides process isolation
- Each client has independent session
- No network attack surface
- Named pipe permissions provide basic access control

**Limitations**:

- IPC endpoint protection depends on OS mechanisms
- Local privilege escalation could access named pipes
- Security Level 5+ network hardening not applicable

### 3.4 Full Server Mode

In full server mode, ScratchBird runs as a persistent service with network listeners for each supported wire protocol.

```
┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ 
│  V2 Client  │ │ MySQL Client│ │  PG Client  │ │  FB Client  │
└──────┬──────┘ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘
       │               │               │               │  
       │  Wire Protocols (TLS)         │               │
       ▼               ▼               ▼               ▼
┌──────────────────────────────────────────────────────────────┐
│              Network Listener                                │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌──────────────┐  │
│  │ V2        │ │ MySQL     │ │ PostgreSQL│ │ FireBirdSQL  │  │
│  │ Listener  │ │ Listener  │ │ Listener  │ │ Listener     │  │
│  │           │ │ :3306     │ │ :5432     │ │ :3050        │  │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬────────┘  │
│        └─────────────┼─────────────┼─────────────┘           │
│                      ▼                                       │
│               Parser Pool                                    │
│        (one parser per connection)                           │
└──────────────────────┬───────────────────────────────────────┘
                       │
                       │  ScratchBird IPC
                       ▼
┌─────────────────────────────────────────────────────┐
│                  Engine Core                        │
└─────────────────────────────────────────────────────┘
```

**Security Characteristics**:

- Full network attack surface (wire protocols)
- TLS available for wire encryption
- Parser pool provides dialect isolation
- IPC separates parser from engine
- All security levels (0-5) available

### 3.5 Cluster Mode

In cluster mode, multiple ScratchBird nodes coordinate via a cluster protocol. Security databases provide centralized identity management.

```
┌─────────────────────────────────────────────────────────────────┐
│                        CLUSTER                                  │
│                                                                 │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐          │
│  │   Node A    │    │   Node B    │    │   Node C    │          │
│  │  (Primary)  │◄──►│ (Secondary) │◄──►│ (Security)  │          │
│  └─────────────┘    └─────────────┘    └─────────────┘          │
│         ▲                  ▲                  ▲                 │
│         │     Cluster Protocol (mTLS)        │                  │
│         └──────────────────┴─────────────────┘                  │
│                                                                 │
│  Security Database: Authoritative source for identity           │
│  Quorum: Required for security-critical operations              │
│  Network Presence: MEK protection via Shamir's Secret Sharing   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Security Characteristics**:

- All Full Server characteristics plus:
- Centralized security catalog with replication
- Quorum-based approval for critical operations
- Fencing for split-brain prevention
- Network Presence Binding available (Level 6)
- All security levels (0-6) available

---

## 4. Component Trust Model

### 4.1 Network Listener

#### 4.1.1 Responsibilities

The Network Listener is responsible for:

- Accepting incoming network connections on protocol-specific ports
- Managing TLS negotiation and termination
- Routing connections to appropriate parser pool
- Managing parser lifecycle (spawn, pool, terminate)
- Receiving control commands from Engine (shutdown, restart, reload)

#### 4.1.2 Security Constraints

The Network Listener:

- MUST NOT make authentication decisions
- MUST NOT make authorization decisions
- MUST NOT access database storage directly
- MUST NOT interpret or modify SQL/SBLR content
- MUST NOT retain credentials after passing to parser
- MUST NOT communicate session information between connections

#### 4.1.3 Trust Level

**Constrained**: The Network Listener processes untrusted network input but cannot influence security decisions.

#### 4.1.4 Attack Surface

| Attack Vector         | Mitigation                                   |
| --------------------- | -------------------------------------------- |
| Protocol fuzzing      | Parser handles protocol parsing              |
| Connection exhaustion | Connection limits, rate limiting             |
| TLS attacks           | Modern TLS only (1.2+), secure cipher suites |
| Port scanning         | Standard port security practices             |

#### 4.1.5 Control Channel

The Engine MAY send the following commands to the Network Listener:

- `SHUTDOWN`: Graceful shutdown, drain connections
- `RESTART`: Restart listener process
- `RELOAD`: Reload configuration (TLS certs, pool sizes)
- `DRAIN`: Stop accepting new connections

The Network Listener MUST NOT send commands that affect database state or security configuration.

### 4.2 Parser Layer

#### 4.2.1 Responsibilities

The Parser is responsible for:

- Protocol-specific wire format parsing
- SQL dialect translation to SBLR bytecode
- Passing client credentials to Engine via IPC
- Translating Engine responses to wire protocol format
- Maintaining connection state for a single client session

#### 4.2.2 Security Constraints

The Parser:

- MUST have zero privileges to the database
- MUST NOT make authentication decisions
- MUST NOT make authorization decisions
- MUST NOT cache or log credentials
- MUST NOT access other sessions' data
- MUST terminate when its client connection ends
- MUST be sandboxed to its dialect's namespace

#### 4.2.3 Trust Level

**Constrained**: The Parser translates between protocols but cannot influence security decisions.

#### 4.2.4 Lifecycle

```
┌──────────────────────────────────────────────────────────────────┐
│                    PARSER LIFECYCLE                              │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. POOL (Idle)                                                  │
│     • Parser instance exists in pool                             │
│     • No client connection                                       │
│     • No session state                                           │
│                                                                  │
│  2. ASSIGNED                                                     │
│     • Client connection accepted                                 │
│     • Parser assigned from pool                                  │
│     • Wire protocol handshake begins                             │
│                                                                  │
│  3. AUTHENTICATING                                               │
│     • Credentials extracted from wire protocol                   │
│     • Credentials passed to Engine via IPC                       │
│     • Parser MUST NOT retain credentials after passing           │
│     • Awaiting Engine authentication result                      │
│                                                                  │
│  4. ACTIVE                                                       │
│     • Engine returned session UUID                               │
│     • Parser translates SQL → SBLR                               │
│     • Parser translates results → wire protocol                  │
│     • All requests include session UUID                          │
│                                                                  │
│  5. TERMINATING                                                  │
│     • Client disconnects (normal or abnormal)                    │
│     • Parser notifies Engine of session end                      │
│     • Parser clears all session state                            │
│     • Parser returns to pool or is destroyed                     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 4.2.5 Credential Handling

The Parser MUST adhere to the following credential handling requirements:

1. **Extraction**: Credentials are extracted from the wire protocol according to dialect specification.

2. **Passing**: Credentials are passed to Engine via IPC in a single message. The Parser MUST NOT:
   
   - Store credentials in persistent storage
   - Log credentials (including in debug logs)
   - Include credentials in error messages
   - Retain credentials in memory after IPC transmission

3. **Memory Clearing**: After credentials are passed to Engine:
   
   - Credential buffers MUST be overwritten with zeros
   - Overwrite MUST use volatile pointer or equivalent to prevent compiler optimization
   - Memory fence MUST follow overwrite to ensure completion

4. **Failure Handling**: If authentication fails:
   
   - Parser receives failure indication from Engine
   - Parser translates to dialect-appropriate error
   - Parser terminates or returns to pool
   - No credential information included in error response

#### 4.2.6 Namespace Sandboxing

Each dialect parser operates within a sandboxed namespace:

| Dialect     | Root Namespace | Visible Catalogs                                    |
| ----------- | -------------- | --------------------------------------------------- |
| MySQL       | `mysql_compat` | `information_schema`, `mysql`, `performance_schema` |
| PostgreSQL  | `pg_compat`    | `pg_catalog`, `information_schema`                  |
| FirebirdSQL | `fb_compat`    | `RDB$*` system tables                               |
| V2 (Native) | `sys`          | Full catalog access (subject to permissions)        |

Parsers:

- MUST NOT generate SBLR referencing objects outside their namespace
- MUST NOT allow SQL injection to escape namespace boundaries
- MUST translate dialect-specific system queries to namespace-appropriate views

The namespace views are implemented as views/synonyms that:

- Map to real `sys.*` catalog objects
- Apply session-appropriate filtering
- Hide ScratchBird-specific extensions not present in emulated dialect

### 4.3 Engine Core

#### 4.3.1 Responsibilities

The Engine Core is responsible for:

- All authentication decisions
- All authorization decisions
- Session lifecycle management
- Transaction management
- SBLR bytecode validation and execution
- Catalog management
- Storage operations
- Audit logging
- Key management

#### 4.3.2 Security Authority

The Engine Core is the sole security authority. This means:

1. **Authentication**: Only the Engine validates credentials and creates sessions.

2. **Authorization**: Only the Engine evaluates permissions. Authorization decisions are made:
   
   - Per SBLR program execution
   - Against the session's security context
   - Using current permission state (subject to transaction-bound staleness)

3. **Audit**: Only the Engine generates authoritative audit records.

4. **Key Management**: Only the Engine accesses encryption keys.

#### 4.3.3 Trust Level

**Trusted**: The Engine Core makes all security decisions and has full access to protected resources.

#### 4.3.4 Internal Components

```
┌─────────────────────────────────────────────────────────────────┐
│                       ENGINE CORE                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐    ┌─────────────────┐                     │
│  │  IPC Handler    │    │  Session        │                     │
│  │                 │───►│  Manager        │                     │
│  │  Receives SBLR  │    │                 │                     │
│  │  from parsers   │    │  Creates/       │                     │
│  └─────────────────┘    │  destroys       │                     │
│                         │  sessions       │                     │
│                         └────────┬────────┘                     │
│                                  │                              │
│                                  ▼                              │
│  ┌─────────────────┐    ┌─────────────────┐                     │
│  │  Authenticator  │    │  Security       │                     │
│  │                 │───►│  Context        │                     │
│  │  Validates      │    │                 │                     │
│  │  credentials    │    │  User, roles,   │                     │
│  │                 │    │  groups,        │                     │
│  │  Internal +     │    │  permissions    │                     │
│  │  UDR providers  │    │  (immutable per │                     │
│  └─────────────────┘    │  transaction)   │                     │
│                         └────────┬────────┘                     │
│                                  │                              │
│                                  ▼                              │
│  ┌─────────────────┐    ┌─────────────────┐                     │
│  │  SBLR Validator │    │  Authorizer     │                     │
│  │                 │───►│                 │                     │
│  │  Structure      │    │  Permission     │                     │
│  │  Object UUIDs   │    │  checking       │                     │
│  │  Opcode sanity  │    │  per operation  │                     │
│  └─────────────────┘    └────────┬────────┘                     │
│                                  │                              │
│                                  ▼                              │
│  ┌─────────────────┐    ┌─────────────────┐                     │
│  │  SBLR Executor  │    │  Transaction    │                     │
│  │                 │◄──►│  Manager        │                     │
│  │  Bytecode       │    │                 │                     │
│  │  interpreter    │    │  Firebird MGA   │                     │
│  └────────┬────────┘    │  model          │                     │
│           │             └─────────────────┘                     │
│           ▼                                                     │
│  ┌─────────────────┐    ┌─────────────────┐                     │
│  │  Storage        │    │  Audit          │                     │
│  │  Manager        │    │  Logger         │                     │
│  │                 │    │                 │                     │
│  │  Page I/O       │    │  All security   │                     │
│  │  Encryption     │    │  events         │                     │
│  └─────────────────┘    └─────────────────┘                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. Session Model

### 5.1 Session Identity

Each session is identified by a UUID v7 generated at session creation. The session UUID:

- Is unique across all time and all nodes
- Contains timestamp component for ordering
- Is not derivable from user identity or credentials
- Is valid only for the duration of the session

### 5.2 Session Creation

```
┌─────────────────────────────────────────────────────────────────┐
│                    SESSION CREATION FLOW                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. Parser receives client connection                           │
│  2. Parser extracts credentials from wire protocol              │
│  3. Parser sends authentication request via IPC:                │
│     {                                                           │
│       "type": "AUTH_REQUEST",                                   │
│       "dialect": "mysql" | "postgresql" | "firebird" | "v2",    │
│       "credentials": { ... dialect-specific ... },              │
│       "client_info": {                                          │
│         "address": "...",                                       │
│         "tls_fingerprint": "..." (if applicable)                │
│       }                                                         │
│     }                                                           │
│                                                                 │
│  4. Engine validates credentials:                               │
│     a. Lookup identity by external identifier                   │
│     b. Verify password/token/certificate                        │
│     c. Check account status (active, blocked, expired)          │
│     d. Map external identity → internal User UUID               │
│                                                                 │
│  5. On success, Engine creates session:                         │
│     a. Generate Session UUID (v7)                               │
│     b. Resolve role memberships                                 │
│     c. Resolve group memberships (transitive)                   │
│     d. Create initial transaction                               │
│     e. Build Security Context                                   │
│     f. Log authentication event                                 │
│                                                                 │
│  6. Engine returns to Parser:                                   │
│     {                                                           │
│       "type": "AUTH_SUCCESS",                                   │
│       "session_uuid": "...",                                    │
│       "dialect_namespace": "mysql_compat"                       │
│     }                                                           │
│                                                                 │
│  7. Parser clears credentials from memory                       │
│  8. Parser uses session_uuid for all subsequent requests        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 5.3 Session State

Each session maintains the following state:

| State Element       | Mutability      | Description                              |
| ------------------- | --------------- | ---------------------------------------- |
| Session UUID        | Immutable       | Unique identifier for session            |
| User UUID           | Immutable       | Internal identity (mapped from external) |
| Dialect             | Immutable       | Wire protocol dialect                    |
| Namespace           | Immutable       | Root namespace for this session          |
| Current Transaction | Per-commit      | Active transaction (always exists)       |
| Security Context    | Per-transaction | Permissions, roles, groups               |
| Active Role         | Per-transaction | Currently activated role (if any)        |
| Current Schema      | Mutable         | Working schema for unqualified names     |

### 5.4 Session Isolation

Sessions are completely isolated:

| Isolation Property | Guarantee                                                          |
| ------------------ | ------------------------------------------------------------------ |
| Memory             | No shared memory between sessions                                  |
| Transactions       | Independent transaction contexts                                   |
| Locks              | Sessions may block on locks but cannot see each other's lock state |
| Temp Objects       | Temp tables are session-local                                      |
| Permissions        | Resolved independently per session                                 |
| Errors             | Errors in one session do not affect others                         |

Two sessions mapped to the same User UUID:

- Have the same base permissions (resolved independently)
- Have independent active roles
- Have independent transactions
- Cannot communicate or signal each other
- Are indistinguishable from sessions of different users (operationally)

### 5.5 Session Termination

#### 5.5.1 Normal Termination

```
1. Client sends disconnect (or Parser detects connection close)
2. Parser sends session termination request to Engine
3. Engine:
   a. Rolls back current transaction (if uncommitted work)
   b. Releases all locks held by session
   c. Drops session-local temp objects
   d. Logs session termination event
   e. Clears session from session table
   f. Returns confirmation to Parser
4. Parser clears session state
5. Parser returns to pool (or terminates)
```

#### 5.5.2 Abnormal Termination

If Parser crashes or connection is lost without clean disconnect:

```
1. IPC layer detects broken pipe/connection
2. Engine treats as implicit session termination:
   a. Rollback current transaction
   b. Release all locks
   c. Drop temp objects
   d. Log abnormal termination event (includes reason if known)
   e. Clear session
3. Parser pool manager:
   a. Detects parser failure
   b. Cleans up parser resources
   c. May spawn replacement parser for pool
```

#### 5.5.3 Administrative Termination

Administrators may terminate sessions:

```
1. Admin issues session kill command (via V2 protocol or API)
2. Engine validates admin has SESSION_ADMIN privilege
3. Engine:
   a. Marks session as TERMINATING
   b. Current operation completes or is interrupted (configurable)
   c. Rollback, release locks, cleanup (as normal termination)
   d. Logs administrative termination (includes admin identity, reason)
4. Next Parser request for this session receives SESSION_TERMINATED error
5. Parser closes client connection with appropriate error
```

#### 5.5.4 Security-Triggered Termination

Sessions may be terminated for security reasons:

| Trigger                 | Behavior                                                                       |
| ----------------------- | ------------------------------------------------------------------------------ |
| User blocked            | Active transactions allowed to complete (configurable timeout), then terminate |
| Password changed        | Continue until next auth check, then require re-auth                           |
| Privileges revoked      | Effective at next transaction boundary                                         |
| Session timeout         | Immediate termination after idle period                                        |
| Administrative lockdown | All non-admin sessions terminated                                              |

---

## 6. Transaction Security Model

### 6.1 Firebird Transaction Semantics

ScratchBird uses Firebird's Multi-Generational Architecture (MGA) transaction model:

1. **Always In Transaction**: A session is always within a transaction. Committing one transaction immediately starts another.

2. **Snapshot Isolation**: Each transaction sees a consistent snapshot of data as of its start time.

3. **No Auto-Commit**: There is no auto-commit mode. Applications must explicitly commit or rely on session termination to rollback.

### 6.2 Security Context Binding

The Security Context is computed at transaction start and is immutable for the transaction's lifetime:

```
┌─────────────────────────────────────────────────────────────────┐
│                    SECURITY CONTEXT                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Computed at transaction start from:                             │
│  • User UUID (from session)                                      │
│  • Active role (from session, may be NONE)                       │
│  • Group memberships (resolved transitively)                     │
│  • Current permission grants (snapshot)                          │
│  • Current policy epoch (for RLS/CLS)                            │
│                                                                  │
│  Immutable for transaction lifetime:                             │
│  • Permission changes during transaction are not visible         │
│  • Role changes require commit/rollback                          │
│  • Group membership changes require commit/rollback              │
│                                                                  │
│  Used for:                                                       │
│  • All authorization decisions                                   │
│  • RLS/CLS policy evaluation                                     │
│  • Audit context                                                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6.3 Role Activation

Roles are activated explicitly using `SET ROLE`:

```sql
-- Activate a role (must be in available_roles)
SET ROLE manager;

-- Deactivate role, return to base privileges
SET ROLE NONE;
-- or
RESET ROLE;
```

Role changes:

- Require transaction commit or rollback to take effect
- Are validated against the user's available roles
- Are recorded in audit log
- Update the Security Context for the new transaction

### 6.4 Privilege Staleness

Because Security Context is transaction-bound, there is a window where privilege changes are not yet effective:

```
Timeline:
─────────────────────────────────────────────────────────────────►
T0          T1              T2                  T3
│           │               │                   │
│ Alice     │ Admin         │ Alice             │ Alice
│ starts    │ revokes       │ queries           │ commits
│ txn       │ Alice's       │ sensitive_table   │
│           │ SELECT        │ (SUCCEEDS)        │
│           │               │                   │
│           │               │                   │
│◄─────────────────────────────────────────────►│
│     Staleness window: Alice retains           │
│     SELECT privilege within this transaction  │
                                                │
                                                ▼
                                         T3+1: Alice's
                                         new txn sees
                                         revoked privilege
```

This is correct behavior that:

- Maintains transaction consistency
- Prevents mid-operation failures
- Provides clear audit boundaries

For high-security environments, additional controls are available:

- `max_transaction_duration` policy limits staleness window
- Privilege revocation can trigger advisory notifications
- Audit logs note "privilege exercised within staleness window"

---

## 7. SBLR Security Model

### 7.1 Overview

SBLR (ScratchBird Language Binary Representation) is the bytecode format executed by the Engine. All SQL (from any dialect) is compiled to SBLR before execution. The Engine never parses SQL directly.

### 7.2 SBLR Validation

Before execution, every SBLR program is validated:

#### 7.2.1 Structural Validation

| Check           | Description                    |
| --------------- | ------------------------------ |
| Magic number    | Correct SBLR format identifier |
| Version         | Compatible SBLR version        |
| Checksum        | Bytecode integrity             |
| Opcode validity | All opcodes are recognized     |
| Operand count   | Correct operands per opcode    |
| Jump targets    | All jumps target valid offsets |
| Stack balance   | Stack operations are balanced  |

#### 7.2.2 Object UUID Validation

Every object reference in SBLR is by UUID. Validation ensures:

| Check            | Description                            |
| ---------------- | -------------------------------------- |
| UUID format      | Valid UUID v7 format                   |
| Object existence | UUID exists in this database's catalog |
| Object type      | UUID refers to expected object type    |
| Database scope   | UUID belongs to current database       |

**Critical Property**: Object UUIDs are database-specific. There are no "well-known" UUIDs for system objects. This means:

- SBLR compiled for Database A will fail validation on Database B
- Attackers cannot craft SBLR with hardcoded system object UUIDs
- Each database has unique UUIDs for its `sys.*` catalog objects

#### 7.2.3 Authorization Validation

For each object UUID referenced:

```
1. Lookup object in catalog by UUID
2. Determine required privilege for operation (SELECT, INSERT, UPDATE, DELETE, EXECUTE, etc.)
3. Check session's Security Context for required privilege
4. If ANY object fails authorization, entire SBLR program is rejected
```

Authorization is checked before execution begins, not during execution.

### 7.3 SBLR Generation Security

Parsers generate SBLR from SQL. Security considerations:

| Concern                    | Mitigation                                                         |
| -------------------------- | ------------------------------------------------------------------ |
| Malformed SBLR             | Structural validation catches this                                 |
| Unauthorized object access | Authorization validation catches this                              |
| Namespace escape           | Parser constrained to dialect namespace; UUID validation enforces  |
| SQL injection              | Injection affects SBLR generation, but authorization still applies |

**SQL Injection Analysis**:

Traditional SQL injection allows attacker-controlled SQL to execute. In ScratchBird:

1. Injected SQL is parsed by the dialect parser
2. Parser generates SBLR (possibly referencing attacker-chosen objects)
3. Engine validates SBLR structure and object UUIDs
4. Engine checks authorization against session's Security Context
5. Attacker can only access objects the session is authorized for

SQL injection is still a vulnerability (attacker can access anything the session can access), but it cannot:

- Escalate privileges
- Access objects outside authorization
- Bypass the Engine's security enforcement

### 7.4 SBLR Caching

Compiled SBLR may be cached for performance. Cache security:

| Property      | Requirement                                                        |
| ------------- | ------------------------------------------------------------------ |
| Scope         | Cache entries are scoped to (database, security context hash)      |
| Invalidation  | Cache invalidated on privilege changes, object DDL, policy changes |
| Isolation     | No cross-session cache access                                      |
| Authorization | Re-validated if security context differs from compilation context  |

---

## 8. IPC Security

### 8.1 IPC Channel

Communication between Parser and Engine uses ScratchBird's IPC protocol over named pipes.

### 8.2 Current Implementation (Alpha)

The current alpha implementation uses named pipes with:

- OS-level permissions on pipe endpoints
- No application-level encryption
- Session binding via UUID tokens

### 8.3 Security Requirements by Level

| Security Level | IPC Requirements                             |
| -------------- | -------------------------------------------- |
| 0-2            | Named pipe with restrictive OS permissions   |
| 3-4            | Above + application-level session validation |
| 5-6            | Above + mandatory IPC encryption             |

### 8.4 Endpoint Protection

#### 8.4.1 OS-Level Protection

| Platform | Mechanism          | Configuration                    |
| -------- | ------------------ | -------------------------------- |
| Windows  | Named pipe DACL    | SYSTEM and service account only  |
| Linux    | Unix domain socket | Mode 0600, owned by service user |

#### 8.4.2 Application-Level Protection

For Levels 3+, additional application-level validation:

1. **Parser Registration**: When parser is spawned by Network Listener, it receives a one-time registration token.

2. **Registration Handshake**: Parser presents registration token to Engine on first IPC connection.

3. **Channel Binding**: Engine issues channel token bound to this parser instance.

4. **Request Validation**: All subsequent requests must include channel token.

#### 8.4.3 IPC Encryption (Levels 5-6)

For Levels 5-6, IPC channel is encrypted:

| Property                | Specification                                |
| ----------------------- | -------------------------------------------- |
| Algorithm               | AES-256-GCM                                  |
| Key Exchange            | ECDH with X25519                             |
| Authentication          | Mutual (Engine and Parser both authenticate) |
| Perfect Forward Secrecy | New keys per parser instance                 |

### 8.5 IPC Message Format

```
┌─────────────────────────────────────────────────────────────────┐
│                    IPC MESSAGE STRUCTURE                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐  │
│  │ Length (4B) │ Type (2B)   │ Flags (2B)  │ Session UUID    │  │
│  │             │             │             │ (16B)           │  │
│  ├─────────────┴─────────────┴─────────────┴─────────────────┤  │
│  │ Channel Token (32B) - Levels 3+ only                      │  │
│  ├───────────────────────────────────────────────────────────┤  │
│  │ Payload (variable)                                        │  │
│  │ • AUTH_REQUEST: credentials                               │  │
│  │ • EXECUTE: SBLR bytecode                                  │  │
│  │ • RESULT: result set / error                              │  │
│  │ • CONTROL: transaction control                            │  │
│  ├───────────────────────────────────────────────────────────┤  │
│  │ HMAC (32B) - Levels 5+ only, covers all above fields      │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
│  Encrypted wrapper (Levels 5-6):                                │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ Nonce (12B) │ Encrypted message │ Auth Tag (16B)          │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 8.6 Session UUID Validation

On every IPC request:

1. Extract Session UUID from message
2. Lookup session in Engine's session table
3. If not found: reject with SESSION_INVALID
4. If found but terminated: reject with SESSION_TERMINATED
5. Verify Channel Token (Levels 3+) matches session's bound parser
6. Proceed with request processing

---

## 9. Security Level Summary

This section summarizes how the architecture adapts to each security level. Detailed requirements are in SBSEC-09.

### 9.1 Level Matrix

| Component          | L0  | L1  | L2  | L3  | L4  | L5  | L6  |
| ------------------ |:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **Authentication** |     |     |     |     |     |     |     |
| Optional           | ●   |     |     |     |     |     |     |
| Required           |     | ●   | ●   | ●   | ●   | ●   | ●   |
| UDR providers      |     | ○   | ○   | ○   | ●   | ●   | ●   |
| **Authorization**  |     |     |     |     |     |     |     |
| RBAC/GBAC          | ○   | ●   | ●   | ●   | ●   | ●   | ●   |
| RLS/CLS            |     |     |     | ●   | ●   | ●   | ●   |
| **Encryption**     |     |     |     |     |     |     |     |
| At rest            |     |     | ●   | ●   | ●   | ●   | ●   |
| Wire (TLS)         |     |     |     | ○   | ○   | ●   | ●   |
| IPC                |     |     |     | ○   | ○   | ●   | ●   |
| **Audit**          |     |     |     |     |     |     |     |
| Basic logging      |     |     |     |     | ●   | ●   | ●   |
| Chain hashing      |     |     |     |     | ●   | ●   | ●   |
| External audit     |     |     |     |     | ○   | ●   | ●   |
| **Cluster**        |     |     |     |     |     |     |     |
| Security DB        |     |     |     |     |     |     | ●   |
| Quorum             |     |     |     |     |     |     | ●   |
| Network Presence   |     |     |     |     |     |     | ●   |

● = Required | ○ = Optional | (blank) = Not applicable

### 9.2 Deployment Mode Constraints

| Level | Embedded | Shared Local | Full Server | Cluster |
| ----- |:--------:|:------------:|:-----------:|:-------:|
| 0     | ✓        | ✓            | ✓           | ✓       |
| 1     | ✓        | ✓            | ✓           | ✓       |
| 2     | ✓        | ✓            | ✓           | ✓       |
| 3     | ✓        | ✓            | ✓           | ✓       |
| 4     | Limited  | ✓            | ✓           | ✓       |
| 5     | —        | —            | ✓           | ✓       |
| 6     | —        | —            | —           | ✓       |

✓ = Fully supported | Limited = Some features unavailable | — = Not applicable

---

## 10. Security Invariants

The following invariants MUST hold at all times, regardless of security level or deployment mode.

### 10.1 Engine Authority Invariant

> All authentication, authorization, and security enforcement decisions are made exclusively by the Engine Core.

No parser, listener, plugin, or external component may bypass Engine enforcement.

### 10.2 Session Isolation Invariant

> Sessions are completely isolated. No session may access another session's state, memory, or transaction context.

This applies regardless of whether sessions share the same User UUID.

### 10.3 Transaction Security Context Invariant

> The Security Context is immutable for the lifetime of a transaction.

Changes to privileges, roles, or group memberships take effect only at transaction boundaries.

### 10.4 UUID Non-Reuse Invariant

> UUIDs are never reused. Drop-and-recreate operations always generate new UUIDs.

This prevents UUID confusion attacks and ensures audit trail integrity.

### 10.5 SBLR Validation Invariant

> All SBLR bytecode is validated for structure, object references, and authorization before execution.

No SBLR executes without passing all validation checks.

### 10.6 Credential Non-Retention Invariant

> Credentials are not retained after authentication.

Parsers clear credentials after passing to Engine. Engine stores only hashes/tokens, never plaintext credentials.

### 10.7 Namespace Confinement Invariant

> Dialect parsers are confined to their assigned namespace.

A MySQL parser cannot generate SBLR referencing objects outside `mysql_compat.*`.

### 10.8 Audit Completeness Invariant

> All security-relevant events generate audit records (when auditing is enabled).

No security event—authentication, authorization decision, privilege change, session lifecycle—occurs without audit logging.

---

## Appendix A: Glossary

| Term                 | Definition                                                                   |
| -------------------- | ---------------------------------------------------------------------------- |
| **Channel Token**    | Application-level token binding an IPC channel to a specific parser instance |
| **Dialect**          | A supported SQL language variant (MySQL, PostgreSQL, FirebirdSQL, V2)        |
| **Engine Core**      | The central ScratchBird component responsible for all security enforcement   |
| **MGA**              | Multi-Generational Architecture, Firebird's MVCC implementation              |
| **Namespace**        | A schema hierarchy isolating objects for a particular dialect                |
| **Parser**           | Component that translates wire protocol and SQL dialect to SBLR              |
| **SBLR**             | ScratchBird Language Binary Representation, the bytecode format              |
| **Security Context** | The immutable set of identity and permission information for a transaction   |
| **Session**          | An authenticated connection context, identified by UUID                      |
| **UUID v7**          | Time-ordered UUID format used for all identifiers                            |

---

## Appendix B: Document History

| Version | Date         | Author | Changes       |
| ------- | ------------ | ------ | ------------- |
| 1.0     | January 2026 | —      | Initial draft |

---

## Appendix C: Open Issues

| Issue                                      | Status           | Notes                   |
| ------------------------------------------ | ---------------- | ----------------------- |
| IPC encryption implementation              | To be designed   | Required for Levels 5-6 |
| Parser registration protocol               | To be designed   | Required for Levels 3+  |
| Channel token format                       | To be specified  | Part of IPC security    |
| Administrative session termination timeout | To be determined | Policy configuration    |

---

*End of Document*
