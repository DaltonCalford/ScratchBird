## 

<title></title>

<style type="text/css">
 @page { size: 21.59cm 27.94cm; margin: 2cm }
 h3 { color: #444444; margin-top: 0.35cm; margin-bottom: 0.18cm; background: transparent; page-break-after: avoid }
 h3.western { font-weight: bold; font-size: 11pt; font-family: "Arial", sans-serif }
 h3.cjk { font-weight: bold; font-size: 11pt; font-family: "Arial" }
 h3.ctl { font-weight: bold; font-family: "Arial"; font-size: 11pt }
 h2 { color: #333333; margin-top: 0.53cm; margin-bottom: 0.26cm; background: transparent; page-break-after: avoid }
 h2.western { font-weight: bold; font-size: 13pt; font-family: "Arial", sans-serif }
 h2.cjk { font-weight: bold; font-size: 13pt; font-family: "Arial" }
 h2.ctl { font-weight: bold; font-family: "Arial"; font-size: 13pt }
 h1 { color: #1a1a1a; margin-top: 0.71cm; margin-bottom: 0.35cm; background: transparent; page-break-after: avoid }
 h1.western { font-weight: bold; font-size: 16pt; font-family: "Arial", sans-serif }
 h1.cjk { font-weight: bold; font-size: 16pt; font-family: "Arial" }
 h1.ctl { font-weight: bold; font-family: "Arial"; font-size: 16pt }
 p { margin-bottom: 0.25cm; line-height: 115%; background: transparent }
 a:link { color: #0563c1; text-decoration: underline }
 </style>

**DATABASE
ENGINE**

**Security Architecture**

Technical
Design Specification

Version
1.0

December
2024

**CONFIDENTIAL**

# **Table

of Contents**

1.
Executive Summary

2.
Security Architecture Overview

3.
Security Modes

4.
Cluster Security

5.
Authentication Framework

6.
Credential Caching

10. Encryption and Key Management

11. Wire Protocol Security

12. Audit and Compliance

13. Policy Reference

# **1. Executive Summary**

This document defines the comprehensive security architecture for a
distributed database engine supporting multiple deployment modes and
wire protocol compatibility with PostgreSQL, MySQL, Microsoft SQL
Server, and Firebird. The architecture provides enterprise-grade
security while maintaining flexibility for embedded, standalone, and
clustered deployments.

## **1.1 Design Principles**

- Defense in Depth: Multiple layers of security controls

- Policy-Driven:  Configurable security policies rather than hardcoded behaviors

- Zero Trust: Verify all connections, encrypt all traffic, audit all
   actions

- Compliance  Ready: Built-in support for SOC 2, HIPAA, PCI-DSS, and GDPR
   requirements

- Protocol  Agnostic: Unified security model across all supported wire protocols

## **1.2 Key Features**

- Three-tier  security model: Embedded (no security), Standalone, and Cluster
   modes

- Quorum-based  cluster security with automatic fencing for split-brain protection

- Pluggable  authentication via trusted User Defined Routines (UDRs)

- Hierarchical  key management with HSM and external KMS support

- Tamper-evident  audit logging with chain hashing and digital signatures

- Real-time  SIEM integration and anomaly detection

# **2. Security Architecture Overview**

The security architecture consists of interconnected subsystems that
provide comprehensive protection for data at rest, in transit, and
during processing.

## **2.1 Architecture Layers**

|                |                                                                                                             |
| -------------- | ----------------------------------------------------------------------------------------------------------- |
| **Layer**      | **Components**                                                                                              |
| Protocol       | Wire<br> protocol handlers (PostgreSQL, MySQL, MSSQL, Firebird), TLS<br> termination, channel binding       |
| Authentication | Auth<br> dispatcher, provider registry, UDR framework, external auth<br> integration (Kerberos, LDAP, OIDC) |
| Authorization  | Permission<br> cache, role hierarchy, grant/revoke management, row-level security                           |
| Encryption     | Key<br> hierarchy (CMK→DBK→TSK→DEK), TDE, write-after log (WAL) encryption, backup<br> encryption                             |
| Cluster        | Security<br> database replication, quorum management, fencing, membership<br> control                       |
| Audit          | Event<br> collection, chain hashing, SIEM streaming, compliance reporting,<br> anomaly detection            |

## **2.2 Trust Hierarchy**

All security authority flows from the Cluster Master Key (CMK) held by
security databases:

1. CMK: Root of trust, derived from passphrase/HSM/KMS, held only by
   security databases

2. Security Databases: Authoritative sources for authentication, authorization,
   and key distribution

3. Data Databases: Receive authorization from security databases, cache
   credentials locally

4. Clients: Authenticate via supported protocols, receive session tokens

# **3. Security Modes**

The database engine supports three distinct security modes to accommodate
different deployment scenarios.

## **3.1 Embedded Mode (auth.mode = NONE)**

In embedded mode, the database operates without any internal security.
The application embedding the database is the sole source of truth
for authentication and authorization.

### **Characteristics**

- No  authentication required for connections

- No  internal user or role management

- Application  responsible for all access control

- Suitable  for single-user desktop applications or trusted environments

### **Use Cases**

- Desktop  applications with integrated database

- Development  and testing environments

- IoT  devices with application-layer security

## **3.2 Standalone Mode (auth.mode = LOCAL)**

In standalone mode, the database maintains its own security information
within the database itself. A SYSTEM_ADMINISTRATOR password is
provided at creation time.

### **Characteristics**

- Self-contained
   security catalog

- Users,
   roles, and permissions stored within the database

- SYSTEM_ADMINISTRATOR
   is the bootstrap user

- No
   external dependencies for security

### **Bootstrap Process**

1. Database creation receives SYSTEM_ADMINISTRATOR password

2. Password is hashed using configured algorithm (default: Argon2id)

3. Security catalog tables are created and populated

4. SYSTEM_ADMINISTRATOR can then create additional users and roles

## **3.3 Cluster Mode (auth.mode = CLUSTER)**

In cluster mode, security is centralized in one or more security
databases that serve as the authoritative source for all
authentication and authorization in the cluster.

### **Characteristics**

- Centralized  security management

- Multiple  security databases for high availability

- Quorum-based  decision making for security changes

- Credential  caching on data databases for availability

- Automatic  fencing for split-brain protection

# **4. Cluster Security**

Cluster security provides centralized identity management, quorum-based
consensus for security changes, and automatic fencing to prevent
split-brain scenarios.

## **4.1 Security Database Architecture**

Security databases are specialized databases that hold the authoritative
security catalog for the cluster. They replicate security information
to each other and provide authentication/authorization services to
data databases.

### **Security Database Responsibilities**

- Store and replicate user credentials, roles, and permissions

- Hold the Cluster Master Key (CMK) for key distribution

- Participate  in quorum decisions for security changes

- Push  credential invalidations to data databases

- Authenticate  database membership requests

## **4.2 Quorum Management**

Quorum ensures that security-critical operations only succeed when a
majority of security databases agree. This prevents conflicting
changes during network partitions.

### **Quorum Models**

|               |                     |                                                       |
| ------------- | ------------------- | ----------------------------------------------------- |
| **Model**     | **Formula**         | **Use Case**                                          |
| MAJORITY      | floor(N/2)<br> + 1  | Standard<br> operations, default for most deployments |
| SUPERMAJORITY | floor(2N/3)<br> + 1 | Membership<br> changes, policy modifications          |
| UNANIMOUS     | N<br> (all members) | Removing<br> compromised security databases           |

### **Operations Requiring Quorum**

|                              |                                  |
| ---------------------------- | -------------------------------- |
| **Operation**                | **Required Quorum**              |
| User<br> authentication      | Any<br> one security database    |
| User/role<br> creation       | Majority                         |
| GRANT/REVOKE                 | Majority                         |
| Policy<br> changes           | Supermajority                    |
| Add<br> security database    | Supermajority                    |
| Remove<br> security database | Unanimous<br> (excluding target) |

## **4.3 Fencing**

Fencing prevents split-brain scenarios by restricting operations when quorum
is lost. A minority partition knows it's a minority and limits its
own capabilities.

### **Fencing States**

|           |                                                                                      |
| --------- | ------------------------------------------------------------------------------------ |
| **State** | **Description**                                                                      |
| HEALTHY   | Quorum<br> achieved, full read/write operations permitted                            |
| FENCED    | Quorum<br> lost but some peers reachable; limited operations, no security<br> writes |
| ISOLATED  | No<br> peers reachable; more restrictive than FENCED                                 |
| SUSPENDED | Extended<br> isolation; admin intervention required                                  |

### **Operations in FENCED Mode**

**Always Denied:** CREATE/ALTER/DROP
USER, GRANT/REVOKE, cluster membership changes, policy changes

**Always Allowed:** Read from security catalogs (cached), heartbeat/discovery, status queries

**Policy-Controlled:** New connections
(cluster.fenced_allow_new_connections), new transactions
(cluster.fenced_allow_new_transactions)

## **4.4 User Identity Management**

Users are identified by UUID rather than name, allowing names to be changed
without affecting permissions. All rights follow the UUID.

### **Name Conflict Resolution**

When network partitions heal and both sides created users with the same
name:

1. Both UUIDs are preserved (they are distinct users)

2. Both accounts are marked as BLOCKED_NAME_CONFLICT

3. Cluster admin reviews and resolves (rename, merge, or delete)

4. Active sessions for blocked users can complete current transaction but
   cannot start new ones

# **5. Authentication Framework**

The authentication framework provides a unified interface for multiple
authentication methods through a pluggable UDR (User Defined Routine)
architecture.

## **5.1 Authentication Provider Types**

|                   |                                                                                             |
| ----------------- | ------------------------------------------------------------------------------------------- |
| **Provider Type** | **Description**                                                                             |
| BUILTIN           | Built-in<br> providers: LOCAL_PASSWORD, TRUST (embedded mode only)                          |
| UDR               | Shared<br> library implementing authentication interface: Kerberos, LDAP,<br> OIDC/SSO, PAM |
| EXTERNAL_SERVICE  | Remote<br> authentication service via API                                                   |

## **5.2 UDR Trust Model**

Authentication UDRs are security-critical and require explicit trust registration:

- Library  path and checksum must be registered by CLUSTER_ADMIN

- Optional  cryptographic signature verification for distribution builds

- Background  daemon verifies checksums periodically

- Checksum  mismatch immediately disables provider and alerts administrators

## **5.3 Sandbox Levels**

UDRs run in configurable sandboxes to limit damage from bugs or
compromises:

|                |           |                       |                  |
| -------------- | --------- | --------------------- | ---------------- |
| **Capability** | **NONE**  | **RESTRICTED**        | **STRICT**       |
| Network        | Unlimited | Engine-proxied        | None             |
| Filesystem     | Unlimited | Config<br> paths only | None             |
| Memory         | Unlimited | Configurable          | 64MB<br> max     |
| CPU<br> Time   | Unlimited | Configurable          | 1<br> second max |

## **5.4 Authentication Flow**

- Parse  authentication request from wire protocol

- Lookup  user in credential cache or security database

- Check  user status (ACTIVE, BLOCKED, SUSPENDED)

- Select  appropriate authentication provider based on user configuration

- Invoke  provider (possibly multi-step for SASL/Kerberos)

- On  success: create session, sync external groups to roles

- Audit  log authentication attempt and result

## **5.5 External Group Mapping**

Groups from external providers (LDAP, Kerberos, OIDC) can be automatically
mapped to database roles:

- Mapping  rules defined per provider with pattern matching support

- auto_grant:  Add role to user on login if group present

- auto_revoke:  Remove role if group no longer present (optional)

# **6. Credential Caching**

Data databases cache credentials from security databases to maintain
availability during network issues while ensuring security through
TTL-based expiration and push invalidation.

## **6.1 Cache Components**

|                      |                                                                             |
| -------------------- | --------------------------------------------------------------------------- |
| **Cache**            | **Contents**                                                                |
| Credential<br> Cache | User<br> UUID, username, password hash, user status, expiration             |
| Permission<br> Cache | User<br> permissions by object, including grants, denies, and grant options |
| Role<br> Cache       | User<br> role memberships with inheritance tracking                         |
| Session<br> Cache    | Active<br> sessions with authentication details and transaction state       |
| Policy<br> Cache     | Cluster<br> policies with version tracking                                  |

## **6.2 TTL Strategy**

|                |                 |                             |
| -------------- | --------------- | --------------------------- |
| **Cache Type** | **Default TTL** | **Degraded TTL**            |
| Credentials    | 5<br> minutes   | 10<br> minutes (max 1 hour) |
| Permissions    | 1<br> minute    | 2<br> minutes (max 1 hour)  |
| Roles          | 2<br> minutes   | 4<br> minutes (max 1 hour)  |
| Policies       | 10<br> minutes  | 20<br> minutes (max 1 hour) |

## **6.3 Invalidation**

Security databases push invalidation notices to data databases when
credentials or permissions change:

- Push  invalidation: Security DB broadcasts change to all registered data
   DBs

- Acknowledgment  required; unacknowledged messages queued for retry

- Pull  fallback: Data DB can poll for missed invalidations on reconnect

- Version  vectors: Each cache entry tracks source version for staleness
   detection

## **6.4 Session Handling During Invalidation**

When a user is blocked or their credentials are invalidated:

- Active  transactions are allowed to complete (commit or rollback)

- No  new transactions are permitted

- Session  transitions to BLOCKED_PENDING until transaction completes

- Configurable timeout for forced termination of long-running transactions

# **7. Encryption and Key Management**

The encryption subsystem provides data protection at rest and in transit
through a hierarchical key management system.

## **7.1 Key Hierarchy**

|              |                      |                      |                               |
| ------------ | -------------------- | -------------------- | ----------------------------- |
| **Key Type** | **Scope**            | **Protected By**     | **Purpose**                   |
| CMK          | Cluster              | Passphrase/HSM/KMS   | Root<br> of trust             |
| SDK          | Security<br> DB      | CMK                  | Encrypt<br> security catalogs |
| CSK          | Credential<br> store | SDK                  | Encrypt<br> stored passwords  |
| DBK          | Database             | CMK                  | Wrap<br> tablespace keys      |
| TSK          | Tablespace           | DBK                  | Wrap<br> data encryption keys |
| DEK          | Page/extent          | TSK                  | Encrypt<br> actual data       |
| LEK          | Transaction<br> log  | DBK                  | Encrypt<br> write-after log (WAL)/journal       |
| BKK          | Backup               | CMK<br> + passphrase | Encrypt<br> backups           |

## **7.2 Key Derivation Sources**

|                 |                                                                        |
| --------------- | ---------------------------------------------------------------------- |
| **Source**      | **Description**                                                        |
| PASSPHRASE      | PBKDF2/Argon2id<br> from admin-provided passphrase at startup          |
| HSM             | Hardware<br> Security Module via PKCS#11; key never leaves HSM         |
| EXTERNAL_KMS    | AWS<br> KMS, HashiCorp Vault, Azure Key Vault                          |
| TPM             | Trusted<br> Platform Module; ties key to specific hardware             |
| SPLIT_KNOWLEDGE | Shamir's<br> Secret Sharing; N-of-M administrators required at startup |

## **7.3 Encryption Modes**

|            |                                                                                |
| ---------- | ------------------------------------------------------------------------------ |
| **Mode**   | **Description**                                                                |
| NONE       | No<br> encryption (embedded mode or non-sensitive data)                        |
| TDE        | Transparent<br> Data Encryption at page level; default for encrypted databases |
| TABLESPACE | Per-tablespace<br> encryption with separate keys                               |
| TABLE      | Per-table<br> encryption for granular control                                  |
| COLUMN     | Application-assisted<br> column-level encryption                               |

## **7.4 Key Rotation**

Keys are rotated according to policy without service interruption:

- New  key version generated and wrapped by parent key

- New  writes use new key version; old version valid for reads

- Background  process re-encrypts existing data with new key

- I/O  throttling prevents performance impact

- Old  key retired when all data re-encrypted; destroyed after retention
   period

## **7.5 Backup Encryption**

Backups can be encrypted for secure offsite storage:

**CMK-only:** Backup encrypted with key wrapped by CMK; requires CMK access to restore

**CMK Passphrase:** Requires both CMK and backup passphrase; recommended for offsite backups

**Passphrase-only:** Portable backups that can be restored without cluster access

# **8. Wire Protocol Security**

The database engine supports multiple wire protocols while providing a
unified security model.

## **8.1 Supported Protocols**

|                 |                                 |                                             |                         |
| --------------- | ------------------------------- | ------------------------------------------- | ----------------------- |
| **Protocol**    | **TLS Support**                 | **Auth Methods**                            | **Channel Binding**     |
| PostgreSQL      | StartupMessage<br> + SSLRequest | MD5,<br> SCRAM-SHA-256, GSSAPI, Certificate | tls-server-end-point    |
| MySQL           | Handshake<br> flag              | mysql_native,<br> caching_sha2, GSSAPI      | Not<br> standard        |
| MSSQL<br> (TDS) | PRELOGIN<br> negotiation        | SQL<br> Auth, SSPI/Kerberos                 | Extended<br> Protection |
| Firebird        | Connection<br> parameter        | SRP,<br> Legacy, Certificate                | Not<br> standard        |

## **8.2 TLS Configuration**

Each listener can be configured independently:

- tls_mode:  DISABLED, PREFERRED, or REQUIRED

- min_tls_version:  TLS12 or TLS13 (TLS 1.0/1.1 not supported)

- client_cert_mode:  DISABLED, OPTIONAL, or REQUIRED

- Configurable cipher suites per listener

## **8.3 Authentication Normalization**

Each protocol's authentication is normalized to a unified internal format:

- Protocol handler extracts credentials in native format

- Abstraction layer converts to UnifiedAuthRequest

- Authentication dispatcher routes to appropriate provider

- Result converted back to protocol-specific response

## **8.4 Password Verification Strategies**

Different strategies for handling protocol-specific password formats:

**NATIVE_FORMAT:** Store password in protocol's native hash format (compatibility mode)

**CLEARTEXT_REQUIRED:** Request cleartext over TLS, verify against internal hash (most secure)

**PREFER_SCRAM:** Use SCRAM where supported; fall back to cleartext or native

**MULTI_FORMAT:** Store multiple hash formats (legacy compatibility, higher storage cost)

## **8.5 Outbound Connections (UDR)**

When the database connects to external databases:

- Credentials stored encrypted in sys.databases_credentials using Credential Store
   Key

- TLS verification mode configurable (DISABLED, PREFERRED, REQUIRED,
   VERIFY_CA, VERIFY_FULL)

- Connection pooling with configurable limits and timeouts

- Support for password, certificate, and Kerberos authentication

# **9. Audit and Compliance**

The audit subsystem provides comprehensive logging, tamper detection, and
compliance reporting.

## **9.1 Audit Event Categories**

|                   |                                                                                                                     |
| ----------------- | ------------------------------------------------------------------------------------------------------------------- |
| **Category**      | **Events**                                                                                                          |
| AUTHENTICATION    | LOGIN_ATTEMPT,<br> LOGIN_SUCCESS, LOGIN_FAILURE, LOGOUT, SESSION_TIMEOUT,<br> PASSWORD_CHANGE, ACCOUNT_LOCKED       |
| AUTHORIZATION     | PRIVILEGE_GRANTED,<br> PRIVILEGE_REVOKED, ROLE_ASSIGNED, ROLE_REMOVED, ACCESS_DENIED                                |
| DATA_ACCESS       | SELECT,<br> SELECT (SENSITIVE), EXPORT                                                                              |
| DATA_MODIFICATION | INSERT,<br> UPDATE, DELETE, TRUNCATE                                                                                |
| SCHEMA_CHANGE     | CREATE_TABLE,<br> ALTER_TABLE, DROP_TABLE, CREATE_INDEX, DROP_INDEX                                                 |
| SECURITY_ADMIN    | USER_CREATED,<br> USER_MODIFIED, USER_DELETED, USER_BLOCKED, ROLE_CREATED,<br> AUTH_PROVIDER changes                |
| CLUSTER_ADMIN     | SECURITY_DB_ADDED/REMOVED,<br> DATABASE_REGISTERED, QUORUM_LOST/RESTORED, FENCING events,<br> POLICY_CHANGED        |
| ENCRYPTION        | KEY_GENERATED,<br> KEY_ROTATED, KEY_EXPORTED, KEY_DESTROYED, DATABASE_ENCRYPTED,<br> HSM/KMS events                 |
| BACKUP_RESTORE    | BACKUP_STARTED,<br> BACKUP_COMPLETED, BACKUP_FAILED, RESTORE_STARTED,<br> RESTORE_COMPLETED, RESTORE_FAILED         |
| COMPLIANCE        | SENSITIVE_DATA_ACCESSED,<br> DATA_EXPORTED, DATA_ANONYMIZED, RIGHT_TO_ERASURE,<br> DATA_PORTABILITY, CONSENT events |

## **9.2 Audit Log Integrity**

Audit logs are protected against tampering:

**Chain
Hashing:** Each event
includes hash of previous event, creating tamper-evident chain

**Digital
Signatures:** Blocks
of events signed with audit signing key at configurable intervals

**Timestamp
Tokens:** Optional
RFC 3161 timestamps from trusted authority

**Integrity
Verification:** Background
process or on-demand verification of chain integrity

## **9.3 SIEM Integration**

Real-time streaming to security information and event management systems:

- Destination
   types: Syslog, Kafka, Webhook, Splunk HEC, Elasticsearch, CloudWatch

- Output
   formats: JSON, CEF (Common Event Format), LEEF, Syslog RFC5424

- Configurable
   filtering by category, type, and severity

- Batching
   with configurable size and timeout

- Retry
   with backoff for failed deliveries

## **9.4 Alert Engine**

Real-time alerting based on audit events:

- Pattern-based
   rules: Match specific event categories, types, or custom conditions

- Aggregation
   rules: Count, distinct count, or rate thresholds over time windows

- Actions:
   Notifications (email, Slack, PagerDuty), auto-block user/IP, create
   incident, custom SQL

- Deduplication:
   Configurable window to prevent alert storms

## **9.5 Compliance Frameworks**

Pre-built reports and controls mapping:

|               |                                                                                                                                     |
| ------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| **Framework** | **Key<br> Controls/Requirements**                                                                                                   |
| SOC<br> 2     | CC6.1<br> (Access Control), CC6.2 (User Management), CC6.3 (Authorization),<br> CC6.7 (Data Protection), A1.2 (Availability)        |
| HIPAA         | 164.312(a)<br> (Access Control), 164.312(b) (Audit Controls), 164.312(d)<br> (Authentication), 164.308(a)(7) (Contingency)          |
| PCI-DSS       | 3.4<br> (Encryption), 3.5/3.6 (Key Management), 7.1/7.2 (Access Control),<br> 8.2 (Authentication), 10.2 (Audit)                    |
| GDPR          | Article<br> 17 (Erasure), Article 20 (Portability), Article 25 (Privacy by<br> Design), Article 30 (Records), Article 32 (Security) |

## **9.6 Anomaly Detection**

Behavioral
analysis for detecting unusual activity:

- Baseline
   calculation: Per-user metrics by hour and day of week

- Metrics
   tracked: Login count, query count, rows accessed, distinct tables,
   failed operations

- Anomaly
   scoring: Standard deviations from baseline

- Configurable
   thresholds and alert integration

# **10. Policy Reference**

All security behaviors are controlled by configurable policies. Below is
a summary of key policy categories.

## **10.1 Authentication Policies**

|                               |             |                                |
| ----------------------------- | ----------- | ------------------------------ |
| **Policy**                    | **Default** | **Description**                |
| auth.mode                     | LOCAL       | NONE,<br> LOCAL, or CLUSTER    |
| auth.password_hash_algo       | ARGON2ID    | Password<br> hashing algorithm |
| auth.max_failed_attempts      | 5           | Before<br> temporary lockout   |
| auth.lockout_duration_minutes | 15          | Lockout<br> period             |
| auth.session_timeout_minutes  | 480         | Idle<br> session expiry        |

## **10.2 Cluster Policies**

|                                      |                |                                |
| ------------------------------------ | -------------- | ------------------------------ |
| **Policy**                           | **Default**    | **Description**                |
| cluster.min_security_dbs             | 1              | Minimum<br> security databases |
| cluster.quorum_model                 | MAJORITY       | Quorum<br> calculation method  |
| cluster.membership_change_requires   | SUPERMAJORITY  | Approval<br> for membership    |
| cluster.partition_behavior           | FENCE_MINORITY | Split-brain<br> response       |
| cluster.fenced_allow_new_connections | FALSE          | Allow<br> connects when fenced |

## **10.3 Cache Policies**

|                                    |             |                                  |
| ---------------------------------- | ----------- | -------------------------------- |
| **Policy**                         | **Default** | **Description**                  |
| cache.ttl.credentials_seconds      | 300         | Credential<br> cache TTL         |
| cache.ttl.permissions_seconds      | 60          | Permission<br> cache TTL         |
| cache.ttl.degraded_multiplier      | 2.0         | TTL<br> multiplier when degraded |
| cache.use_expired_on_fetch_failure | TRUE        | Use<br> stale on fetch failure   |
| cache.invalidation_mode            | IMMEDIATE   | IMMEDIATE<br> or BATCHED         |

## **10.4 Encryption Policies**

|                                      |             |                              |
| ------------------------------------ | ----------- | ---------------------------- |
| **Policy**                           | **Default** | **Description**              |
| encryption.default_algorithm         | AES-256-GCM | Encryption<br> algorithm     |
| encryption.cmk_source                | PASSPHRASE  | Key<br> derivation source    |
| encryption.database.mode             | TDE         | Database<br> encryption mode |
| encryption.database.encrypt_wal      | TRUE        | Encrypt<br> transaction log  |
| encryption.backup.require_encryption | TRUE        | Force<br> backup encryption  |

## **10.5 Audit Policies**

|                              |             |                              |
| ---------------------------- | ----------- | ---------------------------- |
| **Policy**                   | **Default** | **Description**              |
| audit.enabled                | TRUE        | Enable<br> audit logging     |
| audit.chain_hash_enabled     | TRUE        | Tamper-evident<br> chaining  |
| audit.retention.default_days | 90          | Default<br> retention period |
| audit.archive.enabled        | TRUE        | Archive<br> before delete    |
| audit.stream.enabled         | FALSE       | Enable<br> SIEM streaming    |
| audit.alert.enabled          | TRUE        | Enable<br> alert processing  |

## 10.6 Protocol Policies

|                                          |              |                                  |
| ---------------------------------------- | ------------ | -------------------------------- |
| **Policy**                               | **Default**  | **Description**                  |
| protocol.default.min_tls_version         | TLS12        | Minimum<br> TLS version          |
| protocol.default.require_channel_binding | FALSE        | Require<br> channel binding      |
| protocol.password.verify_strategy        | PREFER_SCRAM | Password<br> verification method |
| protocol.outbound.default_tls_mode       | VERIFY_FULL  | TLS<br> for outbound connections |



## Cache Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         DATA DATABASE                           │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  CREDENTIAL CACHE                        │    │
│  ├─────────────────────────────────────────────────────────┤    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  │    │
│  │  │   User      │  │ Permission  │  │    Session      │  │    │
│  │  │   Cache     │  │   Cache     │  │    Cache        │  │    │
│  │  └─────────────┘  └─────────────┘  └─────────────────┘  │    │
│  │                                                          │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  │    │
│  │  │   Role      │  │  Policy     │  │  Invalidation   │  │    │
│  │  │   Cache     │  │  Cache      │  │  Queue          │  │    │
│  │  └─────────────┘  └─────────────┘  └─────────────────┘  │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              v                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              SECURITY DB CONNECTION POOL                 │    │
│  │     ┌─────────┐    ┌─────────┐    ┌─────────┐           │    │
│  │     │ Sec-DB-1│    │ Sec-DB-2│    │ Sec-DB-3│           │    │
│  │     └─────────┘    └─────────┘    └─────────┘           │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

## Cache Table Schemas

### User Credential Cache

sql

```sql
CREATE TABLE sys.credential_cache (
    user_uuid               UUID NOT NULL,
    username                VARCHAR(128) NOT NULL,
    password_hash           BYTEA NOT NULL,
    hash_algorithm          VARCHAR(16) NOT NULL,  -- 'ARGON2ID', 'BCRYPT', etc.
    salt                    BYTEA,

    -- Status tracking
    user_status             ENUM('ACTIVE', 'BLOCKED', 'SUSPENDED', 'DELETED') NOT NULL,
    block_reason            VARCHAR(64),

    -- Cache metadata
    cached_at               TIMESTAMP NOT NULL,
    refreshed_at            TIMESTAMP NOT NULL,
    expires_at              TIMESTAMP NOT NULL,
    source_security_db      UUID NOT NULL,
    source_config_version   BIGINT NOT NULL,

    -- Invalidation tracking
    invalidated             BOOLEAN DEFAULT FALSE,
    invalidated_at          TIMESTAMP,
    invalidation_reason     VARCHAR(64),

    -- Verification
    cache_entry_hash        BYTEA NOT NULL,  -- hash of all fields for tampering detection

    PRIMARY KEY (user_uuid),
    INDEX idx_username (username),
    INDEX idx_expires (expires_at),
    INDEX idx_invalidated (invalidated, expires_at)
);
```

### Permission Cache

sql

```sql
CREATE TABLE sys.permission_cache (
    user_uuid               UUID NOT NULL,
    permission_type         ENUM('TABLE', 'COLUMN', 'PROCEDURE', 'SCHEMA', 'DATABASE', 'SYSTEM') NOT NULL,
    object_uuid             UUID,  -- NULL for SYSTEM permissions
    object_name             VARCHAR(256),  -- denormalized for fast lookup

    -- Permission bits (bitmask)
    granted_permissions     BIGINT NOT NULL,  -- SELECT=1, INSERT=2, UPDATE=4, DELETE=8, etc.
    denied_permissions      BIGINT NOT NULL,  -- explicit denies
    grant_option            BIGINT NOT NULL,  -- can grant to others

    -- Inheritance tracking
    via_role_uuid           UUID,  -- NULL if direct grant, else the role that provided this
    inheritance_depth       INTEGER NOT NULL DEFAULT 0,

    -- Cache metadata  
    cached_at               TIMESTAMP NOT NULL,
    expires_at              TIMESTAMP NOT NULL,
    source_security_db      UUID NOT NULL,
    source_config_version   BIGINT NOT NULL,

    PRIMARY KEY (user_uuid, permission_type, object_uuid),
    INDEX idx_user (user_uuid),
    INDEX idx_object (object_uuid),
    INDEX idx_expires (expires_at)
);
```

### Role Membership Cache

sql

```sql
CREATE TABLE sys.role_cache (
    user_uuid               UUID NOT NULL,
    role_uuid               UUID NOT NULL,
    role_name               VARCHAR(128) NOT NULL,

    -- Nested role tracking
    via_role_uuid           UUID,  -- if membership is inherited
    inheritance_path        UUID[],  -- full path for debugging

    -- Status
    role_status             ENUM('ACTIVE', 'DISABLED') NOT NULL,
    membership_status       ENUM('ACTIVE', 'SUSPENDED') NOT NULL,

    -- Cache metadata
    cached_at               TIMESTAMP NOT NULL,
    expires_at              TIMESTAMP NOT NULL,
    source_security_db      UUID NOT NULL,

    PRIMARY KEY (user_uuid, role_uuid),
    INDEX idx_role (role_uuid),
    INDEX idx_expires (expires_at)
);
```

### Session Cache

sql

```sql
CREATE TABLE sys.session_cache (
    session_id              UUID PRIMARY KEY,
    user_uuid               UUID NOT NULL,

    -- Session state
    created_at              TIMESTAMP NOT NULL,
    last_activity           TIMESTAMP NOT NULL,
    expires_at              TIMESTAMP NOT NULL,

    -- Authentication details
    auth_method             VARCHAR(32) NOT NULL,  -- 'PASSWORD', 'KERBEROS', 'SSO', etc.
    auth_source_db          UUID NOT NULL,
    authenticated_at        TIMESTAMP NOT NULL,

    -- Current transaction state
    in_transaction          BOOLEAN DEFAULT FALSE,
    transaction_started_at  TIMESTAMP,
    transaction_id          UUID,

    -- Cached permission snapshot (optional optimization)
    permission_snapshot     BYTEA,  -- serialized permission set at session start
    snapshot_version        BIGINT,

    -- Connection metadata
    client_address          INET,
    client_application      VARCHAR(128),

    INDEX idx_user (user_uuid),
    INDEX idx_expires (expires_at),
    INDEX idx_activity (last_activity)
);
```

### Policy Cache

sql

```sql
CREATE TABLE sys.policy_cache (
    policy_key              VARCHAR(64) PRIMARY KEY,
    policy_value            VARCHAR(256) NOT NULL,
    value_type              VARCHAR(16) NOT NULL,

    -- Cache metadata
    cached_at               TIMESTAMP NOT NULL,
    expires_at              TIMESTAMP NOT NULL,
    source_security_db      UUID NOT NULL,
    source_config_version   BIGINT NOT NULL,

    INDEX idx_expires (expires_at)
);
```

### Invalidation Queue

sql

```sql
CREATE TABLE sys.cache_invalidation_queue (
    invalidation_id         UUID PRIMARY KEY,
    received_at             TIMESTAMP NOT NULL,
    processed_at            TIMESTAMP,

    -- What to invalidate
    invalidation_type       ENUM('USER', 'PERMISSION', 'ROLE', 'POLICY', 'ALL') NOT NULL,
    target_uuid             UUID,  -- NULL for ALL or POLICY
    target_pattern          VARCHAR(256),  -- for pattern-based invalidation

    -- Source
    source_security_db      UUID NOT NULL,
    source_config_version   BIGINT NOT NULL,

    -- Processing status
    status                  ENUM('PENDING', 'PROCESSING', 'COMPLETED', 'FAILED') NOT NULL,
    error_message           TEXT,
    retry_count             INTEGER DEFAULT 0,

    INDEX idx_status (status, received_at),
    INDEX idx_target (invalidation_type, target_uuid)
);
```

## TTL Strategy

### Tiered TTL by Sensitivity

Different cache types warrant different TTLs:

sql

```sql
-- Policy defaults (configurable)
cache.ttl.credentials_seconds = 300          -- 5 minutes
cache.ttl.permissions_seconds = 60           -- 1 minute  
cache.ttl.roles_seconds = 120                -- 2 minutes
cache.ttl.policy_seconds = 600               -- 10 minutes
cache.ttl.session_idle_seconds = 1800        -- 30 minutes

-- During degraded mode (security DB unreachable)
cache.ttl.degraded_multiplier = 2.0          -- double TTLs
cache.ttl.degraded_max_seconds = 3600        -- but never more than 1 hour

-- Hard limits
cache.ttl.absolute_max_seconds = 86400       -- nothing cached > 24 hours ever
```

### TTL Calculation Logic
```
function calculate_ttl(cache_type, cluster_state):
    base_ttl = policy.get('cache.ttl.' + cache_type + '_seconds')

    if cluster_state == HEALTHY:
        return base_ttl

    elif cluster_state == FENCED or cluster_state == DEGRADED:
        multiplier = policy.get('cache.ttl.degraded_multiplier')
        degraded_max = policy.get('cache.ttl.degraded_max_seconds')
        absolute_max = policy.get('cache.ttl.absolute_max_seconds')

        extended_ttl = base_ttl * multiplier
        return min(extended_ttl, degraded_max, absolute_max)

    elif cluster_state == ISOLATED:
        -- No TTL extension when isolated; we're likely the problem
        return base_ttl
```

### Sliding vs Fixed Expiration

**Fixed Expiration (Default):** Entry expires at `cached_at + ttl`, regardless of access. More secure because stale data eventually expires.

**Sliding Expiration (Optional):** Each access extends expiration. Better availability but risk of indefinitely caching stale data.



```sql

-- Policy setting
cache.expiration_mode = 'FIXED'  -- or 'SLIDING'
cache.sliding_max_extensions = 3  -- if SLIDING, limit how many times

-- Implementation for sliding:
function on_cache_access(entry):
    if policy.expiration_mode == 'SLIDING':
        if entry.extension_count < policy.sliding_max_extensions:
            entry.expires_at = now() + entry.original_ttl
            entry.extension_count += 1
```

## Cache Population Strategy

### On-Demand Population
```
Authentication request for user 'alice':
    │
    ├── Check credential_cache for 'alice'
    │       ├── Found and valid → use cached
    │       ├── Found but expired → refresh
    │       └── Not found → fetch
    │
    ├── Fetch from security DB:
    │       │
    │       ├── Try primary security DB
    │       │       ├── Success → cache and use
    │       │       └── Fail → try next
    │       │
    │       ├── Try secondary security DB
    │       │       └── ...
    │       │
    │       └── All failed:
    │               ├── If expired entry exists:
    │               │       └── Policy: cache.use_expired_on_fetch_failure
    │               │               ├── TRUE → use expired with warning
    │               │               └── FALSE → reject authentication
    │               │
    │               └── No entry exists → reject authentication
    │
    └── Return authentication result
```

### Proactive Refresh

Background thread refreshes entries before expiration:
```
function cache_refresh_daemon():
    refresh_threshold = policy.get('cache.refresh_before_expiry_seconds')  -- e.g., 60

    while running:
        sleep(policy.get('cache.refresh_check_interval_seconds'))  -- e.g., 10

        -- Find entries expiring soon
        expiring_entries = SELECT * FROM sys.credential_cache
                          WHERE expires_at < now() + refresh_threshold
                          AND invalidated = FALSE
                          ORDER BY expires_at
                          LIMIT policy.get('cache.refresh_batch_size')  -- e.g., 100

        for entry in expiring_entries:
            if security_db_available():
                try:
                    fresh_data = fetch_from_security_db(entry.user_uuid)
                    update_cache(entry.user_uuid, fresh_data)
                except:
                    log_warning("Failed to refresh cache for " + entry.user_uuid)
                    -- Entry will expire naturally; on-demand fetch will retry
```

### Warm-Up on Startup
```
function cache_warmup():
    if not policy.get('cache.warmup_enabled'):
        return

    -- Option 1: Warm from active sessions
    if policy.get('cache.warmup_from_sessions'):
        active_users = SELECT DISTINCT user_uuid FROM sys.session_cache
                      WHERE expires_at > now()

        for user_uuid in active_users:
            prefetch_user_credentials(user_uuid)
            prefetch_user_permissions(user_uuid)

    -- Option 2: Warm from security DB's "hot users" list
    if policy.get('cache.warmup_from_security_db'):
        hot_users = call_security_db('get_recently_active_users', 
                                      limit=policy.get('cache.warmup_limit'))

        for user_uuid in hot_users:
            prefetch_user_credentials(user_uuid)
```

## Invalidation Strategies

### Push Invalidation (Primary)

Security DB pushes invalidation notices to data DBs:
```
On security DB, after REVOKE or password change:
    │
    ├── Commit change locally
    │
    ├── Replicate to peer security DBs (quorum)
    │
    └── Broadcast invalidation to all registered data DBs:
            │
            ├── Build invalidation message:
            │       {
            │           invalidation_id: UUID,
            │           type: 'PERMISSION',
            │           target_uuid: user-uuid,
            │           timestamp: now(),
            │           source_config_version: current_version
            │       }
            │
            ├── For each data DB in cluster:
            │       ├── Send invalidation message
            │       ├── Wait for ACK (with timeout)
            │       └── Log delivery status
            │
            └── For unacknowledged data DBs:
                    └── Queue for retry
```

**Data DB Invalidation Handler:**
```
function handle_invalidation(message):
    -- Deduplicate
    if exists_in_invalidation_queue(message.invalidation_id):
        return ACK  -- already processed

    -- Record receipt
    INSERT INTO sys.cache_invalidation_queue (
        invalidation_id = message.invalidation_id,
        received_at = now(),
        invalidation_type = message.type,
        target_uuid = message.target_uuid,
        source_security_db = message.source,
        source_config_version = message.source_config_version,
        status = 'PENDING'
    )

    -- Process immediately or queue for batch
    if policy.get('cache.invalidation_mode') == 'IMMEDIATE':
        process_invalidation(message)

    return ACK
```

**Processing Invalidation:**
```
function process_invalidation(message):
    BEGIN TRANSACTION

    UPDATE sys.cache_invalidation_queue 
    SET status = 'PROCESSING'
    WHERE invalidation_id = message.invalidation_id

    switch message.type:
        case 'USER':
            -- Invalidate all caches for this user
            UPDATE sys.credential_cache 
            SET invalidated = TRUE, 
                invalidated_at = now(),
                invalidation_reason = 'PUSH_INVALIDATION'
            WHERE user_uuid = message.target_uuid

            UPDATE sys.permission_cache
            SET -- mark invalid or delete
            WHERE user_uuid = message.target_uuid

            UPDATE sys.role_cache
            SET -- mark invalid or delete
            WHERE user_uuid = message.target_uuid

            -- Handle active sessions for this user
            handle_invalidated_user_sessions(message.target_uuid)

        case 'PERMISSION':
            UPDATE sys.permission_cache
            SET -- invalidate
            WHERE user_uuid = message.target_uuid

        case 'ROLE':
            -- Invalidate role and all users with this role
            affected_users = SELECT user_uuid FROM sys.role_cache
                            WHERE role_uuid = message.target_uuid

            for user_uuid in affected_users:
                invalidate_user_permissions(user_uuid)

        case 'POLICY':
            DELETE FROM sys.policy_cache 
            WHERE policy_key = message.target_pattern

        case 'ALL':
            TRUNCATE sys.credential_cache
            TRUNCATE sys.permission_cache
            TRUNCATE sys.role_cache
            TRUNCATE sys.policy_cache
            -- Sessions remain; they'll re-auth on next action

    UPDATE sys.cache_invalidation_queue
    SET status = 'COMPLETED', processed_at = now()
    WHERE invalidation_id = message.invalidation_id

    COMMIT
```

### Pull Invalidation (Backup)

If push fails or for consistency verification:
```
function pull_invalidation_check():
    -- Called periodically or on reconnection to security DB

    my_version = SELECT MAX(source_config_version) FROM sys.credential_cache

    security_version = call_security_db('get_config_version')

    if security_version > my_version:
        -- We've missed invalidations
        missed = call_security_db('get_invalidations_since', my_version)

        for invalidation in missed:
            handle_invalidation(invalidation)
```

### Version Vector Validation

Each cache entry tracks its source version. On any cache hit:
```
function validate_cache_entry(entry):
    -- Fast path: entry not expired
    if entry.expires_at > now() and not entry.invalidated:
        -- Optional: periodic version check
        if policy.get('cache.validate_version_on_access'):
            if random() < policy.get('cache.version_check_probability'):  -- e.g., 0.01
                spawn_async(verify_entry_version, entry)

        return VALID

    return EXPIRED_OR_INVALID

function verify_entry_version(entry):
    -- Background check that entry is still current
    current_version = call_security_db('get_user_version', entry.user_uuid)

    if current_version > entry.source_config_version:
        invalidate_entry(entry)
```

## Session Handling During Invalidation

### Active Session Impact
```
function handle_invalidated_user_sessions(user_uuid):
    sessions = SELECT * FROM sys.session_cache 
               WHERE user_uuid = user_uuid

    for session in sessions:
        if session.in_transaction:
            -- Policy: allow transaction to complete
            mark_session_blocked_after_transaction(session.session_id)

            -- Set a timeout for transaction completion
            schedule_session_termination(
                session.session_id, 
                delay = policy.get('cache.blocked_transaction_timeout_seconds')
            )
        else:
            -- Not in transaction; can block immediately
            mark_session_blocked(session.session_id)

    -- Notify affected sessions on next interaction
```

### Session State Transitions
```
SESSION STATES:

ACTIVE
    │
    ├── user invalidated, not in txn ──────────> BLOCKED
    │
    ├── user invalidated, in txn ──────────────> BLOCKED_PENDING
    │       │
    │       ├── txn completes (commit/rollback) ──> BLOCKED
    │       │
    │       └── timeout expires ───────────────────> TERMINATED
    │
    ├── credential expired, security DB available ──> RE_AUTH_REQUIRED
    │       │
    │       └── re-auth succeeds ──────────────────> ACTIVE
    │
    └── credential expired, security DB unavailable:
            │
            ├── policy allows cached ───────────────> DEGRADED
            │       │
            │       └── security DB returns ───────> ACTIVE (after re-validation)
            │
            └── policy denies cached ───────────────> TERMINATED
```

## Cache Size Management

### Eviction Policy

sql

```sql
-- Policy settings
cache.max_credential_entries = 100000
cache.max_permission_entries = 1000000
cache.max_role_entries = 500000
cache.eviction_policy = 'LRU'  -- or 'TTL_FIRST', 'FIFO'
cache.eviction_batch_size = 1000
cache.eviction_trigger_percent = 90  -- start evicting at 90% capacity
```

**Eviction Process:**
```
function check_cache_capacity():
    for cache_table in [credential_cache, permission_cache, role_cache]:
        current_count = SELECT COUNT(*) FROM cache_table
        max_count = policy.get('cache.max_' + cache_table + '_entries')

        if current_count > max_count * policy.eviction_trigger_percent / 100:
            evict_entries(cache_table, current_count - max_count * 0.8)

function evict_entries(cache_table, count_to_evict):
    switch policy.eviction_policy:
        case 'LRU':
            -- Requires tracking last_accessed; add column if using LRU
            DELETE FROM cache_table
            WHERE id IN (
                SELECT id FROM cache_table
                WHERE invalidated = TRUE  -- evict invalidated first
                ORDER BY last_accessed
                LIMIT count_to_evict
            )

        case 'TTL_FIRST':
            DELETE FROM cache_table
            WHERE id IN (
                SELECT id FROM cache_table
                ORDER BY expires_at  -- soonest to expire first
                LIMIT count_to_evict
            )

        case 'FIFO':
            DELETE FROM cache_table
            WHERE id IN (
                SELECT id FROM cache_table
                ORDER BY cached_at
                LIMIT count_to_evict
            )
```

## Security Considerations

### Cache Entry Integrity
```
function compute_cache_entry_hash(entry):
    -- Hash all security-relevant fields
    data = concat(
        entry.user_uuid,
        entry.password_hash,
        entry.user_status,
        entry.source_security_db,
        entry.source_config_version
    )
    return hmac_sha256(data, local_cache_secret_key)

function verify_cache_entry_integrity(entry):
    expected_hash = compute_cache_entry_hash(entry)
    if entry.cache_entry_hash != expected_hash:
        log_security_event('CACHE_TAMPERING_DETECTED', entry)
        invalidate_entry(entry)
        return FALSE
    return TRUE
```

### Cache Isolation
```
-- Cache tables should be:
-- 1. Not accessible via normal SQL (system tables only)
-- 2. Not included in backups (or encrypted separately)
-- 3. Memory-only option for highest security

cache.storage_mode = 'DISK'  -- or 'MEMORY', 'ENCRYPTED_DISK'

-- If MEMORY, caches don't survive restart (cold start re-fetches)
-- If ENCRYPTED_DISK, uses separate encryption key from data
```

### Credential Hash Security
```
-- Never cache plaintext passwords (obvious, but stated for spec)
-- Password hashes are safe to cache; they're already stored hashed on security DB

-- Additional protection: re-encrypt hashes with local key
function cache_credential(user_uuid, password_hash_from_security_db):
    local_encrypted = encrypt_aes256(
        password_hash_from_security_db, 
        local_cache_encryption_key
    )

    INSERT INTO sys.credential_cache (
        user_uuid = user_uuid,
        password_hash = local_encrypted,  -- double protection
        ...
    )

function verify_password(user_uuid, provided_password):
    entry = SELECT * FROM sys.credential_cache WHERE user_uuid = user_uuid

    original_hash = decrypt_aes256(entry.password_hash, local_cache_encryption_key)

    return verify_hash(provided_password, original_hash, entry.hash_algorithm)
```

## Metrics and Monitoring

sql

```sql
CREATE TABLE sys.cache_metrics (
    metric_timestamp        TIMESTAMP NOT NULL,

    -- Hit rates
    credential_hits         BIGINT,
    credential_misses       BIGINT,
    permission_hits         BIGINT,
    permission_misses       BIGINT,

    -- Cache state
    credential_entries      INTEGER,
    permission_entries      INTEGER,
    role_entries            INTEGER,
    expired_entries         INTEGER,
    invalidated_entries     INTEGER,

    -- Invalidation stats
    push_invalidations_received    INTEGER,
    pull_invalidations_fetched     INTEGER,
    invalidation_processing_time_ms INTEGER,

    -- Errors
    fetch_failures          INTEGER,
    security_db_timeouts    INTEGER,
    integrity_failures      INTEGER,

    PRIMARY KEY (metric_timestamp)
);

-- Emit metrics every minute
function emit_cache_metrics():
    INSERT INTO sys.cache_metrics (
        metric_timestamp = now(),
        credential_hits = atomic_swap(credential_hit_counter, 0),
        credential_misses = atomic_swap(credential_miss_counter, 0),
        -- ... etc
    )
```

**Alert Conditions:**
```
-- High miss rate indicates cache thrashing or undersized cache
ALERT IF credential_misses / (credential_hits + credential_misses) > 0.3
       FOR 5 MINUTES

-- Integrity failures indicate tampering or bugs
ALERT IF integrity_failures > 0

-- Fetch failures during healthy cluster indicate network issues
ALERT IF fetch_failures > 10 AND cluster_state = 'HEALTHY'
       FOR 1 MINUTE

-- Growing invalidation queue indicates processing bottleneck
ALERT IF (SELECT COUNT(*) FROM sys.cache_invalidation_queue 
          WHERE status = 'PENDING') > 1000
```

## Policy Summary Table

| Policy Key                                  | Type    | Default   | Description                  |
| ------------------------------------------- | ------- | --------- | ---------------------------- |
| `cache.ttl.credentials_seconds`             | INTEGER | 300       | Credential cache TTL         |
| `cache.ttl.permissions_seconds`             | INTEGER | 60        | Permission cache TTL         |
| `cache.ttl.roles_seconds`                   | INTEGER | 120       | Role cache TTL               |
| `cache.ttl.policy_seconds`                  | INTEGER | 600       | Policy cache TTL             |
| `cache.ttl.degraded_multiplier`             | FLOAT   | 2.0       | TTL multiplier when degraded |
| `cache.ttl.degraded_max_seconds`            | INTEGER | 3600      | Max TTL when degraded        |
| `cache.ttl.absolute_max_seconds`            | INTEGER | 86400     | Absolute max TTL ever        |
| `cache.expiration_mode`                     | ENUM    | FIXED     | FIXED or SLIDING             |
| `cache.sliding_max_extensions`              | INTEGER | 3         | Max extensions if SLIDING    |
| `cache.refresh_before_expiry_seconds`       | INTEGER | 60        | Proactive refresh window     |
| `cache.refresh_check_interval_seconds`      | INTEGER | 10        | Refresh daemon interval      |
| `cache.refresh_batch_size`                  | INTEGER | 100       | Entries per refresh cycle    |
| `cache.warmup_enabled`                      | BOOLEAN | FALSE     | Enable cache warmup          |
| `cache.warmup_from_sessions`                | BOOLEAN | TRUE      | Warm from active sessions    |
| `cache.warmup_from_security_db`             | BOOLEAN | FALSE     | Warm from security DB list   |
| `cache.warmup_limit`                        | INTEGER | 1000      | Max entries to warm          |
| `cache.use_expired_on_fetch_failure`        | BOOLEAN | TRUE      | Use stale on fetch fail      |
| `cache.invalidation_mode`                   | ENUM    | IMMEDIATE | IMMEDIATE or BATCHED         |
| `cache.validate_version_on_access`          | BOOLEAN | FALSE     | Probabilistic version check  |
| `cache.version_check_probability`           | FLOAT   | 0.01      | Check probability            |
| `cache.max_credential_entries`              | INTEGER | 100000    | Max cached users             |
| `cache.max_permission_entries`              | INTEGER | 1000000   | Max cached permissions       |
| `cache.max_role_entries`                    | INTEGER | 500000    | Max cached role memberships  |
| `cache.eviction_policy`                     | ENUM    | LRU       | LRU, TTL_FIRST, FIFO         |
| `cache.eviction_trigger_percent`            | INTEGER | 90        | Eviction threshold           |
| `cache.storage_mode`                        | ENUM    | DISK      | DISK, MEMORY, ENCRYPTED_DISK |
| `cache.blocked_transaction_timeout_seconds` | INTEGER | 300       | Force-kill blocked txn       |

The authentication UDR framework is the extension point where external identity systems integrate. This needs to be flexible enough to support diverse protocols while being locked down enough that a malicious UDR can't compromise the engine.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            CLIENT CONNECTION                                 │
│                                                                             │
│   username: alice                                                           │
│   auth_method: KERBEROS                                                     │
│   credentials: <kerberos ticket>                                            │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      v
┌─────────────────────────────────────────────────────────────────────────────┐
│                         AUTHENTICATION DISPATCHER                            │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  1. Parse authentication request                                     │   │
│   │  2. Lookup user → determine allowed auth methods                     │   │
│   │  3. Select appropriate auth provider                                 │   │
│   │  4. Invoke provider UDR                                              │   │
│   │  5. Process result                                                   │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                    ┌─────────────────┼─────────────────┐
                    v                 v                 v
          ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
          │  LOCAL_PASSWORD │ │    KERBEROS     │ │      LDAP       │
          │     (built-in)  │ │      UDR        │ │      UDR        │
          └─────────────────┘ └─────────────────┘ └─────────────────┘
                                      │
                                      v
                            ┌─────────────────┐
                            │  External KDC   │
                            └─────────────────┘
```

## Authentication Provider Registry

### Provider Registration Schema

sql

```sql
CREATE TABLE sys.auth_providers (
    provider_id             UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    provider_name           VARCHAR(64) NOT NULL UNIQUE,
    provider_type           ENUM('BUILTIN', 'UDR', 'EXTERNAL_SERVICE') NOT NULL,

    -- For UDR providers
    library_path            VARCHAR(512),
    entry_point             VARCHAR(128),  -- function name in library

    -- Trust verification
    library_checksum        BYTEA,  -- SHA-256 of library file
    library_signature       BYTEA,  -- optional PGP/X.509 signature
    signature_key_id        VARCHAR(64),  -- key used for signature
    checksum_verified_at    TIMESTAMP,

    -- Provider status
    status                  ENUM('ACTIVE', 'DISABLED', 'PENDING_VERIFICATION') NOT NULL,
    priority                INTEGER NOT NULL DEFAULT 100,  -- lower = tried first for multi-method

    -- Capabilities
    supports_password       BOOLEAN DEFAULT FALSE,
    supports_token          BOOLEAN DEFAULT FALSE,
    supports_certificate    BOOLEAN DEFAULT FALSE,
    supports_ticket         BOOLEAN DEFAULT FALSE,  -- Kerberos
    supports_challenge      BOOLEAN DEFAULT FALSE,  -- multi-step auth

    -- Runtime constraints
    timeout_ms              INTEGER NOT NULL DEFAULT 5000,
    max_retries             INTEGER NOT NULL DEFAULT 0,
    sandbox_level           ENUM('NONE', 'RESTRICTED', 'STRICT') NOT NULL DEFAULT 'RESTRICTED',

    -- Audit
    registered_by           UUID NOT NULL,  -- admin who registered
    registered_at           TIMESTAMP NOT NULL DEFAULT now(),
    last_modified_by        UUID,
    last_modified_at        TIMESTAMP,

    -- Configuration (provider-specific)
    config                  JSONB,

    INDEX idx_status (status),
    INDEX idx_type (provider_type)
);
```

### Provider Configuration Schema

sql

```sql
CREATE TABLE sys.auth_provider_config (
    provider_id             UUID NOT NULL REFERENCES sys.auth_providers(provider_id),
    config_key              VARCHAR(128) NOT NULL,
    config_value            TEXT,
    is_secret               BOOLEAN DEFAULT FALSE,  -- encrypted storage

    PRIMARY KEY (provider_id, config_key)
);

-- Example configurations:

-- Kerberos provider
INSERT INTO sys.auth_provider_config VALUES
    ('kerberos-uuid', 'realm', 'EXAMPLE.COM', FALSE),
    ('kerberos-uuid', 'kdc_host', 'kdc.example.com', FALSE),
    ('kerberos-uuid', 'kdc_port', '88', FALSE),
    ('kerberos-uuid', 'service_principal', 'mydb/dbserver.example.com', FALSE),
    ('kerberos-uuid', 'keytab_path', '/etc/mydb/mydb.keytab', FALSE);

-- LDAP provider
INSERT INTO sys.auth_provider_config VALUES
    ('ldap-uuid', 'server_uri', 'ldaps://ldap.example.com:636', FALSE),
    ('ldap-uuid', 'bind_dn', 'cn=dbauth,ou=services,dc=example,dc=com', FALSE),
    ('ldap-uuid', 'bind_password', '<encrypted>', TRUE),
    ('ldap-uuid', 'user_base_dn', 'ou=users,dc=example,dc=com', FALSE),
    ('ldap-uuid', 'user_filter', '(&(objectClass=person)(uid=%s))', FALSE),
    ('ldap-uuid', 'group_base_dn', 'ou=groups,dc=example,dc=com', FALSE),
    ('ldap-uuid', 'tls_ca_cert', '/etc/mydb/ldap-ca.pem', FALSE);

-- OIDC/SSO provider  
INSERT INTO sys.auth_provider_config VALUES
    ('oidc-uuid', 'issuer', 'https://auth.example.com', FALSE),
    ('oidc-uuid', 'client_id', 'mydb-client', FALSE),
    ('oidc-uuid', 'client_secret', '<encrypted>', TRUE),
    ('oidc-uuid', 'token_endpoint', 'https://auth.example.com/oauth/token', FALSE),
    ('oidc-uuid', 'userinfo_endpoint', 'https://auth.example.com/oauth/userinfo', FALSE),
    ('oidc-uuid', 'jwks_uri', 'https://auth.example.com/.well-known/jwks.json', FALSE);
```

### User-Provider Mapping

sql

```sql
CREATE TABLE sys.user_auth_methods (
    user_uuid               UUID NOT NULL,
    provider_id             UUID NOT NULL REFERENCES sys.auth_providers(provider_id),
    priority                INTEGER NOT NULL DEFAULT 100,  -- try order

    -- Provider-specific user identifier
    external_id             VARCHAR(256),  -- e.g., LDAP DN, Kerberos principal

    -- Method status
    status                  ENUM('ACTIVE', 'DISABLED') NOT NULL DEFAULT 'ACTIVE',

    -- Constraints
    allowed_from            INET[],  -- NULL = anywhere; else restrict by source IP
    allowed_times           JSONB,   -- time-of-day restrictions

    -- Audit
    created_at              TIMESTAMP NOT NULL DEFAULT now(),
    last_used_at            TIMESTAMP,
    last_success_at         TIMESTAMP,
    failure_count           INTEGER DEFAULT 0,

    PRIMARY KEY (user_uuid, provider_id),
    INDEX idx_provider (provider_id)
);
```

## UDR Interface Specification

### Authentication Request Structure

c

```c
typedef struct AuthRequest {
    /* Request identification */
    uuid_t          request_id;
    timestamp_t     request_time;

    /* User identification (may be partially filled) */
    char*           username;           /* provided by client */
    uuid_t          user_uuid;          /* looked up, may be NULL_UUID if unknown */
    char*           external_id;        /* from user_auth_methods mapping */

    /* Credentials (type depends on auth method) */
    AuthCredentialType  credential_type;
    union {
        PasswordCredential      password;
        TokenCredential         token;
        CertificateCredential   certificate;
        KerberosCredential      kerberos;
        ChallengeResponse       challenge_response;
    } credentials;

    /* Connection context */
    char*           client_address;
    int             client_port;
    char*           client_application;
    char*           server_hostname;
    int             server_port;
    char*           database_name;

    /* Protocol context */
    WireProtocol    wire_protocol;      /* POSTGRES, MYSQL, MSSQL, FIREBIRD */

    /* For multi-step authentication */
    uuid_t          conversation_id;    /* NULL_UUID for first step */
    int             step_number;
    void*           conversation_state; /* opaque, provider-managed */

} AuthRequest;

typedef struct PasswordCredential {
    char*           password;           /* cleartext from client - SENSITIVE */
    size_t          password_length;
} PasswordCredential;

typedef struct TokenCredential {
    char*           token;              /* JWT, OIDC token, etc. */
    size_t          token_length;
    char*           token_type;         /* "Bearer", "Basic", etc. */
} TokenCredential;

typedef struct KerberosCredential {
    void*           ticket_data;        /* AP-REQ */
    size_t          ticket_length;
    char*           service_principal;
} KerberosCredential;

typedef struct CertificateCredential {
    void*           certificate_der;    /* X.509 DER encoded */
    size_t          certificate_length;
    void*           signature;          /* challenge signature */
    size_t          signature_length;
} CertificateCredential;

typedef struct ChallengeResponse {
    void*           challenge;          /* from previous step */
    size_t          challenge_length;
    void*           response;           /* client response */
    size_t          response_length;
} ChallengeResponse;
```

### Authentication Response Structure

c

```c
typedef enum AuthResultCode {
    AUTH_SUCCESS            = 0,
    AUTH_FAILED             = 1,    /* credentials invalid */
    AUTH_USER_NOT_FOUND     = 2,    /* user unknown to provider */
    AUTH_USER_DISABLED      = 3,    /* user exists but disabled */
    AUTH_USER_LOCKED        = 4,    /* too many failures */
    AUTH_EXPIRED            = 5,    /* password/token expired */
    AUTH_CONTINUE           = 6,    /* multi-step: send challenge */
    AUTH_PROVIDER_ERROR     = 7,    /* provider internal error */
    AUTH_TIMEOUT            = 8,    /* external service timeout */
    AUTH_CONFIG_ERROR       = 9,    /* provider misconfigured */
    AUTH_PROTOCOL_ERROR     = 10,   /* client sent invalid data */
} AuthResultCode;

typedef struct AuthResponse {
    /* Result */
    AuthResultCode      result_code;
    char*               result_message;     /* human-readable, for logging */
    char*               error_detail;       /* technical detail, not sent to client */

    /* Successful auth info */
    uuid_t              authenticated_user_uuid;    /* confirmed identity */
    char*               authenticated_name;         /* canonical name from provider */
    timestamp_t         auth_valid_until;           /* session expiry hint */

    /* Additional attributes from provider */
    AuthAttribute*      attributes;         /* linked list */
    int                 attribute_count;

    /* For multi-step auth (result_code == AUTH_CONTINUE) */
    void*               challenge;
    size_t              challenge_length;
    void*               conversation_state; /* engine stores, passes back next step */
    size_t              state_length;
    char*               challenge_prompt;   /* human-readable prompt */

    /* Group/role information from external provider */
    char**              external_groups;    /* groups from LDAP, OIDC claims, etc. */
    int                 group_count;

    /* Audit information */
    char*               auth_source;        /* e.g., "ldap://ldap.example.com" */
    char*               auth_method_detail; /* e.g., "Kerberos5 AES256" */

} AuthResponse;

typedef struct AuthAttribute {
    char*               name;
    char*               value;
    AuthAttributeType   type;       /* STRING, INTEGER, BOOLEAN, TIMESTAMP */
    struct AuthAttribute* next;
} AuthAttribute;
```

### UDR Entry Point Signature

c

```c
/*
 * Main authentication entry point.
 * 
 * The UDR must implement this exact signature.
 * 
 * Parameters:
 *   request     - Authentication request (read-only)
 *   config      - Provider configuration key-value pairs
 *   response    - Output: authentication result (UDR allocates, engine frees)
 *   engine_ctx  - Opaque engine context for callbacks
 * 
 * Returns:
 *   0 on successful processing (check response->result_code for auth result)
 *   Non-zero on fatal UDR error
 * 
 * Threading:
 *   May be called concurrently; must be thread-safe.
 *   Do not store state in global variables.
 * 
 * Memory:
 *   Use engine_alloc/engine_free for response allocations.
 *   Engine will free response after processing.
 */
typedef int (*AuthProviderFunc)(
    const AuthRequest*      request,
    const AuthConfig*       config,
    AuthResponse**          response,
    EngineContext*          engine_ctx
);

/*
 * Provider initialization.
 * Called once when provider is loaded/enabled.
 * 
 * Use for: connection pool setup, config validation, etc.
 */
typedef int (*AuthProviderInitFunc)(
    const AuthConfig*       config,
    EngineContext*          engine_ctx,
    void**                  provider_state
);

/*
 * Provider shutdown.
 * Called when provider is disabled/unloaded.
 */
typedef void (*AuthProviderShutdownFunc)(
    void*                   provider_state,
    EngineContext*          engine_ctx
);

/*
 * Configuration validation.
 * Called before provider is activated.
 * Return 0 if config is valid, non-zero with error message if not.
 */
typedef int (*AuthProviderValidateConfigFunc)(
    const AuthConfig*       config,
    char**                  error_message
);

/*
 * Provider manifest - exported by the UDR library.
 */
typedef struct AuthProviderManifest {
    uint32_t                manifest_version;   /* must be 1 */
    char*                   provider_name;
    char*                   provider_version;
    char*                   provider_author;

    /* Required functions */
    AuthProviderFunc            authenticate;

    /* Optional functions (NULL if not implemented) */
    AuthProviderInitFunc        init;
    AuthProviderShutdownFunc    shutdown;
    AuthProviderValidateConfigFunc validate_config;

    /* Capability flags */
    uint32_t                capabilities;       /* bitmask of AUTH_CAP_* */

} AuthProviderManifest;

#define AUTH_CAP_PASSWORD       0x0001
#define AUTH_CAP_TOKEN          0x0002
#define AUTH_CAP_CERTIFICATE    0x0004
#define AUTH_CAP_KERBEROS       0x0008
#define AUTH_CAP_MULTI_STEP     0x0010
#define AUTH_CAP_GROUP_MAPPING  0x0020

/* The UDR must export this symbol */
extern AuthProviderManifest* auth_provider_manifest;
```

### Engine Callbacks Available to UDRs

c

```c
/*
 * Callbacks the UDR can invoke on the engine.
 * These are provided via the EngineContext parameter.
 */
typedef struct EngineContext {
    /* Memory management - UDR MUST use these, not malloc/free */
    void* (*engine_alloc)(size_t size);
    void  (*engine_free)(void* ptr);
    void* (*engine_realloc)(void* ptr, size_t size);
    char* (*engine_strdup)(const char* str);

    /* Logging */
    void (*log_debug)(const char* fmt, ...);
    void (*log_info)(const char* fmt, ...);
    void (*log_warning)(const char* fmt, ...);
    void (*log_error)(const char* fmt, ...);
    void (*log_security)(const char* event_type, const char* detail);

    /* Configuration access (secrets decrypted automatically) */
    const char* (*get_config)(const char* key);
    int         (*get_config_int)(const char* key, int default_val);
    bool        (*get_config_bool)(const char* key, bool default_val);

    /* Time - use engine time, not system clock (for testing/consistency) */
    timestamp_t (*current_time)(void);

    /* Secure memory for credentials (won't swap, zeroed on free) */
    void* (*secure_alloc)(size_t size);
    void  (*secure_free)(void* ptr, size_t size);

    /* HTTP client for OIDC, REST APIs (uses engine connection pooling) */
    HttpResponse* (*http_request)(const HttpRequest* req);
    void          (*http_response_free)(HttpResponse* resp);

    /* LDAP client (if LDAP support compiled in) */
    LdapConnection* (*ldap_connect)(const char* uri, const char* bind_dn, 
                                     const char* password);
    LdapResult*     (*ldap_search)(LdapConnection* conn, const char* base_dn,
                                    const char* filter, const char** attrs);
    void            (*ldap_close)(LdapConnection* conn);

    /* Kerberos (if Kerberos support compiled in) */
    int (*krb5_verify_ticket)(const void* ticket, size_t len, 
                               const char* service_principal,
                               const char* keytab_path,
                               char** client_principal);

    /* Certificate validation */
    int (*x509_validate)(const void* cert_der, size_t len,
                          const char* ca_cert_path,
                          char** subject_dn, char** issuer_dn);

    /* Engine-managed state storage (persistent across calls) */
    void* (*state_get)(const char* key);
    void  (*state_set)(const char* key, void* value, size_t size);
    void  (*state_delete)(const char* key);

    /* Opaque internal pointer - do not access */
    void* _internal;

} EngineContext;
```

## Sandbox Levels

### Sandbox Level Definitions

sql

```sql
CREATE TYPE sandbox_level AS ENUM ('NONE', 'RESTRICTED', 'STRICT');
```

| Capability       | NONE      | RESTRICTED          | STRICT   |
| ---------------- | --------- | ------------------- | -------- |
| Network access   | Unlimited | Engine-proxied only | None     |
| Filesystem read  | Unlimited | Config paths only   | None     |
| Filesystem write | Unlimited | None                | None     |
| System calls     | All       | Allowlisted         | Minimal  |
| Memory limit     | None      | Configurable        | 64MB     |
| CPU time limit   | None      | Configurable        | 1 second |
| Thread creation  | Allowed   | Limited             | None     |
| Signal handling  | Allowed   | None                | None     |
| exec/fork        | Allowed   | None                | None     |

### Sandbox Implementation Strategy

c

```c
/*
 * For RESTRICTED and STRICT sandboxing on Linux, use seccomp-bpf.
 * This is applied before calling the UDR.
 */

typedef struct SandboxConfig {
    SandboxLevel    level;

    /* For RESTRICTED */
    size_t          memory_limit_bytes;
    int             cpu_time_limit_ms;
    int             max_threads;
    char**          allowed_paths;      /* filesystem whitelist */
    int             allowed_path_count;

    /* Network proxy config */
    bool            allow_http;
    bool            allow_https;
    bool            allow_ldap;
    bool            allow_ldaps;
    char**          allowed_hosts;      /* NULL = use provider config hosts */
    int             allowed_host_count;

} SandboxConfig;

/*
 * Wrapper that applies sandbox before invoking UDR.
 */
int invoke_auth_udr_sandboxed(
    AuthProviderFunc        func,
    const AuthRequest*      request,
    const AuthConfig*       config,
    AuthResponse**          response,
    EngineContext*          engine_ctx,
    const SandboxConfig*    sandbox
) {
    pid_t child = fork();

    if (child == 0) {
        /* Child process */
        apply_seccomp_filter(sandbox);
        apply_rlimits(sandbox);

        int result = func(request, config, response, engine_ctx);

        /* Send result back via pipe/shared memory */
        exit(result);
    } else {
        /* Parent: wait with timeout */
        int status;
        int wait_result = waitpid_timeout(child, &status, sandbox->cpu_time_limit_ms);

        if (wait_result == TIMEOUT) {
            kill(child, SIGKILL);
            return AUTH_TIMEOUT;
        }

        return WEXITSTATUS(status);
    }
}
```

### Sandbox Policy Configuration

sql

```sql
-- Policy settings for UDR sandboxing
auth.udr.default_sandbox_level = 'RESTRICTED'
auth.udr.sandbox_memory_limit_mb = 128
auth.udr.sandbox_cpu_limit_ms = 5000
auth.udr.sandbox_max_threads = 4

-- Per-provider override (in sys.auth_providers)
UPDATE sys.auth_providers 
SET sandbox_level = 'NONE',  -- trusted internal UDR
    timeout_ms = 10000       -- allow longer for slow LDAP
WHERE provider_name = 'corporate_ldap';
```

## Provider Registration Workflow

### Registration DDL

sql

```sql
-- Register a new authentication provider
CREATE AUTHENTICATION PROVIDER kerberos_auth
    TYPE UDR
    LIBRARY '/usr/lib/mydb/auth_kerberos.so'
    ENTRY_POINT 'auth_provider_manifest'
    CHECKSUM 'sha256:a3f2b8c9d1e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0'
    SANDBOX RESTRICTED
    TIMEOUT 5000
    CONFIG (
        realm = 'EXAMPLE.COM',
        kdc_host = 'kdc.example.com',
        service_principal = 'mydb/dbserver.example.com',
        keytab_path = '/etc/mydb/mydb.keytab'
    );

-- Requires CLUSTER_ADMIN or AUTHENTICATION_ADMIN privilege
-- Provider starts in PENDING_VERIFICATION status
```

### Verification Process
```
Provider registration:
    │
    ├── DDL parsed, metadata stored
    │
    ├── Status = PENDING_VERIFICATION
    │
    ├── Verification checks:
    │       │
    │       ├── File exists at library_path?
    │       │       └── No → registration fails
    │       │
    │       ├── Compute file checksum
    │       │       ├── Matches provided checksum?
    │       │       │       └── No → registration fails
    │       │       │
    │       │       └── Store checksum_verified_at = now()
    │       │
    │       ├── If signature provided:
    │       │       ├── Verify signature against trusted keyring
    │       │       └── No match → registration fails or warn
    │       │
    │       ├── Load library in isolated process
    │       │       ├── Find entry point symbol?
    │       │       │       └── No → registration fails
    │       │       │
    │       │       ├── Validate manifest structure
    │       │       │
    │       │       └── Call validate_config if present
    │       │               └── Returns error → registration fails
    │       │
    │       └── All checks pass
    │
    ├── Status = ACTIVE
    │
    └── Audit log: provider registered
```

### Checksum Verification Daemon
```
function checksum_verification_daemon():
    interval = policy.get('auth.udr.checksum_verify_interval_seconds')

    while running:
        sleep(interval)

        providers = SELECT * FROM sys.auth_providers
                   WHERE provider_type = 'UDR' 
                   AND status = 'ACTIVE'

        for provider in providers:
            current_checksum = compute_file_checksum(provider.library_path)

            if current_checksum != provider.library_checksum:
                -- Library has changed!
                log_security('UDR_CHECKSUM_MISMATCH', provider.provider_name)

                UPDATE sys.auth_providers
                SET status = 'DISABLED',
                    last_modified_at = now()
                WHERE provider_id = provider.provider_id

                alert_security_admins('Auth provider library modified: ' + 
                                       provider.provider_name)

                -- Optionally: invalidate all sessions using this provider
                if policy.get('auth.udr.invalidate_sessions_on_checksum_fail'):
                    invalidate_sessions_by_provider(provider.provider_id)
```

## Authentication Flow

### Complete Authentication Sequence
```
Client connection with credentials:
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 1. PARSE AUTHENTICATION REQUEST                                  │
├─────────────────────────────────────────────────────────────────┤
│  - Extract username from wire protocol                          │
│  - Extract credential type and data                             │
│  - Record client connection metadata                            │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 2. USER LOOKUP                                                   │
├─────────────────────────────────────────────────────────────────┤
│  - Query credential cache for username                          │
│  - If not cached, query security DB                             │
│  - If user not found:                                           │
│      - Policy: auth.allow_unknown_user_to_provider              │
│      - If TRUE: continue (provider may auto-create)             │
│      - If FALSE: AUTH_FAILED, stop                              │
│  - Retrieve user_uuid, status, allowed auth methods             │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 3. USER STATUS CHECK                                             │
├─────────────────────────────────────────────────────────────────┤
│  - If BLOCKED/SUSPENDED/DELETED: AUTH_FAILED, stop              │
│  - If ACTIVE: continue                                          │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 4. SELECT AUTHENTICATION PROVIDER                                │
├─────────────────────────────────────────────────────────────────┤
│  - Get user's allowed auth methods from user_auth_methods       │
│  - Filter by: method supports client's credential type          │
│  - Filter by: method is ACTIVE                                  │
│  - Filter by: method allows client's source address             │
│  - Filter by: current time in method's allowed_times            │
│  - Order by: priority                                           │
│  - If no valid methods: AUTH_FAILED, stop                       │
│  - Select first matching provider                               │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 5. INVOKE PROVIDER                                               │
├─────────────────────────────────────────────────────────────────┤
│  - Build AuthRequest structure                                  │
│  - Retrieve provider configuration                              │
│  - Apply sandbox                                                │
│  - Set timeout timer                                            │
│  - Call provider's authenticate function                        │
│  - Collect AuthResponse                                         │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 6. PROCESS RESULT                                                │
├─────────────────────────────────────────────────────────────────┤
│  switch (response->result_code):                                │
│                                                                 │
│    AUTH_SUCCESS:                                                │
│      - Verify authenticated_user_uuid matches expected          │
│      - Create session                                           │
│      - Process external_groups for role mapping                 │
│      - Update last_success_at, reset failure_count              │
│      - Return success to client                                 │
│                                                                 │
│    AUTH_CONTINUE:                                               │
│      - Store conversation_state                                 │
│      - Send challenge to client                                 │
│      - Wait for response, goto step 5                           │
│                                                                 │
│    AUTH_FAILED:                                                 │
│      - Increment failure_count                                  │
│      - Check lockout threshold                                  │
│      - Try next provider (if policy allows)                     │
│      - Or return failure to client                              │
│                                                                 │
│    AUTH_PROVIDER_ERROR / AUTH_TIMEOUT:                          │
│      - Log error                                                │
│      - Try next provider (if available)                         │
│      - Or return failure to client                              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 7. AUDIT LOG                                                     │
├─────────────────────────────────────────────────────────────────┤
│  - Log authentication attempt                                   │
│  - Include: username, user_uuid, provider, result,              │
│             client_address, timestamp, session_id (if success)  │
└─────────────────────────────────────────────────────────────────┘
```

### Multi-Step Authentication Handling
```
First step:
    │
    ├── Client sends: username + initial credentials
    │
    ├── Provider returns: AUTH_CONTINUE + challenge + state
    │
    ├── Engine stores state in temporary table:
    │       INSERT INTO sys.auth_conversations (
    │           conversation_id = new_uuid(),
    │           user_uuid = ...,
    │           provider_id = ...,
    │           step_number = 1,
    │           state_data = response.conversation_state,
    │           challenge_sent = response.challenge,
    │           started_at = now(),
    │           expires_at = now() + policy.auth.conversation_timeout_seconds
    │       )
    │
    └── Send challenge to client (protocol-specific encoding)

Subsequent steps:
    │
    ├── Client sends: conversation_id + response
    │
    ├── Engine retrieves conversation state
    │       ├── Not found or expired → AUTH_FAILED
    │       └── Found → continue
    │
    ├── Build AuthRequest with:
    │       ├── conversation_id
    │       ├── step_number = previous + 1
    │       ├── conversation_state from stored state
    │       └── credentials = client's response
    │
    ├── Invoke provider
    │
    ├── Process result:
    │       ├── AUTH_CONTINUE → store new state, send new challenge
    │       ├── AUTH_SUCCESS → delete conversation, create session
    │       └── AUTH_FAILED → delete conversation, return failure
    │
    └── Max steps check: policy.auth.max_conversation_steps (default 10)
```

### Provider Fallback Logic
```
function authenticate_with_fallback(user, credentials):
    providers = get_valid_providers_for_user(user, credentials.type)

    if providers.empty():
        return AUTH_FAILED, "No valid authentication method"

    last_error = null

    for provider in providers:
        result = invoke_provider(provider, user, credentials)

        switch result.code:
            case AUTH_SUCCESS:
                return result  -- done

            case AUTH_FAILED:
                -- Credentials invalid for this provider
                if policy.get('auth.fallback_on_credential_failure'):
                    last_error = result
                    continue  -- try next
                else:
                    return result  -- stop on first credential failure

            case AUTH_PROVIDER_ERROR:
            case AUTH_TIMEOUT:
                -- Provider had an error, not credential issue
                log_warning("Provider failed: " + provider.name)
                last_error = result
                continue  -- try next provider

            case AUTH_USER_NOT_FOUND:
                -- This provider doesn't know this user
                if policy.get('auth.fallback_on_user_not_found'):
                    continue
                else:
                    last_error = result
                    -- Don't continue; user should exist in specified provider

    -- All providers exhausted
    return AUTH_FAILED, "Authentication failed: " + last_error.message
```

## External Group Mapping

### Group-to-Role Mapping Schema

sql

```sql
CREATE TABLE sys.external_group_mapping (
    mapping_id              UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    provider_id             UUID NOT NULL REFERENCES sys.auth_providers(provider_id),

    -- External group identifier (from provider)
    external_group          VARCHAR(256) NOT NULL,
    external_group_pattern  VARCHAR(256),  -- regex, if pattern matching
    match_type              ENUM('EXACT', 'PATTERN', 'PREFIX') DEFAULT 'EXACT',

    -- Internal role to grant
    role_uuid               UUID NOT NULL,

    -- Mapping behavior
    auto_grant              BOOLEAN DEFAULT TRUE,   -- add role on login
    auto_revoke             BOOLEAN DEFAULT FALSE,  -- remove if group missing

    -- Status
    status                  ENUM('ACTIVE', 'DISABLED') DEFAULT 'ACTIVE',

    UNIQUE (provider_id, external_group, role_uuid),
    INDEX idx_provider (provider_id),
    INDEX idx_role (role_uuid)
);
```

### Group Synchronization Logic
```
function sync_user_groups(user_uuid, provider_id, external_groups):
    mappings = SELECT * FROM sys.external_group_mapping
               WHERE provider_id = provider_id
               AND status = 'ACTIVE'

    granted_roles = []

    for group in external_groups:
        for mapping in mappings:
            if matches(group, mapping):
                if mapping.auto_grant:
                    grant_role_if_not_exists(user_uuid, mapping.role_uuid)
                    granted_roles.append(mapping.role_uuid)

    if policy.get('auth.auto_revoke_unmapped_groups'):
        -- Find roles from this provider that user no longer has
        current_roles = SELECT role_uuid FROM sys.role_cache
                       WHERE user_uuid = user_uuid
                       AND via_external_mapping = TRUE
                       AND mapping_provider = provider_id

        for role in current_roles:
            if role not in granted_roles:
                mapping = get_mapping_for_role(provider_id, role)
                if mapping.auto_revoke:
                    revoke_role(user_uuid, role)
                    log_info("Auto-revoked role due to missing external group")

function matches(group, mapping):
    switch mapping.match_type:
        case 'EXACT':
            return group == mapping.external_group
        case 'PREFIX':
            return group.starts_with(mapping.external_group)
        case 'PATTERN':
            return regex_match(group, mapping.external_group_pattern)
```

## Built-in Providers

### LOCAL_PASSWORD Provider

c

```c
/*
 * Built-in password authentication.
 * Always available, no UDR required.
 */

int builtin_password_auth(
    const AuthRequest*      request,
    const AuthConfig*       config,
    AuthResponse**          response,
    EngineContext*          ctx
) {
    *response = ctx->engine_alloc(sizeof(AuthResponse));

    /* Get stored hash from credential cache or security DB */
    StoredCredential* stored = get_stored_credential(request->user_uuid);

    if (!stored) {
        (*response)->result_code = AUTH_USER_NOT_FOUND;
        return 0;
    }

    /* Verify password against stored hash */
    bool valid = verify_password_hash(
        request->credentials.password.password,
        stored->password_hash,
        stored->hash_algorithm,
        stored->salt
    );

    /* Secure clear the password from memory */
    ctx->secure_free(request->credentials.password.password,
                     request->credentials.password.password_length);

    if (valid) {
        (*response)->result_code = AUTH_SUCCESS;
        (*response)->authenticated_user_uuid = request->user_uuid;
        (*response)->authenticated_name = ctx->engine_strdup(request->username);
    } else {
        (*response)->result_code = AUTH_FAILED;
        (*response)->result_message = ctx->engine_strdup("Invalid password");
    }

    return 0;
}
```

### TRUST Provider (for embedded/testing)

c

```c
/*
 * Trust authentication - accepts any connection.
 * Only enabled when auth.mode = NONE or explicitly configured.
 */

int builtin_trust_auth(
    const AuthRequest*      request,
    const AuthConfig*       config,
    AuthResponse**          response,
    EngineContext*          ctx
) {
    *response = ctx->engine_alloc(sizeof(AuthResponse));

    /* Check if trust is allowed */
    if (!ctx->get_config_bool("allow_trust_auth", false)) {
        (*response)->result_code = AUTH_CONFIG_ERROR;
        (*response)->result_message = ctx->engine_strdup(
            "Trust authentication not enabled");
        return 0;
    }

    /* Check IP restrictions if configured */
    const char* allowed_ips = ctx->get_config("trust_allowed_ips");
    if (allowed_ips && !ip_matches(request->client_address, allowed_ips)) {
        (*response)->result_code = AUTH_FAILED;
        (*response)->result_message = ctx->engine_strdup(
            "Trust auth not allowed from this address");
        return 0;
    }

    (*response)->result_code = AUTH_SUCCESS;
    (*response)->authenticated_user_uuid = request->user_uuid;
    (*response)->authenticated_name = ctx->engine_strdup(request->username);

    ctx->log_security("TRUST_AUTH", request->username);

    return 0;
}
```

## Audit Logging

### Authentication Event Schema

sql

```sql
CREATE TABLE sys.auth_audit_log (
    event_id                UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    event_timestamp         TIMESTAMP NOT NULL DEFAULT now(),

    -- Event type
    event_type              ENUM(
                                'AUTH_ATTEMPT',
                                'AUTH_SUCCESS', 
                                'AUTH_FAILURE',
                                'AUTH_LOCKOUT',
                                'AUTH_UNLOCK',
                                'SESSION_CREATE',
                                'SESSION_END',
                                'PROVIDER_ERROR',
                                'PASSWORD_CHANGE',
                                'PRIVILEGE_ESCALATION'
                            ) NOT NULL,

    -- User information
    username                VARCHAR(128),
    user_uuid               UUID,

    -- Provider information
    provider_id             UUID,
    provider_name           VARCHAR(64),
    auth_method             VARCHAR(32),

    -- Result
    result_code             INTEGER,
    result_message          TEXT,

    -- Connection details
    client_address          INET,
    client_port             INTEGER,
    client_application      VARCHAR(128),
    server_address          INET,
    server_port             INTEGER,
    database_name           VARCHAR(128),

    -- Session (if applicable)
    session_id              UUID,

    -- Additional context
    details                 JSONB,

    -- Indexing for common queries
    INDEX idx_timestamp (event_timestamp),
    INDEX idx_user (user_uuid, event_timestamp),
    INDEX idx_type (event_type, event_timestamp),
    INDEX idx_client (client_address, event_timestamp),
    INDEX idx_failures (event_type, user_uuid) 
        WHERE event_type IN ('AUTH_FAILURE', 'AUTH_LOCKOUT')
);

-- Partition by time for manageability
-- Implementation depends on your partitioning strategy
```

### Audit Policy

sql

```sql
-- What to log
auth.audit.log_success = TRUE
auth.audit.log_failure = TRUE
auth.audit.log_provider_errors = TRUE
auth.audit.log_session_lifecycle = TRUE
auth.audit.log_privilege_changes = TRUE

-- Detail level
auth.audit.include_client_address = TRUE
auth.audit.include_application_name = TRUE
auth.audit.include_provider_details = TRUE

-- Retention
auth.audit.retention_days = 90
auth.audit.archive_before_delete = TRUE
auth.audit.archive_path = '/var/log/mydb/auth_audit/'

-- Real-time alerting
auth.audit.alert_on_lockout = TRUE
auth.audit.alert_on_privilege_escalation = TRUE
auth.audit.alert_threshold_failures_per_minute = 10
```

## Policy Summary

| Policy Key                                       | Type    | Default    | Description                        |
| ------------------------------------------------ | ------- | ---------- | ---------------------------------- |
| `auth.allow_unknown_user_to_provider`            | BOOLEAN | FALSE      | Let provider handle unknown users  |
| `auth.fallback_on_credential_failure`            | BOOLEAN | FALSE      | Try next provider on bad password  |
| `auth.fallback_on_user_not_found`                | BOOLEAN | TRUE       | Try next provider if user unknown  |
| `auth.conversation_timeout_seconds`              | INTEGER | 60         | Multi-step auth timeout            |
| `auth.max_conversation_steps`                    | INTEGER | 10         | Max steps in multi-step auth       |
| `auth.auto_revoke_unmapped_groups`               | BOOLEAN | FALSE      | Remove roles when groups disappear |
| `auth.udr.default_sandbox_level`                 | ENUM    | RESTRICTED | Default UDR sandbox                |
| `auth.udr.sandbox_memory_limit_mb`               | INTEGER | 128        | Memory limit for UDRs              |
| `auth.udr.sandbox_cpu_limit_ms`                  | INTEGER | 5000       | CPU time limit                     |
| `auth.udr.sandbox_max_threads`                   | INTEGER | 4          | Thread limit                       |
| `auth.udr.checksum_verify_interval_seconds`      | INTEGER | 3600       | Library checksum check             |
| `auth.udr.invalidate_sessions_on_checksum_fail`  | BOOLEAN | TRUE       | Kill sessions on tamper            |
| `auth.audit.log_success`                         | BOOLEAN | TRUE       | Log successful auth                |
| `auth.audit.log_failure`                         | BOOLEAN | TRUE       | Log failed auth                    |
| `auth.audit.retention_days`                      | INTEGER | 90         | Audit log retention                |
| `auth.audit.alert_threshold_failures_per_minute` | INTEGER | 10         | Brute force detection              |

Encryption and key management is where security architecture gets complicated. The challenge is balancing security, usability, and operational reality. Let me build out the complete specification:

## Key Hierarchy Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           KEY HIERARCHY                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    CLUSTER MASTER KEY (CMK)                          │   │
│  │         Held only by security database(s)                            │   │
│  │         Derived from: passphrase, HSM, or external KMS               │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                    ┌───────────────┼───────────────┐                       │
│                    v               v               v                        │
│  ┌──────────────────────┐ ┌──────────────────────┐ ┌──────────────────────┐│
│  │  Database Key (DBK)  │ │  Database Key (DBK)  │ │  Database Key (DBK)  ││
│  │    Database A        │ │    Database B        │ │    Database C        ││
│  │  Encrypted by CMK    │ │  Encrypted by CMK    │ │  Encrypted by CMK    ││
│  └──────────────────────┘ └──────────────────────┘ └──────────────────────┘│
│            │                       │                        │               │
│            v                       v                        v               │
│  ┌──────────────────────┐ ┌──────────────────────┐ ┌──────────────────────┐│
│  │  Tablespace Keys     │ │  Tablespace Keys     │ │  Tablespace Keys     ││
│  │  (TSK)               │ │  (TSK)               │ │  (TSK)               ││
│  │  Encrypted by DBK    │ │  Encrypted by DBK    │ │  Encrypted by DBK    ││
│  └──────────────────────┘ └──────────────────────┘ └──────────────────────┘│
│            │                                                                │
│            v                                                                │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                     Data Encryption Keys (DEK)                        │  │
│  │              Per-page or per-extent encryption                        │  │
│  │                    Encrypted by TSK                                   │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Key Types and Purposes

| Key Type             | Abbreviation | Scope              | Protected By            | Purpose                     |
| -------------------- | ------------ | ------------------ | ----------------------- | --------------------------- |
| Cluster Master Key   | CMK          | Entire cluster     | Passphrase/HSM/KMS      | Root of trust               |
| Security DB Key      | SDK          | Security database  | CMK                     | Encrypt security catalogs   |
| Database Key         | DBK          | Single database    | CMK                     | Wrap tablespace keys        |
| Tablespace Key       | TSK          | Tablespace         | DBK                     | Wrap data encryption keys   |
| Data Encryption Key  | DEK          | Page/extent        | TSK                     | Actual data encryption      |
| Backup Key           | BKK          | Backup set         | CMK + backup passphrase | Encrypt backups             |
| Credential Store Key | CSK          | DATABASES table    | SDK                     | Encrypt stored passwords    |
| Log Encryption Key   | LEK          | Transaction log    | DBK                     | Encrypt write-after log (WAL)/journal         |
| Replication Key      | RPK          | Replication stream | CMK                     | Encrypt replication traffic |

## Master Key Management

### Key Derivation Sources

sql

```sql
CREATE TYPE key_derivation_source AS ENUM (
    'PASSPHRASE',       -- PBKDF2/Argon2 from admin-provided passphrase
    'HSM',              -- Hardware Security Module
    'EXTERNAL_KMS',     -- AWS KMS, HashiCorp Vault, Azure Key Vault
    'TPM',              -- Trusted Platform Module (ties to hardware)
    'SPLIT_KNOWLEDGE',  -- Shamir's Secret Sharing
    'FILE'              -- Key file on disk (weakest, for dev/test)
);
```

### Master Key Schema

sql

```sql
CREATE TABLE sys.master_key_config (
    key_id                  UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    key_type                ENUM('CMK', 'SDK', 'DBK', 'TSK', 'BKK', 'CSK', 'LEK', 'RPK') NOT NULL,

    -- Key identification
    key_name                VARCHAR(128) NOT NULL,
    key_version             INTEGER NOT NULL DEFAULT 1,

    -- Derivation
    derivation_source       key_derivation_source NOT NULL,
    derivation_params       JSONB,  -- source-specific parameters

    -- The wrapped key (encrypted by parent key or external source)
    wrapped_key             BYTEA NOT NULL,
    key_algorithm           VARCHAR(32) NOT NULL,  -- 'AES-256-GCM', 'ChaCha20-Poly1305'
    key_length_bits         INTEGER NOT NULL,

    -- Key wrapping details
    wrapped_by_key_id       UUID REFERENCES sys.master_key_config(key_id),
    wrapping_algorithm      VARCHAR(32),  -- 'AES-256-KWP', 'RSA-OAEP'
    wrapping_iv             BYTEA,
    wrapping_auth_tag       BYTEA,

    -- Lifecycle
    status                  ENUM('ACTIVE', 'ROTATING', 'RETIRED', 'DESTROYED') NOT NULL,
    created_at              TIMESTAMP NOT NULL DEFAULT now(),
    activated_at            TIMESTAMP,
    rotation_started_at     TIMESTAMP,
    retired_at              TIMESTAMP,
    destroy_after           TIMESTAMP,  -- scheduled destruction

    -- Rotation policy
    rotation_period_days    INTEGER,
    last_rotated_at         TIMESTAMP,
    next_rotation_at        TIMESTAMP,

    -- Audit
    created_by              UUID,
    last_accessed_at        TIMESTAMP,
    access_count            BIGINT DEFAULT 0,

    UNIQUE (key_name, key_version),
    INDEX idx_type_status (key_type, status),
    INDEX idx_wrapped_by (wrapped_by_key_id)
);
```

### Passphrase-Based Key Derivation

c

```c
typedef struct PassphraseDerivationParams {
    char*           algorithm;          /* "ARGON2ID", "PBKDF2-SHA512", "SCRYPT" */

    /* Argon2 parameters */
    uint32_t        argon2_memory_kb;   /* memory cost */
    uint32_t        argon2_iterations;  /* time cost */
    uint32_t        argon2_parallelism; /* parallelism */

    /* PBKDF2 parameters */
    uint32_t        pbkdf2_iterations;

    /* Common */
    uint8_t*        salt;               /* random salt */
    size_t          salt_length;        /* 32 bytes recommended */

} PassphraseDerivationParams;

/*
 * Derive master key from passphrase.
 * 
 * The passphrase is provided at startup and never stored.
 * The derived key is held only in secure memory.
 */
int derive_master_key_from_passphrase(
    const char*                     passphrase,
    const PassphraseDerivationParams* params,
    uint8_t*                        derived_key,
    size_t                          key_length
) {
    switch (params->algorithm) {
        case ARGON2ID:
            return argon2id_hash_raw(
                params->argon2_iterations,
                params->argon2_memory_kb,
                params->argon2_parallelism,
                passphrase, strlen(passphrase),
                params->salt, params->salt_length,
                derived_key, key_length
            );

        case PBKDF2_SHA512:
            return PKCS5_PBKDF2_HMAC(
                passphrase, strlen(passphrase),
                params->salt, params->salt_length,
                params->pbkdf2_iterations,
                EVP_sha512(),
                key_length, derived_key
            );

        /* etc. */
    }
}
```

### HSM Integration

sql

```sql
-- HSM configuration in derivation_params
{
    "hsm_type": "PKCS11",           -- or "KMIP", vendor-specific
    "library_path": "/usr/lib/softhsm/libsofthsm2.so",
    "slot_id": 0,
    "key_label": "mydb-cmk",
    "pin_source": "ENV:MYDB_HSM_PIN",  -- or "FILE:/path/to/pin"
    "key_id": "0x0001"
}
```

c

```c
typedef struct HSMKeyHandle {
    void*           session;        /* PKCS#11 session or equivalent */
    uint64_t        key_handle;     /* handle to key in HSM */
    char*           key_label;
    KeyAlgorithm    algorithm;
} HSMKeyHandle;

/*
 * The CMK never leaves the HSM.
 * All operations happen inside the HSM.
 */
int hsm_wrap_key(
    HSMKeyHandle*       cmk_handle,
    const uint8_t*      key_to_wrap,
    size_t              key_length,
    uint8_t*            wrapped_key,
    size_t*             wrapped_length
) {
    /* Use HSM to wrap the key */
    return pkcs11_wrap_key(
        cmk_handle->session,
        cmk_handle->key_handle,
        CKM_AES_KEY_WRAP_PAD,
        key_to_wrap, key_length,
        wrapped_key, wrapped_length
    );
}

int hsm_unwrap_key(
    HSMKeyHandle*       cmk_handle,
    const uint8_t*      wrapped_key,
    size_t              wrapped_length,
    uint8_t*            unwrapped_key,
    size_t*             key_length
) {
    return pkcs11_unwrap_key(
        cmk_handle->session,
        cmk_handle->key_handle,
        CKM_AES_KEY_WRAP_PAD,
        wrapped_key, wrapped_length,
        unwrapped_key, key_length
    );
}
```

### External KMS Integration

sql

```sql
-- AWS KMS configuration
{
    "kms_type": "AWS_KMS",
    "region": "us-east-1",
    "key_id": "arn:aws:kms:us-east-1:123456789:key/abcd-1234-...",
    "auth_method": "IAM_ROLE",  -- or "ACCESS_KEY"
    "role_arn": "arn:aws:iam::123456789:role/mydb-kms-role"
}

-- HashiCorp Vault configuration
{
    "kms_type": "HASHICORP_VAULT",
    "vault_addr": "https://vault.example.com:8200",
    "auth_method": "APPROLE",
    "role_id": "mydb-role",
    "secret_id_source": "ENV:VAULT_SECRET_ID",
    "transit_key": "mydb-cmk",
    "mount_path": "transit"
}

-- Azure Key Vault configuration
{
    "kms_type": "AZURE_KEY_VAULT",
    "vault_url": "https://mydb-vault.vault.azure.net",
    "key_name": "mydb-cmk",
    "auth_method": "MANAGED_IDENTITY"
}
```

c

```c
/*
 * KMS provider interface.
 * Each KMS type implements this.
 */
typedef struct KMSProvider {
    /* Initialize connection to KMS */
    int (*init)(const char* config_json, void** ctx);

    /* Shutdown */
    void (*shutdown)(void* ctx);

    /* Generate a new data key, return both plaintext and encrypted */
    int (*generate_data_key)(
        void* ctx,
        size_t key_length,
        uint8_t* plaintext_key,
        uint8_t* encrypted_key,
        size_t* encrypted_length
    );

    /* Decrypt an encrypted data key */
    int (*decrypt_data_key)(
        void* ctx,
        const uint8_t* encrypted_key,
        size_t encrypted_length,
        uint8_t* plaintext_key,
        size_t* key_length
    );

    /* Encrypt data directly (for small values) */
    int (*encrypt)(
        void* ctx,
        const uint8_t* plaintext,
        size_t plaintext_length,
        uint8_t* ciphertext,
        size_t* ciphertext_length
    );

    /* Decrypt data directly */
    int (*decrypt)(
        void* ctx,
        const uint8_t* ciphertext,
        size_t ciphertext_length,
        uint8_t* plaintext,
        size_t* plaintext_length
    );

    /* Re-encrypt under a new key version (for rotation) */
    int (*reencrypt)(
        void* ctx,
        const uint8_t* ciphertext,
        size_t ciphertext_length,
        uint8_t* new_ciphertext,
        size_t* new_ciphertext_length
    );

} KMSProvider;
```

### Split Knowledge (Shamir's Secret Sharing)

sql

```sql
-- Split knowledge configuration
{
    "scheme": "SHAMIR",
    "total_shares": 5,
    "threshold": 3,
    "share_holders": [
        {"id": "admin1", "contact": "admin1@example.com"},
        {"id": "admin2", "contact": "admin2@example.com"},
        {"id": "admin3", "contact": "admin3@example.com"},
        {"id": "admin4", "contact": "admin4@example.com"},
        {"id": "admin5", "contact": "admin5@example.com"}
    ]
}
```

c

```c
/*
 * Generate shares of the master key.
 * Called once during cluster initialization.
 */
int generate_key_shares(
    const uint8_t*      master_key,
    size_t              key_length,
    int                 total_shares,
    int                 threshold,
    KeyShare**          shares
) {
    /* Use Shamir's Secret Sharing */
    return shamir_split(
        master_key, key_length,
        total_shares, threshold,
        shares
    );
}

/*
 * Reconstruct master key from shares.
 * Called at startup when split knowledge is used.
 */
int reconstruct_key_from_shares(
    const KeyShare**    shares,
    int                 share_count,
    int                 threshold,
    uint8_t*            master_key,
    size_t*             key_length
) {
    if (share_count < threshold) {
        return ERR_INSUFFICIENT_SHARES;
    }

    return shamir_combine(
        shares, share_count,
        master_key, key_length
    );
}

/*
 * Startup flow with split knowledge.
 */
typedef enum StartupState {
    STARTUP_WAITING_FOR_SHARES,
    STARTUP_SHARES_SUFFICIENT,
    STARTUP_KEY_RECONSTRUCTED,
    STARTUP_COMPLETE
} StartupState;

typedef struct ShareCollector {
    StartupState        state;
    int                 threshold;
    int                 total_shares;
    int                 collected_count;
    KeyShare*           collected_shares[MAX_SHARES];
    timestamp_t         collection_started;
    timestamp_t         collection_timeout;
} ShareCollector;

/*
 * Admin submits their share via secure channel.
 */
int submit_key_share(
    ShareCollector*     collector,
    const char*         admin_id,
    const KeyShare*     share
) {
    /* Verify admin is authorized share holder */
    if (!is_authorized_share_holder(admin_id)) {
        return ERR_UNAUTHORIZED;
    }

    /* Verify share not already submitted */
    if (share_already_submitted(collector, admin_id)) {
        return ERR_DUPLICATE_SHARE;
    }

    /* Store share in secure memory */
    collector->collected_shares[collector->collected_count++] = 
        secure_copy_share(share);

    log_security("KEY_SHARE_SUBMITTED", admin_id);

    if (collector->collected_count >= collector->threshold) {
        collector->state = STARTUP_SHARES_SUFFICIENT;
        return try_reconstruct_key(collector);
    }

    return OK;
}
```

## Database Encryption

### Encryption Modes

sql

```sql
CREATE TYPE encryption_mode AS ENUM (
    'NONE',             -- no encryption
    'TDE',              -- Transparent Data Encryption (page-level)
    'TABLESPACE',       -- per-tablespace encryption
    'TABLE',            -- per-table encryption
    'COLUMN'            -- per-column encryption (application-assisted)
);

CREATE TYPE encryption_algorithm AS ENUM (
    'AES-256-GCM',      -- authenticated encryption, recommended
    'AES-256-CBC',      -- legacy compatibility
    'AES-256-XTS',      -- for storage (disk sector encryption style)
    'CHACHA20-POLY1305' -- modern alternative to AES
);
```

### Database Encryption Configuration

sql

```sql
CREATE TABLE sys.database_encryption (
    database_uuid           UUID PRIMARY KEY,

    -- Encryption settings
    encryption_mode         encryption_mode NOT NULL DEFAULT 'NONE',
    encryption_algorithm    encryption_algorithm,

    -- Key reference
    database_key_id         UUID REFERENCES sys.master_key_config(key_id),

    -- Status
    encryption_status       ENUM('UNENCRYPTED', 'ENCRYPTING', 'ENCRYPTED', 
                                 'DECRYPTING', 'ROTATING', 'ERROR') NOT NULL,
    status_detail           TEXT,

    -- Progress tracking (for in-progress operations)
    total_pages             BIGINT,
    processed_pages         BIGINT,
    started_at              TIMESTAMP,
    estimated_completion    TIMESTAMP,

    -- Configuration
    encrypt_temp_tables     BOOLEAN DEFAULT TRUE,
    encrypt_system_tables   BOOLEAN DEFAULT TRUE,
    encrypt_wal             BOOLEAN DEFAULT TRUE,

    -- Audit
    encrypted_at            TIMESTAMP,
    encrypted_by            UUID,
    last_key_rotation       TIMESTAMP
);
```

### Tablespace Encryption

sql

```sql
CREATE TABLE sys.tablespace_encryption (
    tablespace_uuid         UUID PRIMARY KEY,
    database_uuid           UUID NOT NULL,

    -- Encryption settings (can override database defaults)
    encryption_enabled      BOOLEAN NOT NULL DEFAULT TRUE,
    encryption_algorithm    encryption_algorithm,

    -- Key reference
    tablespace_key_id       UUID REFERENCES sys.master_key_config(key_id),

    -- Status
    encryption_status       ENUM('UNENCRYPTED', 'ENCRYPTING', 'ENCRYPTED',
                                 'ROTATING', 'ERROR') NOT NULL,

    FOREIGN KEY (database_uuid) REFERENCES sys.database_encryption(database_uuid)
);
```

### Page-Level Encryption (TDE)

c

```c
/*
 * Page structure with encryption.
 * 
 * Each page has its own IV derived from page number.
 * Authentication tag ensures integrity.
 */
typedef struct EncryptedPageHeader {
    uint32_t        page_number;
    uint16_t        page_type;
    uint16_t        flags;

    /* Encryption metadata */
    uint8_t         key_version;        /* which version of TSK */
    uint8_t         algorithm;          /* encryption algorithm */
    uint8_t         iv[12];             /* initialization vector */
    uint8_t         auth_tag[16];       /* GCM authentication tag */

    /* The rest of the header is encrypted along with data */

} EncryptedPageHeader;

/*
 * Encrypt a page before writing to disk.
 */
int encrypt_page(
    const uint8_t*      plaintext_page,
    size_t              page_size,
    uint32_t            page_number,
    const TablespaceKey* tsk,
    uint8_t*            encrypted_page
) {
    EncryptedPageHeader* header = (EncryptedPageHeader*)encrypted_page;

    /* Generate IV: page number + random nonce */
    /* Using page number in IV prevents IV reuse across pages */
    generate_page_iv(page_number, tsk->key_version, header->iv);

    /* Encrypt page content (after header) */
    size_t plaintext_offset = sizeof(EncryptedPageHeader);
    size_t plaintext_len = page_size - plaintext_offset;

    int result = aes_256_gcm_encrypt(
        tsk->key_material,
        header->iv, sizeof(header->iv),
        /* Additional authenticated data: page number, type, flags */
        (uint8_t*)header, offsetof(EncryptedPageHeader, iv),
        plaintext_page + plaintext_offset, plaintext_len,
        encrypted_page + plaintext_offset,
        header->auth_tag
    );

    header->page_number = page_number;
    header->key_version = tsk->version;
    header->algorithm = ALG_AES_256_GCM;

    return result;
}

/*
 * Decrypt a page after reading from disk.
 */
int decrypt_page(
    const uint8_t*      encrypted_page,
    size_t              page_size,
    const TablespaceKey* tsk,
    uint8_t*            plaintext_page
) {
    const EncryptedPageHeader* header = (const EncryptedPageHeader*)encrypted_page;

    /* Verify key version matches */
    if (header->key_version != tsk->version) {
        /* May need to fetch old key version for rotation-in-progress */
        tsk = get_key_version(tsk->key_id, header->key_version);
        if (!tsk) {
            return ERR_KEY_VERSION_NOT_FOUND;
        }
    }

    size_t ciphertext_offset = sizeof(EncryptedPageHeader);
    size_t ciphertext_len = page_size - ciphertext_offset;

    int result = aes_256_gcm_decrypt(
        tsk->key_material,
        header->iv, sizeof(header->iv),
        (uint8_t*)header, offsetof(EncryptedPageHeader, iv),
        encrypted_page + ciphertext_offset, ciphertext_len,
        header->auth_tag,
        plaintext_page + ciphertext_offset
    );

    if (result != OK) {
        log_security("PAGE_DECRYPTION_FAILED", 
                     "page=%u, possible tampering", header->page_number);
        return ERR_DECRYPTION_FAILED;
    }

    /* Copy unencrypted header portion */
    memcpy(plaintext_page, encrypted_page, ciphertext_offset);

    return OK;
}
```

### Write-after Log (WAL)/Transaction Log Encryption

c

```c
/*
 * Write-after log (WAL) records are encrypted individually.
 * Each record has its own IV based on LSN.
 */
typedef struct EncryptedWALRecord {
    uint64_t        lsn;                /* Log Sequence Number */
    uint32_t        record_length;
    uint16_t        record_type;
    uint8_t         key_version;
    uint8_t         flags;
    uint8_t         iv[12];
    uint8_t         auth_tag[16];
    /* encrypted payload follows */
} EncryptedWALRecord;

int encrypt_wal_record(
    const uint8_t*      record_data,
    size_t              record_length,
    uint64_t            lsn,
    const LogEncryptionKey* lek,
    uint8_t*            encrypted_record,
    size_t*             encrypted_length
) {
    EncryptedWALRecord* header = (EncryptedWALRecord*)encrypted_record;

    /* IV derived from LSN ensures uniqueness */
    derive_wal_iv(lsn, lek->key_version, header->iv);

    header->lsn = lsn;
    header->record_length = record_length;
    header->key_version = lek->version;

    size_t header_size = sizeof(EncryptedWALRecord);

    int result = aes_256_gcm_encrypt(
        lek->key_material,
        header->iv, sizeof(header->iv),
        (uint8_t*)header, offsetof(EncryptedWALRecord, iv),
        record_data, record_length,
        encrypted_record + header_size,
        header->auth_tag
    );

    *encrypted_length = header_size + record_length + 16; /* +16 for GCM expansion */

    return result;
}
```

## Key Rotation

### Rotation Workflow
```
Key rotation request:
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 1. GENERATE NEW KEY VERSION                                      │
├─────────────────────────────────────────────────────────────────┤
│  - Generate new random key material                             │
│  - Wrap with parent key (CMK wraps DBK, DBK wraps TSK, etc.)    │
│  - Store as new version, status = 'PENDING'                     │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 2. ACTIVATE NEW VERSION                                          │
├─────────────────────────────────────────────────────────────────┤
│  - New writes use new key version                               │
│  - Old version still valid for reads                            │
│  - Update key status: old = 'ROTATING', new = 'ACTIVE'          │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 3. RE-ENCRYPT DATA (background process)                          │
├─────────────────────────────────────────────────────────────────┤
│  - For each page/extent encrypted with old version:             │
│      - Decrypt with old key                                     │
│      - Re-encrypt with new key                                  │
│      - Write back                                               │
│  - Track progress in sys.database_encryption                    │
│  - Throttle to avoid I/O saturation                             │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 4. RETIRE OLD VERSION                                            │
├─────────────────────────────────────────────────────────────────┤
│  - Verify no data uses old version                              │
│  - Update old key status = 'RETIRED'                            │
│  - Keep old key for backup decryption (configurable period)     │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 5. DESTROY OLD VERSION (optional, after retention period)       │
├─────────────────────────────────────────────────────────────────┤
│  - Securely erase old key material                              │
│  - Update status = 'DESTROYED'                                  │
│  - Log destruction for audit                                    │
└─────────────────────────────────────────────────────────────────┘
```

### Rotation State Machine

sql

```sql
CREATE TABLE sys.key_rotation_state (
    rotation_id             UUID PRIMARY KEY DEFAULT gen_uuid_v7(),

    -- Keys involved
    old_key_id              UUID NOT NULL REFERENCES sys.master_key_config(key_id),
    new_key_id              UUID NOT NULL REFERENCES sys.master_key_config(key_id),

    -- Scope
    key_type                ENUM('CMK', 'DBK', 'TSK', 'LEK') NOT NULL,
    scope_uuid              UUID,  -- database_uuid or tablespace_uuid

    -- State
    state                   ENUM('INITIATED', 'NEW_KEY_ACTIVE', 'REENCRYPTING',
                                 'VERIFYING', 'COMPLETING', 'COMPLETED', 
                                 'FAILED', 'ROLLED_BACK') NOT NULL,

    -- Progress
    total_items             BIGINT,  -- pages, records, child keys
    processed_items         BIGINT,
    failed_items            BIGINT,

    -- Timing
    initiated_at            TIMESTAMP NOT NULL DEFAULT now(),
    new_key_activated_at    TIMESTAMP,
    reencryption_started_at TIMESTAMP,
    reencryption_completed_at TIMESTAMP,
    verified_at             TIMESTAMP,
    completed_at            TIMESTAMP,

    -- Error handling
    last_error              TEXT,
    retry_count             INTEGER DEFAULT 0,

    -- Throttling
    max_iops                INTEGER,  -- limit I/O during reencryption
    max_cpu_percent         INTEGER,

    -- Audit
    initiated_by            UUID,

    INDEX idx_state (state),
    INDEX idx_scope (key_type, scope_uuid)
);
```

### Background Re-encryption Process

c

```c
/*
 * Background thread that handles key rotation re-encryption.
 */
void key_rotation_worker(void* ctx) {
    while (running) {
        /* Find active rotations that need work */
        RotationState* rotation = get_next_pending_rotation();

        if (!rotation) {
            sleep_ms(policy.rotation_check_interval_ms);
            continue;
        }

        switch (rotation->state) {
            case NEW_KEY_ACTIVE:
                start_reencryption(rotation);
                break;

            case REENCRYPTING:
                process_reencryption_batch(rotation);
                break;

            case VERIFYING:
                verify_rotation_complete(rotation);
                break;

            case COMPLETING:
                finalize_rotation(rotation);
                break;
        }
    }
}

void process_reencryption_batch(RotationState* rotation) {
    /* Throttle based on policy */
    wait_for_io_budget(rotation->max_iops);

    /* Get batch of items to reencrypt */
    PageBatch* batch = get_pages_with_old_key_version(
        rotation->scope_uuid,
        rotation->old_key_id,
        policy.rotation_batch_size
    );

    if (batch->count == 0) {
        /* No more pages with old key */
        transition_state(rotation, VERIFYING);
        return;
    }

    for (int i = 0; i < batch->count; i++) {
        Page* page = batch->pages[i];

        /* Acquire page lock */
        lock_page_exclusive(page);

        /* Re-check key version (may have been updated concurrently) */
        if (page->key_version == rotation->old_key_version) {
            /* Decrypt with old key */
            uint8_t* plaintext = secure_alloc(page->size);
            decrypt_page(page->data, page->size, rotation->old_key, plaintext);

            /* Encrypt with new key */
            encrypt_page(plaintext, page->size, page->number, 
                        rotation->new_key, page->data);

            /* Mark page dirty */
            mark_page_dirty(page);

            secure_free(plaintext, page->size);
        }

        unlock_page(page);

        rotation->processed_items++;
    }

    /* Update progress */
    update_rotation_progress(rotation);

    /* Checkpoint periodically */
    if (should_checkpoint(rotation)) {
        checkpoint_rotation_state(rotation);
    }
}
```

### Rotation DDL

sql

```sql
-- Initiate rotation
ALTER DATABASE mydb ROTATE ENCRYPTION KEY
    [MAX_IOPS 1000]
    [MAX_CPU_PERCENT 25]
    [WAIT | NOWAIT];

-- Check rotation status
SELECT * FROM sys.key_rotation_status
WHERE database_uuid = (SELECT uuid FROM sys.databases WHERE name = 'mydb');

-- Cancel/rollback rotation (if possible)
ALTER DATABASE mydb CANCEL KEY ROTATION;

-- Force completion (skip remaining pages - dangerous)
ALTER DATABASE mydb COMPLETE KEY ROTATION FORCE;
```

## Backup Encryption

### Backup Key Generation

sql

```sql
CREATE TABLE sys.backup_keys (
    backup_id               UUID PRIMARY KEY,
    backup_name             VARCHAR(256),
    backup_type             ENUM('FULL', 'INCREMENTAL', 'DIFFERENTIAL', 'WAL') NOT NULL,

    -- Scope
    database_uuid           UUID,  -- NULL for cluster-level backup

    -- Encryption
    backup_key_id           UUID REFERENCES sys.master_key_config(key_id),
    encryption_algorithm    encryption_algorithm NOT NULL,

    -- Key derivation (backup-specific passphrase + CMK)
    passphrase_required     BOOLEAN DEFAULT FALSE,
    passphrase_salt         BYTEA,
    passphrase_params       JSONB,  -- Argon2 parameters

    -- Metadata
    created_at              TIMESTAMP NOT NULL DEFAULT now(),
    created_by              UUID,
    expires_at              TIMESTAMP,

    -- Verification
    key_verification_hash   BYTEA,  -- to verify correct passphrase on restore

    INDEX idx_database (database_uuid),
    INDEX idx_created (created_at)
);
```

### Backup Encryption Flow
```
Backup creation:
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 1. GENERATE BACKUP KEY                                           │
├─────────────────────────────────────────────────────────────────┤
│  Option A: CMK-only protection                                  │
│    - Generate random backup key                                 │
│    - Wrap with CMK                                              │
│    - Store wrapped key in backup header                         │
│                                                                 │
│  Option B: CMK + passphrase (recommended for offsite)           │
│    - Generate random backup key                                 │
│    - Derive passphrase key from admin-provided passphrase       │
│    - XOR/combine backup key with passphrase key                 │
│    - Wrap result with CMK                                       │
│    - Store wrapped key in backup header                         │
│    - Restore requires both CMK and passphrase                   │
│                                                                 │
│  Option C: Passphrase-only (for portable backups)               │
│    - Derive key entirely from passphrase                        │
│    - No CMK dependency                                          │
│    - Can restore without access to cluster                      │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 2. ENCRYPT BACKUP STREAM                                         │
├─────────────────────────────────────────────────────────────────┤
│  - Encrypt each backup block/chunk with backup key              │
│  - Include IV per chunk                                         │
│  - Include authentication tag for integrity                     │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 3. WRITE BACKUP HEADER                                           │
├─────────────────────────────────────────────────────────────────┤
│  - Backup metadata (unencrypted)                                │
│  - Wrapped backup key                                           │
│  - Salt and derivation params (if passphrase used)              │
│  - Key verification hash                                        │
│  - Signature (optional)                                         │
└─────────────────────────────────────────────────────────────────┘
```

### Backup File Header Structure

c

```c
typedef struct BackupHeader {
    /* Magic and version */
    uint8_t         magic[8];           /* "MYDB_BKP" */
    uint32_t        header_version;
    uint32_t        header_size;

    /* Backup identification */
    uuid_t          backup_id;
    uint32_t        backup_type;
    timestamp_t     backup_timestamp;
    char            backup_name[256];

    /* Source information */
    uuid_t          cluster_uuid;
    uuid_t          database_uuid;
    uint64_t        start_lsn;
    uint64_t        end_lsn;

    /* Encryption metadata */
    uint8_t         encryption_algorithm;
    uint8_t         key_derivation_mode;    /* CMK_ONLY, CMK_PLUS_PASSPHRASE, PASSPHRASE_ONLY */
    uint8_t         wrapped_key[256];
    uint16_t        wrapped_key_length;

    /* Passphrase derivation (if used) */
    uint8_t         passphrase_salt[32];
    uint8_t         argon2_memory_cost;     /* log2 of KB */
    uint8_t         argon2_time_cost;
    uint8_t         argon2_parallelism;

    /* Key verification */
    uint8_t         key_verification_hash[32];

    /* Integrity */
    uint8_t         header_hmac[32];        /* HMAC of header (excluding this field) */

    /* Optional signature */
    uint16_t        signature_length;
    uint8_t         signature[512];         /* RSA/ECDSA signature */

} BackupHeader;
```

### Restore with Passphrase

c

```c
int restore_backup_with_passphrase(
    const char*         backup_path,
    const char*         passphrase,        /* may be NULL if CMK-only */
    const CMKHandle*    cmk,               /* may be NULL if passphrase-only */
    RestoreContext*     ctx
) {
    /* Read and parse header */
    BackupHeader header;
    read_backup_header(backup_path, &header);

    /* Verify header integrity */
    if (!verify_header_hmac(&header)) {
        return ERR_BACKUP_CORRUPTED;
    }

    /* Derive backup key based on mode */
    uint8_t backup_key[32];

    switch (header.key_derivation_mode) {
        case CMK_ONLY:
            if (!cmk) return ERR_CMK_REQUIRED;
            unwrap_key_with_cmk(cmk, header.wrapped_key, 
                               header.wrapped_key_length, backup_key);
            break;

        case CMK_PLUS_PASSPHRASE:
            if (!cmk || !passphrase) 
                return ERR_CMK_AND_PASSPHRASE_REQUIRED;

            /* Derive passphrase component */
            uint8_t passphrase_key[32];
            derive_from_passphrase(passphrase, header.passphrase_salt,
                                   &header.argon2_params, passphrase_key);

            /* Unwrap with CMK */
            uint8_t wrapped_component[32];
            unwrap_key_with_cmk(cmk, header.wrapped_key,
                               header.wrapped_key_length, wrapped_component);

            /* Combine */
            xor_keys(wrapped_component, passphrase_key, backup_key);

            secure_zero(passphrase_key, 32);
            secure_zero(wrapped_component, 32);
            break;

        case PASSPHRASE_ONLY:
            if (!passphrase) return ERR_PASSPHRASE_REQUIRED;
            derive_from_passphrase(passphrase, header.passphrase_salt,
                                   &header.argon2_params, backup_key);
            break;
    }

    /* Verify we got the right key */
    uint8_t verification[32];
    compute_key_verification(backup_key, verification);
    if (memcmp(verification, header.key_verification_hash, 32) != 0) {
        secure_zero(backup_key, 32);
        return ERR_INVALID_PASSPHRASE_OR_CMK;
    }

    /* Proceed with restore using backup_key */
    ctx->backup_key = secure_copy(backup_key, 32);
    secure_zero(backup_key, 32);

    return OK;
}
```

## Credential Store Encryption

### DATABASES Table Password Protection

sql

```sql
-- The DATABASES table stores connection credentials to other databases.
-- These passwords must be encrypted at rest.

CREATE TABLE sys.databases_credentials (
    database_entry_uuid     UUID PRIMARY KEY,

    -- Connection parameters (not sensitive)
    hostname                VARCHAR(256),
    port                    INTEGER,
    database_name           VARCHAR(128),
    protocol                ENUM('POSTGRES', 'MYSQL', 'MSSQL', 'FIREBIRD'),

    -- Encrypted credentials
    username_encrypted      BYTEA,
    password_encrypted      BYTEA,
    encryption_key_id       UUID REFERENCES sys.master_key_config(key_id),
    encryption_iv           BYTEA,

    -- Certificate-based auth (alternative to password)
    client_cert_encrypted   BYTEA,
    client_key_encrypted    BYTEA,

    -- Metadata
    created_at              TIMESTAMP,
    last_used_at            TIMESTAMP,
    created_by              UUID
);
```

### Credential Encryption Functions

c

```c
/*
 * Encrypt a credential for storage.
 * Uses the Credential Store Key (CSK).
 */
int encrypt_credential(
    const char*             plaintext_credential,
    const CredentialStoreKey* csk,
    EncryptedCredential*    result
) {
    /* Generate random IV */
    generate_random_bytes(result->iv, 12);

    size_t plaintext_len = strlen(plaintext_credential);
    result->ciphertext = secure_alloc(plaintext_len + 16);  /* +16 for GCM */

    int rc = aes_256_gcm_encrypt(
        csk->key_material,
        result->iv, 12,
        NULL, 0,  /* no AAD */
        (uint8_t*)plaintext_credential, plaintext_len,
        result->ciphertext,
        result->auth_tag
    );

    result->ciphertext_len = plaintext_len + 16;
    result->key_id = csk->key_id;

    return rc;
}

/*
 * Decrypt a credential for use.
 * Returns pointer to secure memory that caller must free.
 */
int decrypt_credential(
    const EncryptedCredential* encrypted,
    const CredentialStoreKey* csk,
    char**                  plaintext_credential
) {
    /* Verify key ID matches */
    if (!uuid_equals(encrypted->key_id, csk->key_id)) {
        /* Need to get the correct key version */
        csk = get_credential_key(encrypted->key_id);
        if (!csk) return ERR_KEY_NOT_FOUND;
    }

    size_t plaintext_len = encrypted->ciphertext_len - 16;
    *plaintext_credential = secure_alloc(plaintext_len + 1);

    int rc = aes_256_gcm_decrypt(
        csk->key_material,
        encrypted->iv, 12,
        NULL, 0,
        encrypted->ciphertext, encrypted->ciphertext_len,
        encrypted->auth_tag,
        (uint8_t*)*plaintext_credential
    );

    if (rc != OK) {
        secure_free(*plaintext_credential, plaintext_len + 1);
        *plaintext_credential = NULL;
        return ERR_DECRYPTION_FAILED;
    }

    (*plaintext_credential)[plaintext_len] = '\0';
    return OK;
}
```

### In-Memory Credential Handling

c

```c
/*
 * Secure memory allocation for credentials.
 * 
 * - Memory is locked (won't swap)
 * - Memory is zeroed on allocation
 * - Memory is zeroed on free
 * - Guard pages detect overflows
 */
void* secure_alloc(size_t size) {
    /* Allocate with guard pages */
    size_t total_size = size + 2 * PAGE_SIZE;
    void* block = mmap(NULL, total_size, PROT_NONE, 
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    /* Make middle portion accessible */
    void* usable = block + PAGE_SIZE;
    mprotect(usable, size, PROT_READ | PROT_WRITE);

    /* Lock in memory (prevent swap) */
    mlock(usable, size);

    /* Zero memory */
    explicit_bzero(usable, size);

    return usable;
}

void secure_free(void* ptr, size_t size) {
    if (!ptr) return;

    /* Zero before freeing */
    explicit_bzero(ptr, size);

    /* Unlock */
    munlock(ptr, size);

    /* Calculate original mmap address */
    void* block = ptr - PAGE_SIZE;
    size_t total_size = size + 2 * PAGE_SIZE;

    munmap(block, total_size);
}
```

## Replication Stream Encryption

### Replication Key Distribution
```
When data database connects to security database for replication:
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 1. MUTUAL TLS AUTHENTICATION                                     │
├─────────────────────────────────────────────────────────────────┤
│  - Data DB presents its certificate                             │
│  - Security DB verifies against registered database list        │
│  - Security DB presents its certificate                         │
│  - Data DB verifies against known security DB list              │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 2. SESSION KEY ESTABLISHMENT                                     │
├─────────────────────────────────────────────────────────────────┤
│  - Perform ECDHE key exchange                                   │
│  - Derive session keys for encryption and MAC                   │
│  - Session keys rotate periodically                             │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 3. REPLICATION KEY DELIVERY                                      │
├─────────────────────────────────────────────────────────────────┤
│  - Security DB sends database's DBK (wrapped by CMK)            │
│  - Data DB stores wrapped key locally                           │
│  - Data DB can unwrap when CMK holder (security DB) available   │
└─────────────────────────────────────────────────────────────────┘
```

### Replication Packet Encryption

c

```c
typedef struct ReplicationPacket {
    uint64_t        sequence_number;
    uint32_t        packet_type;
    uint32_t        payload_length;
    uint8_t         iv[12];
    uint8_t         auth_tag[16];
    /* encrypted payload follows */
} ReplicationPacket;

/*
 * The replication stream uses session keys derived from
 * the initial key exchange. Keys rotate every N packets
 * or T seconds.
 */
typedef struct ReplicationSession {
    uint64_t        session_id;

    /* Current keys */
    uint8_t         send_key[32];
    uint8_t         recv_key[32];
    uint8_t         send_mac_key[32];
    uint8_t         recv_mac_key[32];

    /* Sequence numbers (prevent replay) */
    uint64_t        send_sequence;
    uint64_t        recv_sequence;
    uint64_t        recv_window_bitmap;  /* for out-of-order detection */

    /* Key rotation */
    uint64_t        packets_since_rotation;
    timestamp_t     last_rotation;

} ReplicationSession;

int encrypt_replication_packet(
    ReplicationSession*     session,
    uint32_t                packet_type,
    const uint8_t*          payload,
    size_t                  payload_length,
    uint8_t*                output,
    size_t*                 output_length
) {
    ReplicationPacket* pkt = (ReplicationPacket*)output;

    pkt->sequence_number = ++session->send_sequence;
    pkt->packet_type = packet_type;
    pkt->payload_length = payload_length;

    /* Generate IV from sequence number (ensures uniqueness) */
    derive_iv_from_sequence(session->session_id, pkt->sequence_number, pkt->iv);

    /* Encrypt payload */
    size_t header_size = sizeof(ReplicationPacket);

    int rc = aes_256_gcm_encrypt(
        session->send_key,
        pkt->iv, 12,
        /* AAD: sequence number and packet type (prevent type confusion) */
        (uint8_t*)pkt, offsetof(ReplicationPacket, iv),
        payload, payload_length,
        output + header_size,
        pkt->auth_tag
    );

    *output_length = header_size + payload_length;

    /* Check if rotation needed */
    session->packets_since_rotation++;
    if (should_rotate_keys(session)) {
        initiate_key_rotation(session);
    }

    return rc;
}
```

## Key Escrow and Recovery

### Escrow Configuration

sql

```sql
CREATE TABLE sys.key_escrow (
    escrow_id               UUID PRIMARY KEY DEFAULT gen_uuid_v7(),

    -- What's escrowed
    key_id                  UUID REFERENCES sys.master_key_config(key_id),
    key_type                ENUM('CMK', 'DBK', 'BKK') NOT NULL,

    -- Escrow method
    escrow_method           ENUM('ENCRYPTED_EXPORT', 'SPLIT_SHARES', 
                                 'HSM_BACKUP', 'EXTERNAL_KMS') NOT NULL,

    -- Encrypted key material
    escrowed_key            BYTEA,
    escrow_encryption       VARCHAR(64),  -- how the escrow is protected

    -- For split shares
    total_shares            INTEGER,
    threshold               INTEGER,
    share_holders           JSONB,

    -- Metadata
    created_at              TIMESTAMP NOT NULL DEFAULT now(),
    created_by              UUID,
    expires_at              TIMESTAMP,
    last_verified_at        TIMESTAMP,

    -- Access control
    recovery_requires       ENUM('SINGLE_ADMIN', 'MULTI_ADMIN', 'QUORUM') NOT NULL,
    recovery_admins         UUID[],

    INDEX idx_key (key_id)
);
```

### Key Export for Escrow

c

```c
/*
 * Export a key for escrow, encrypted with escrow password.
 */
int export_key_for_escrow(
    const MasterKeyConfig*  key,
    const char*             escrow_password,
    EscrowPackage*          package
) {
    /* Generate salt for password derivation */
    generate_random_bytes(package->salt, 32);

    /* Derive escrow encryption key from password */
    uint8_t escrow_key[32];
    argon2id_derive(escrow_password, package->salt, 
                    &high_security_params, escrow_key);

    /* Generate IV */
    generate_random_bytes(package->iv, 12);

    /* Encrypt the key material */
    /* We export the wrapped key, not the raw key material */
    package->encrypted_key = secure_alloc(key->wrapped_key_length + 16);

    aes_256_gcm_encrypt(
        escrow_key,
        package->iv, 12,
        /* AAD includes key metadata */
        (uint8_t*)&key->key_id, sizeof(uuid_t),
        key->wrapped_key, key->wrapped_key_length,
        package->encrypted_key,
        package->auth_tag
    );

    package->encrypted_key_length = key->wrapped_key_length + 16;

    /* Include metadata needed for import */
    package->key_id = key->key_id;
    package->key_type = key->key_type;
    package->key_algorithm = key->key_algorithm;
    package->wrapped_by_key_id = key->wrapped_by_key_id;

    /* Compute verification hash */
    compute_escrow_verification(package);

    secure_zero(escrow_key, 32);

    return OK;
}
```

### Emergency Key Recovery

sql

```sql
-- Recovery process DDL
BEGIN KEY RECOVERY
    KEY_ID 'uuid-of-lost-key'
    ESCROW_ID 'uuid-of-escrow-record'
    ESCROW_PASSWORD 'provided-by-admin'
    AUTHORIZED_BY 'admin1-uuid', 'admin2-uuid'
    REASON 'Primary HSM failed, recovering from escrow';

-- If multi-admin required, each admin runs:
APPROVE KEY RECOVERY
    RECOVERY_ID 'uuid-of-recovery-request'
    ADMIN_KEY [their-key-or-passphrase];

-- Once approvals met:
COMPLETE KEY RECOVERY
    RECOVERY_ID 'uuid-of-recovery-request';
```

## Startup and Key Loading

### Encrypted Database Startup Sequence
```
Database start with encryption enabled:
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 1. MINIMAL BOOT (no encryption yet)                              │
├─────────────────────────────────────────────────────────────────┤
│  - Load unencrypted configuration                               │
│  - Identify encryption mode and key source                      │
│  - Initialize secure memory subsystem                           │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 2. OBTAIN MASTER KEY                                             │
├─────────────────────────────────────────────────────────────────┤
│  Passphrase mode:                                               │
│    - Wait for passphrase via console/API                        │
│    - Derive CMK from passphrase                                 │
│    - Store CMK in secure memory                                 │
│                                                                 │
│  HSM mode:                                                      │
│    - Connect to HSM                                             │
│    - Authenticate to HSM (PIN/token)                            │
│    - Obtain handle to CMK (key stays in HSM)                    │
│                                                                 │
│  External KMS mode:                                             │
│    - Authenticate to KMS (IAM/credentials)                      │
│    - Request key or decrypt capability                          │
│                                                                 │
│  Split knowledge mode:                                          │
│    - Wait for threshold shares                                  │
│    - Reconstruct CMK                                            │
│    - Store in secure memory                                     │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 3. VERIFY MASTER KEY                                             │
├─────────────────────────────────────────────────────────────────┤
│  - Load key verification record from disk                       │
│  - Attempt to decrypt verification block                        │
│  - Compare against expected value                               │
│  - If mismatch: wrong passphrase/HSM key, abort                 │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 4. LOAD KEY HIERARCHY                                            │
├─────────────────────────────────────────────────────────────────┤
│  For security database:                                         │
│    - CMK is available                                           │
│    - Unwrap SDK (Security DB Key)                               │
│    - Unwrap CSK (Credential Store Key)                          │
│    - Ready to serve key requests                                │
│                                                                 │
│  For data database:                                             │
│    - Connect to security database                               │
│    - Request DBK for this database                              │
│    - Security DB unwraps DBK with CMK, sends over secure channel│
│    - Store DBK in secure memory                                 │
│    - Unwrap TSKs with DBK                                       │
│    - Unwrap LEK with DBK                                        │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 5. INITIALIZE ENCRYPTION SUBSYSTEM                               │
├─────────────────────────────────────────────────────────────────┤
│  - Register page encryption hooks                               │
│  - Register write-after log (WAL) encryption hooks               │
│  - Enable encrypted I/O                                         │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│ 6. NORMAL STARTUP                                                │
├─────────────────────────────────────────────────────────────────┤
│  - Open encrypted data files                                    │
│  - Replay encrypted write-after log (WAL) if needed              │
│  - Accept connections                                           │
└─────────────────────────────────────────────────────────────────┘
```

### Key Loading State Machine

c

```c
typedef enum KeyLoadState {
    KEY_STATE_INIT,
    KEY_STATE_WAITING_FOR_PASSPHRASE,
    KEY_STATE_WAITING_FOR_SHARES,
    KEY_STATE_CONNECTING_TO_HSM,
    KEY_STATE_CONNECTING_TO_KMS,
    KEY_STATE_VERIFYING_KEY,
    KEY_STATE_LOADING_HIERARCHY,
    KEY_STATE_READY,
    KEY_STATE_FAILED
} KeyLoadState;

typedef struct KeyLoadContext {
    KeyLoadState        state;

    /* For passphrase */
    timestamp_t         passphrase_deadline;
    int                 passphrase_attempts;

    /* For split knowledge */
    int                 shares_required;
    int                 shares_collected;
    KeyShare*           shares[MAX_SHARES];

    /* For HSM/KMS */
    void*               provider_context;
    int                 connection_attempts;

    /* Result */
    CMKHandle*          cmk;
    char*               error_message;

} KeyLoadContext;

/*
 * State machine driver.
 */
int key_load_step(KeyLoadContext* ctx) {
    switch (ctx->state) {
        case KEY_STATE_INIT:
            return key_load_init(ctx);

        case KEY_STATE_WAITING_FOR_PASSPHRASE:
            if (has_passphrase_input()) {
                char* passphrase = get_passphrase_input();
                int rc = try_passphrase(ctx, passphrase);
                secure_zero(passphrase, strlen(passphrase));
                if (rc == OK) {
                    ctx->state = KEY_STATE_VERIFYING_KEY;
                } else {
                    ctx->passphrase_attempts++;
                    if (ctx->passphrase_attempts >= MAX_ATTEMPTS) {
                        ctx->state = KEY_STATE_FAILED;
                        ctx->error_message = "Too many failed passphrase attempts";
                    }
                }
            } else if (now() > ctx->passphrase_deadline) {
                ctx->state = KEY_STATE_FAILED;
                ctx->error_message = "Passphrase timeout";
            }
            break;

        case KEY_STATE_WAITING_FOR_SHARES:
            if (ctx->shares_collected >= ctx->shares_required) {
                int rc = reconstruct_from_shares(ctx);
                if (rc == OK) {
                    ctx->state = KEY_STATE_VERIFYING_KEY;
                } else {
                    ctx->state = KEY_STATE_FAILED;
                    ctx->error_message = "Share reconstruction failed";
                }
            }
            break;

        case KEY_STATE_CONNECTING_TO_HSM:
            return key_load_hsm_connect(ctx);

        case KEY_STATE_CONNECTING_TO_KMS:
            return key_load_kms_connect(ctx);

        case KEY_STATE_VERIFYING_KEY:
            return key_load_verify(ctx);

        case KEY_STATE_LOADING_HIERARCHY:
            return key_load_hierarchy(ctx);

        case KEY_STATE_READY:
        case KEY_STATE_FAILED:
            return ctx->state == KEY_STATE_READY ? OK : ERR_KEY_LOAD_FAILED;
    }

    return IN_PROGRESS;
}
```

## Policy Configuration

### Encryption Policies

sql

```sql
-- Algorithm selection
encryption.default_algorithm = 'AES-256-GCM'
encryption.allow_legacy_algorithms = FALSE  -- disable AES-CBC

-- Key management
encryption.cmk_source = 'PASSPHRASE'  -- or 'HSM', 'EXTERNAL_KMS', 'SPLIT_KNOWLEDGE'
encryption.cmk_rotation_days = 365
encryption.dbk_rotation_days = 90
encryption.tsk_rotation_days = 30

-- HSM configuration
encryption.hsm.library_path = '/usr/lib/softhsm/libsofthsm2.so'
encryption.hsm.slot_id = 0
encryption.hsm.pin_source = 'ENV:HSM_PIN'

-- External KMS
encryption.kms.provider = 'AWS_KMS'
encryption.kms.key_arn = 'arn:aws:kms:...'
encryption.kms.region = 'us-east-1'

-- Split knowledge
encryption.split.total_shares = 5
encryption.split.threshold = 3

-- Passphrase requirements
encryption.passphrase.min_length = 16
encryption.passphrase.require_complexity = TRUE
encryption.passphrase.timeout_seconds = 300

-- Database encryption
encryption.database.mode = 'TDE'  -- or 'TABLESPACE', 'TABLE', 'NONE'
encryption.database.encrypt_temp = TRUE
encryption.database.encrypt_wal = TRUE

-- Backup encryption
encryption.backup.require_encryption = TRUE
encryption.backup.require_passphrase = TRUE  -- portable backups need passphrase
encryption.backup.passphrase_min_length = 20

-- Key rotation
encryption.rotation.max_iops = 1000
encryption.rotation.max_cpu_percent = 25
encryption.rotation.checkpoint_interval = 10000  -- pages

-- Escrow
encryption.escrow.require_escrow = TRUE
encryption.escrow.escrow_method = 'SPLIT_SHARES'
encryption.escrow.verify_interval_days = 90
```

## Audit and Monitoring

### Key Access Audit

sql

```sql
CREATE TABLE sys.key_access_audit (
    audit_id                UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    event_timestamp         TIMESTAMP NOT NULL DEFAULT now(),

    -- What key
    key_id                  UUID NOT NULL,
    key_type                VARCHAR(16) NOT NULL,
    key_name                VARCHAR(128),

    -- What operation
    operation               ENUM('UNWRAP', 'WRAP', 'ENCRYPT', 'DECRYPT',
                                 'ROTATE', 'EXPORT', 'IMPORT', 'DESTROY') NOT NULL,

    -- Who/what performed it
    performer_type          ENUM('ENGINE', 'ADMIN', 'REPLICATION', 'BACKUP') NOT NULL,
    performer_uuid          UUID,
    performer_address       INET,

    -- Result
    success                 BOOLEAN NOT NULL,
    error_code              INTEGER,
    error_message           TEXT,

    -- Context
    database_uuid           UUID,
    session_id              UUID,

    INDEX idx_timestamp (event_timestamp),
    INDEX idx_key (key_id, event_timestamp),
    INDEX idx_operation (operation, event_timestamp)
);
```

### Key Health Monitoring

sql

```sql
-- View for monitoring key status
CREATE VIEW sys.key_health AS
SELECT 
    k.key_id,
    k.key_name,
    k.key_type,
    k.status,
    k.key_version,
    k.created_at,
    k.last_rotated_at,
    k.next_rotation_at,
    CASE 
        WHEN k.next_rotation_at < now() THEN 'OVERDUE'
        WHEN k.next_rotation_at < now() + interval '7 days' THEN 'DUE_SOON'
        ELSE 'OK'
    END AS rotation_status,
    k.access_count,
    k.last_accessed_at,
    CASE
        WHEN r.state IS NOT NULL AND r.state NOT IN ('COMPLETED', 'ROLLED_BACK') 
            THEN 'ROTATION_IN_PROGRESS'
        ELSE 'IDLE'
    END AS operation_status,
    e.last_verified_at AS escrow_verified,
    CASE
        WHEN e.escrow_id IS NULL THEN 'NO_ESCROW'
        WHEN e.last_verified_at < now() - interval '90 days' THEN 'ESCROW_STALE'
        ELSE 'ESCROW_OK'
    END AS escrow_status
FROM sys.master_key_config k
LEFT JOIN sys.key_rotation_state r 
    ON k.key_id IN (r.old_key_id, r.new_key_id) 
    AND r.state NOT IN ('COMPLETED', 'ROLLED_BACK', 'FAILED')
LEFT JOIN sys.key_escrow e 
    ON k.key_id = e.key_id;
```

### Alerting Conditions
```
-- Key rotation overdue
ALERT IF EXISTS (
    SELECT 1 FROM sys.master_key_config
    WHERE status = 'ACTIVE'
    AND next_rotation_at < now()
)

-- Escrow not verified recently
ALERT IF EXISTS (
    SELECT 1 FROM sys.key_escrow
    WHERE last_verified_at < now() - interval '90 days'
)

-- Key access spike (possible attack)
ALERT IF (
    SELECT COUNT(*) FROM sys.key_access_audit
    WHERE event_timestamp > now() - interval '1 minute'
    AND operation = 'DECRYPT'
) > 1000

-- Failed key operations
ALERT IF EXISTS (
    SELECT 1 FROM sys.key_access_audit
    WHERE event_timestamp > now() - interval '5 minutes'
    AND success = FALSE
    AND operation IN ('UNWRAP', 'DECRYPT')
)

-- HSM connection failure
ALERT IF encryption.cmk_source = 'HSM' 
AND hsm_connection_status() != 'CONNECTED'

-- KMS connection failure
ALERT IF encryption.cmk_source = 'EXTERNAL_KMS'
AND kms_connection_status() != 'CONNECTED'
```

## Summary Tables

### Key Type Reference

| Key Type | Scope            | Stored Where          | Protected By       | Rotation Frequency |
| -------- | ---------------- | --------------------- | ------------------ | ------------------ |
| CMK      | Cluster          | Security DB (wrapped) | Passphrase/HSM/KMS | Annual             |
| SDK      | Security DB      | Security DB           | CMK                | Annual             |
| CSK      | Credential store | Security DB           | SDK                | Annual             |
| DBK      | Database         | Security DB           | CMK                | Quarterly          |
| TSK      | Tablespace       | Database              | DBK                | Monthly            |
| DEK      | Page/extent      | Page header           | TSK                | Per-page IV        |
| LEK      | Transaction log  | Database              | DBK                | Quarterly          |
| RPK      | Replication      | Session               | Key exchange       | Per-session        |
| BKK      | Backup           | Backup header         | CMK + passphrase   | Per-backup         |

### Policy Reference

| Policy Key                               | Type    | Default     | Description                  |
| ---------------------------------------- | ------- | ----------- | ---------------------------- |
| `encryption.default_algorithm`           | ENUM    | AES-256-GCM | Default encryption algorithm |
| `encryption.cmk_source`                  | ENUM    | PASSPHRASE  | CMK derivation source        |
| `encryption.cmk_rotation_days`           | INTEGER | 365         | CMK rotation period          |
| `encryption.dbk_rotation_days`           | INTEGER | 90          | DBK rotation period          |
| `encryption.tsk_rotation_days`           | INTEGER | 30          | TSK rotation period          |
| `encryption.passphrase.min_length`       | INTEGER | 16          | Minimum passphrase length    |
| `encryption.passphrase.timeout_seconds`  | INTEGER | 300         | Startup passphrase timeout   |
| `encryption.database.mode`               | ENUM    | TDE         | Database encryption mode     |
| `encryption.database.encrypt_temp`       | BOOLEAN | TRUE        | Encrypt temp tables          |
| `encryption.database.encrypt_wal`        | BOOLEAN | TRUE        | Encrypt transaction log      |
| `encryption.backup.require_encryption`   | BOOLEAN | TRUE        | Force backup encryption      |
| `encryption.backup.require_passphrase`   | BOOLEAN | TRUE        | Require backup passphrase    |
| `encryption.rotation.max_iops`           | INTEGER | 1000        | I/O limit during rotation    |
| `encryption.rotation.max_cpu_percent`    | INTEGER | 25          | CPU limit during rotation    |
| `encryption.escrow.require_escrow`       | BOOLEAN | TRUE        | Mandate key escrow           |
| `encryption.escrow.verify_interval_days` | INTEGER | 90          | Escrow verification period   |

Wire protocol security is where your compatibility layers meet the authentication and encryption systems we've designed. Each protocol has its own authentication dance, and you need to bridge them all to your unified security model.

## Protocol Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         CLIENT CONNECTIONS                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │  PostgreSQL  │  │    MySQL     │  │    MSSQL     │  │   Firebird   │    │
│  │   Client     │  │   Client     │  │   Client     │  │   Client     │    │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘    │
│         │                 │                 │                 │             │
│         v                 v                 v                 v             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │  PG Protocol │  │ MySQL Proto  │  │  TDS Proto   │  │  FB Protocol │    │
│  │   Handler    │  │   Handler    │  │   Handler    │  │   Handler    │    │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘    │
│         │                 │                 │                 │             │
│         └─────────────────┴────────┬────────┴─────────────────┘             │
│                                    │                                        │
│                                    v                                        │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                    PROTOCOL ABSTRACTION LAYER                         │  │
│  │                                                                       │  │
│  │   ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐      │  │
│  │   │  Auth Request   │  │  TLS Handling   │  │  Session Mgmt   │      │  │
│  │   │  Normalization  │  │                 │  │                 │      │  │
│  │   └─────────────────┘  └─────────────────┘  └─────────────────┘      │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                    │                                        │
│                                    v                                        │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                    UNIFIED SECURITY LAYER                             │  │
│  │                                                                       │  │
│  │   Authentication Dispatcher → Auth Providers → Session Creation      │  │
│  │                                                                       │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Protocol Authentication Comparison

| Feature            | PostgreSQL                     | MySQL                                        | MSSQL (TDS)         | Firebird             |
| ------------------ | ------------------------------ | -------------------------------------------- | ------------------- | -------------------- |
| TLS Negotiation    | StartupMessage then SSLRequest | Initial handshake flag                       | PRELOGIN            | Connection parameter |
| Password Auth      | MD5, SCRAM-SHA-256             | mysql_native_password, caching_sha2_password | SQL Auth            | SRP, Legacy          |
| Cleartext Password | Supported (discouraged)        | mysql_clear_password                         | Within TLS only     | Legacy only          |
| Kerberos/GSSAPI    | GSSAPI auth type               | auth_gssapi_client plugin                    | Windows Auth / SSPI | Supported            |
| Certificate Auth   | cert auth type                 | Client cert validation                       | Certificate mapping | Supported            |
| Multi-step Auth    | SASL continuation              | Auth switch                                  | SSPI exchange       | SRP exchange         |
| Channel Binding    | tls-server-end-point           | Not standard                                 | Extended Protection | Not standard         |

## TLS Integration

### Listener Configuration

sql

```sql
CREATE TABLE sys.protocol_listeners (
    listener_id             UUID PRIMARY KEY DEFAULT gen_uuid_v7(),

    -- Binding
    address                 INET NOT NULL,
    port                    INTEGER NOT NULL,
    protocol                ENUM('POSTGRES', 'MYSQL', 'MSSQL', 'FIREBIRD') NOT NULL,

    -- TLS settings
    tls_mode                ENUM('DISABLED', 'PREFERRED', 'REQUIRED') NOT NULL DEFAULT 'PREFERRED',
    tls_cert_file           VARCHAR(512),
    tls_key_file            VARCHAR(512),
    tls_ca_file             VARCHAR(512),  -- for client cert verification
    tls_min_version         ENUM('TLS12', 'TLS13') DEFAULT 'TLS12',
    tls_ciphers             TEXT,  -- cipher suite specification

    -- Client certificate handling
    client_cert_mode        ENUM('DISABLED', 'OPTIONAL', 'REQUIRED') DEFAULT 'DISABLED',
    client_cert_cn_mapping  ENUM('NONE', 'USERNAME', 'EXTERNAL_ID') DEFAULT 'NONE',

    -- Protocol-specific options
    protocol_options        JSONB,

    -- Status
    status                  ENUM('ACTIVE', 'DISABLED') NOT NULL DEFAULT 'ACTIVE',

    UNIQUE (address, port),
    INDEX idx_protocol (protocol)
);
```

### TLS Handshake by Protocol

c

```c
/*
 * PostgreSQL TLS negotiation.
 * 
 * Client sends SSLRequest, server responds with 'S' or 'N',
 * then TLS handshake occurs before authentication.
 */
typedef struct PostgresConnectionState {
    int                 socket_fd;
    SSL*                ssl;
    ConnectionPhase     phase;
    bool                ssl_requested;
    bool                ssl_established;

    /* Post-handshake */
    char*               client_cert_cn;
    uuid_t              session_id;

} PostgresConnectionState;

int postgres_handle_ssl_request(PostgresConnectionState* conn) {
    /* Read SSLRequest message */
    SSLRequest req;
    read_message(conn->socket_fd, &req, sizeof(req));

    if (ntohl(req.request_code) != SSL_REQUEST_CODE) {
        /* Not an SSL request, rewind and continue */
        return handle_startup_message(conn, &req);
    }

    ListenerConfig* listener = get_listener_config(conn);

    switch (listener->tls_mode) {
        case TLS_DISABLED:
            /* Send 'N' - SSL not supported */
            write_byte(conn->socket_fd, 'N');
            conn->ssl_established = false;
            break;

        case TLS_PREFERRED:
        case TLS_REQUIRED:
            /* Send 'S' - SSL supported */
            write_byte(conn->socket_fd, 'S');

            /* Perform TLS handshake */
            int rc = perform_tls_handshake(conn, listener);
            if (rc != OK) {
                log_warning("TLS handshake failed: %s", get_tls_error());
                close_connection(conn);
                return ERR_TLS_HANDSHAKE_FAILED;
            }

            conn->ssl_established = true;

            /* Extract client certificate info if present */
            if (listener->client_cert_mode != CERT_DISABLED) {
                extract_client_cert_info(conn);
            }
            break;
    }

    /* Now wait for StartupMessage (over TLS if established) */
    conn->phase = PHASE_STARTUP;
    return OK;
}

/*
 * MySQL TLS negotiation.
 * 
 * Server sends greeting with capability flags,
 * client responds with SSL flag set if desired,
 * TLS handshake occurs, then client sends full handshake response.
 */
int mysql_handle_connection(MySQLConnectionState* conn) {
    ListenerConfig* listener = get_listener_config(conn);

    /* Build server greeting */
    MySQLHandshakeV10 greeting = {0};
    greeting.protocol_version = 10;
    strcpy(greeting.server_version, "8.0.0-mydb");
    greeting.connection_id = conn->connection_id;
    generate_random_bytes(greeting.auth_plugin_data, 20);

    /* Set capability flags */
    greeting.capability_flags = 
        CLIENT_LONG_PASSWORD |
        CLIENT_PROTOCOL_41 |
        CLIENT_SECURE_CONNECTION |
        CLIENT_PLUGIN_AUTH |
        CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA;

    if (listener->tls_mode != TLS_DISABLED) {
        greeting.capability_flags |= CLIENT_SSL;
    }

    /* Send greeting */
    send_mysql_packet(conn, &greeting);

    /* Read client response */
    MySQLHandshakeResponse response;
    recv_mysql_packet(conn, &response);

    /* Client wants SSL? */
    if (response.capability_flags & CLIENT_SSL) {
        if (listener->tls_mode == TLS_DISABLED) {
            send_mysql_error(conn, ER_NOT_SUPPORTED_AUTH_MODE,
                           "SSL not supported on this listener");
            return ERR_TLS_NOT_AVAILABLE;
        }

        /* Perform TLS handshake */
        int rc = perform_tls_handshake(conn, listener);
        if (rc != OK) {
            return rc;
        }
        conn->ssl_established = true;

        /* Client sends full handshake response again over TLS */
        recv_mysql_packet(conn, &response);
    } else if (listener->tls_mode == TLS_REQUIRED) {
        send_mysql_error(conn, ER_SECURE_TRANSPORT_REQUIRED,
                        "Connections using insecure transport are prohibited");
        return ERR_TLS_REQUIRED;
    }

    /* Store auth data for processing */
    conn->client_capabilities = response.capability_flags;
    conn->username = strdup(response.username);
    conn->auth_response = dup_bytes(response.auth_response, 
                                     response.auth_response_len);
    conn->auth_plugin = strdup(response.auth_plugin);
    conn->database = response.database ? strdup(response.database) : NULL;

    return proceed_to_authentication(conn);
}

/*
 * MSSQL (TDS) TLS negotiation.
 * 
 * PRELOGIN exchange includes encryption negotiation,
 * then TLS handshake occurs within TDS packets (if encrypting).
 */
int tds_handle_connection(TDSConnectionState* conn) {
    ListenerConfig* listener = get_listener_config(conn);

    /* Read PRELOGIN */
    TDSPrelogin client_prelogin;
    recv_tds_packet(conn, TDS_PRELOGIN, &client_prelogin);

    /* Parse encryption option */
    uint8_t client_encrypt = get_prelogin_option(&client_prelogin, 
                                                   PL_OPTION_ENCRYPTION);

    /* Build server PRELOGIN response */
    TDSPrelogin server_prelogin = {0};

    switch (listener->tls_mode) {
        case TLS_DISABLED:
            if (client_encrypt == ENCRYPT_REQ) {
                /* Client requires encryption, we don't support it */
                set_prelogin_option(&server_prelogin, PL_OPTION_ENCRYPTION, 
                                   ENCRYPT_NOT_SUP);
            } else {
                set_prelogin_option(&server_prelogin, PL_OPTION_ENCRYPTION,
                                   ENCRYPT_OFF);
            }
            break;

        case TLS_PREFERRED:
            if (client_encrypt == ENCRYPT_OFF) {
                set_prelogin_option(&server_prelogin, PL_OPTION_ENCRYPTION,
                                   ENCRYPT_OFF);
            } else {
                set_prelogin_option(&server_prelogin, PL_OPTION_ENCRYPTION,
                                   ENCRYPT_ON);
                conn->will_encrypt = true;
            }
            break;

        case TLS_REQUIRED:
            if (client_encrypt == ENCRYPT_NOT_SUP) {
                close_connection(conn);
                return ERR_TLS_REQUIRED;
            }
            set_prelogin_option(&server_prelogin, PL_OPTION_ENCRYPTION,
                               ENCRYPT_REQ);
            conn->will_encrypt = true;
            break;
    }

    /* Add other PRELOGIN options */
    set_prelogin_option(&server_prelogin, PL_OPTION_VERSION, 
                        TDS_VERSION_7_4);

    send_tds_packet(conn, TDS_PRELOGIN, &server_prelogin);

    /* TLS handshake if encrypting */
    if (conn->will_encrypt) {
        /* TDS wraps TLS handshake in PRELOGIN packets */
        int rc = perform_tds_tls_handshake(conn, listener);
        if (rc != OK) {
            return rc;
        }
        conn->ssl_established = true;
    }

    /* Now wait for LOGIN7 */
    conn->phase = PHASE_LOGIN;
    return OK;
}
```

## Authentication Message Normalization

### Unified Authentication Request

c

```c
/*
 * Normalize protocol-specific auth into unified request.
 */
typedef struct UnifiedAuthRequest {
    /* Identity */
    char*               username;
    char*               database;       /* requested database, if any */

    /* Protocol info */
    WireProtocol        source_protocol;
    void*               protocol_state; /* for protocol-specific responses */

    /* TLS info */
    bool                tls_established;
    char*               client_cert_cn;
    char*               client_cert_dn;

    /* Credentials (type depends on auth method) */
    ProtocolAuthMethod  protocol_auth_method;
    union {
        /* Password-based */
        struct {
            uint8_t*    password_hash;      /* pre-hashed by client */
            size_t      hash_length;
            uint8_t*    salt;               /* server-provided salt */
            HashType    hash_type;          /* MD5, SHA256, etc. */
        } hashed_password;

        struct {
            char*       password;           /* cleartext - over TLS only! */
            size_t      password_length;
        } cleartext_password;

        /* SASL/SCRAM */
        struct {
            char*       mechanism;          /* SCRAM-SHA-256, etc. */
            uint8_t*    client_first;
            size_t      client_first_len;
            uint8_t*    client_final;
            size_t      client_final_len;
        } sasl;

        /* Kerberos/GSSAPI */
        struct {
            uint8_t*    token;
            size_t      token_length;
            char*       service_principal;
        } gssapi;

        /* Certificate */
        struct {
            uint8_t*    certificate_der;
            size_t      certificate_length;
        } certificate;

    } credentials;

    /* Connection context */
    struct sockaddr*    client_addr;
    char*               client_application;

} UnifiedAuthRequest;
```

### Protocol-Specific Normalization

c

```c
/*
 * PostgreSQL authentication normalization.
 */
int normalize_postgres_auth(PostgresConnectionState* conn, 
                            UnifiedAuthRequest* request) {
    StartupMessage* startup = &conn->startup_message;

    request->username = get_startup_param(startup, "user");
    request->database = get_startup_param(startup, "database");
    request->source_protocol = PROTO_POSTGRES;
    request->protocol_state = conn;
    request->tls_established = conn->ssl_established;
    request->client_cert_cn = conn->client_cert_cn;

    /* Determine auth method based on configuration */
    AuthMethod method = get_auth_method_for_user(request->username,
                                                  request->database,
                                                  conn->client_addr);

    switch (method) {
        case AUTH_MD5:
            /* Server sends random salt, client sends md5(md5(pass+user)+salt) */
            generate_random_bytes(conn->md5_salt, 4);
            send_auth_request(conn, AUTH_REQ_MD5, conn->md5_salt, 4);

            /* Receive password message */
            PasswordMessage pwd_msg;
            recv_password_message(conn, &pwd_msg);

            request->protocol_auth_method = PROTO_AUTH_MD5;
            request->credentials.hashed_password.password_hash = 
                dup_bytes(pwd_msg.password, 35);  /* "md5" + 32 hex chars */
            request->credentials.hashed_password.hash_length = 35;
            request->credentials.hashed_password.salt = 
                dup_bytes(conn->md5_salt, 4);
            request->credentials.hashed_password.hash_type = HASH_MD5;
            break;

        case AUTH_SCRAM_SHA_256:
            /* SASL negotiation */
            send_auth_request(conn, AUTH_REQ_SASL, "SCRAM-SHA-256", 0);

            /* Receive SASLInitialResponse */
            SASLMessage sasl_init;
            recv_sasl_message(conn, &sasl_init);

            request->protocol_auth_method = PROTO_AUTH_SASL;
            request->credentials.sasl.mechanism = strdup(sasl_init.mechanism);
            request->credentials.sasl.client_first = 
                dup_bytes(sasl_init.data, sasl_init.length);
            request->credentials.sasl.client_first_len = sasl_init.length;
            /* client_final filled in during SASL continuation */
            break;

        case AUTH_GSSAPI:
            send_auth_request(conn, AUTH_REQ_GSS, NULL, 0);

            /* Receive GSSAPI token */
            GSSAPIMessage gss_msg;
            recv_gssapi_message(conn, &gss_msg);

            request->protocol_auth_method = PROTO_AUTH_GSSAPI;
            request->credentials.gssapi.token = 
                dup_bytes(gss_msg.data, gss_msg.length);
            request->credentials.gssapi.token_length = gss_msg.length;
            request->credentials.gssapi.service_principal = 
                strdup(get_service_principal());
            break;

        case AUTH_CERT:
            if (!conn->ssl_established || !conn->client_cert_cn) {
                send_error(conn, "certificate required");
                return ERR_CERT_REQUIRED;
            }
            request->protocol_auth_method = PROTO_AUTH_CERTIFICATE;
            request->credentials.certificate.certificate_der = 
                get_peer_certificate_der(conn->ssl, 
                    &request->credentials.certificate.certificate_length);
            break;

        case AUTH_TRUST:
            request->protocol_auth_method = PROTO_AUTH_TRUST;
            break;
    }

    return OK;
}

/*
 * MySQL authentication normalization.
 */
int normalize_mysql_auth(MySQLConnectionState* conn,
                         UnifiedAuthRequest* request) {
    request->username = strdup(conn->username);
    request->database = conn->database ? strdup(conn->database) : NULL;
    request->source_protocol = PROTO_MYSQL;
    request->protocol_state = conn;
    request->tls_established = conn->ssl_established;

    /* Determine auth plugin */
    if (strcmp(conn->auth_plugin, "mysql_native_password") == 0) {
        request->protocol_auth_method = PROTO_AUTH_MYSQL_NATIVE;

        /* mysql_native_password: SHA1(SHA1(password)) XOR SHA1(scramble + SHA1(SHA1(password))) */
        request->credentials.hashed_password.password_hash = 
            dup_bytes(conn->auth_response, conn->auth_response_len);
        request->credentials.hashed_password.hash_length = conn->auth_response_len;
        request->credentials.hashed_password.salt = 
            dup_bytes(conn->scramble, 20);
        request->credentials.hashed_password.hash_type = HASH_MYSQL_NATIVE;

    } else if (strcmp(conn->auth_plugin, "caching_sha2_password") == 0) {
        request->protocol_auth_method = PROTO_AUTH_MYSQL_CACHING_SHA2;

        /* caching_sha2_password: more complex, may require RSA key exchange */
        request->credentials.hashed_password.password_hash = 
            dup_bytes(conn->auth_response, conn->auth_response_len);
        request->credentials.hashed_password.hash_length = conn->auth_response_len;
        request->credentials.hashed_password.salt = 
            dup_bytes(conn->scramble, 20);
        request->credentials.hashed_password.hash_type = HASH_CACHING_SHA2;

    } else if (strcmp(conn->auth_plugin, "mysql_clear_password") == 0) {
        if (!conn->ssl_established) {
            send_mysql_error(conn, ER_SECURE_TRANSPORT_REQUIRED,
                           "Cleartext password requires TLS");
            return ERR_TLS_REQUIRED;
        }

        request->protocol_auth_method = PROTO_AUTH_CLEARTEXT;
        request->credentials.cleartext_password.password = 
            strdup((char*)conn->auth_response);
        request->credentials.cleartext_password.password_length = 
            strlen((char*)conn->auth_response);

    } else if (strcmp(conn->auth_plugin, "authentication_kerberos") == 0) {
        request->protocol_auth_method = PROTO_AUTH_GSSAPI;
        /* SPNEGO token in auth_response */
        request->credentials.gssapi.token = 
            dup_bytes(conn->auth_response, conn->auth_response_len);
        request->credentials.gssapi.token_length = conn->auth_response_len;
    }

    return OK;
}

/*
 * MSSQL (TDS) authentication normalization.
 */
int normalize_tds_auth(TDSConnectionState* conn,
                       UnifiedAuthRequest* request) {
    TDSLogin7* login = &conn->login7;

    request->username = strdup(login->username);
    request->database = login->database ? strdup(login->database) : NULL;
    request->source_protocol = PROTO_MSSQL;
    request->protocol_state = conn;
    request->tls_established = conn->ssl_established;
    request->client_application = strdup(login->app_name);

    /* Check for integrated auth (SSPI) */
    if (login->option_flags2 & TDS_INTEGRATED_SECURITY) {
        request->protocol_auth_method = PROTO_AUTH_SSPI;

        /* SSPI token in SSPI field */
        request->credentials.gssapi.token = 
            dup_bytes(login->sspi_data, login->sspi_length);
        request->credentials.gssapi.token_length = login->sspi_length;

    } else {
        /* SQL Authentication */
        if (!conn->ssl_established) {
            /* TDS sends password XOR-obfuscated, but NOT encrypted */
            /* Should require TLS for SQL auth */
            log_warning("SQL auth without TLS from %s", 
                       conn->client_addr_str);
        }

        request->protocol_auth_method = PROTO_AUTH_CLEARTEXT;

        /* TDS password is UTF-16LE, nibble-swapped */
        char* decoded_password = decode_tds_password(login->password, 
                                                      login->password_length);
        request->credentials.cleartext_password.password = decoded_password;
        request->credentials.cleartext_password.password_length = 
            strlen(decoded_password);
    }

    return OK;
}
```

## Authentication Method Mapping

### Protocol to Internal Auth Mapping

sql

```sql
CREATE TABLE sys.protocol_auth_mapping (
    mapping_id              UUID PRIMARY KEY DEFAULT gen_uuid_v7(),

    -- Match criteria
    protocol                ENUM('POSTGRES', 'MYSQL', 'MSSQL', 'FIREBIRD', 'ANY') NOT NULL,
    protocol_auth_method    VARCHAR(64) NOT NULL,  -- e.g., 'SCRAM-SHA-256', 'mysql_native_password'

    -- Target internal auth
    internal_provider_id    UUID REFERENCES sys.auth_providers(provider_id),

    -- Transformation rules
    transform_type          ENUM('DIRECT', 'VERIFY_THEN_HASH', 'CHALLENGE_RESPONSE') NOT NULL,
    transform_config        JSONB,

    -- Status
    status                  ENUM('ACTIVE', 'DISABLED') NOT NULL DEFAULT 'ACTIVE',
    priority                INTEGER NOT NULL DEFAULT 100,

    UNIQUE (protocol, protocol_auth_method),
    INDEX idx_protocol (protocol)
);
```

### Password Verification Strategies

c

```c
/*
 * The challenge: client sends protocol-specific hash,
 * but we store passwords in our own format.
 * 
 * Options:
 * 1. Store multiple hash formats (wasteful, security risk)
 * 2. Request cleartext and verify against our hash (requires TLS)
 * 3. Use protocol's native format as primary (compatibility mode)
 * 4. SCRAM/SRP where possible (both sides compute, compare)
 */

typedef enum PasswordVerifyStrategy {
    /* Store password in protocol-native format */
    STRATEGY_NATIVE_FORMAT,

    /* Store our format, request cleartext over TLS */
    STRATEGY_CLEARTEXT_REQUIRED,

    /* Store our format, use SCRAM for protocols that support it */
    STRATEGY_PREFER_SCRAM,

    /* Store multiple formats (legacy compatibility) */
    STRATEGY_MULTI_FORMAT,

} PasswordVerifyStrategy;

/*
 * Verify password using appropriate strategy.
 */
int verify_password(UnifiedAuthRequest* request, 
                    StoredCredential* stored,
                    VerifyResult* result) {

    PasswordVerifyStrategy strategy = get_verify_strategy(stored, request);

    switch (strategy) {
        case STRATEGY_NATIVE_FORMAT:
            /* Stored hash is in protocol's native format */
            return verify_native_format(request, stored, result);

        case STRATEGY_CLEARTEXT_REQUIRED:
            if (request->protocol_auth_method != PROTO_AUTH_CLEARTEXT) {
                /* Need to request cleartext */
                return request_cleartext_auth(request, result);
            }
            return verify_cleartext_against_stored(
                request->credentials.cleartext_password.password,
                stored,
                result
            );

        case STRATEGY_PREFER_SCRAM:
            if (protocol_supports_scram(request->source_protocol)) {
                return verify_scram(request, stored, result);
            }
            /* Fall back to cleartext if TLS, else native */
            if (request->tls_established) {
                return request_cleartext_auth(request, result);
            }
            return verify_native_format(request, stored, result);

        case STRATEGY_MULTI_FORMAT:
            /* Try to find matching format in stored hashes */
            return verify_multi_format(request, stored, result);
    }
}

/*
 * Native format verification examples.
 */
int verify_native_format(UnifiedAuthRequest* request,
                         StoredCredential* stored,
                         VerifyResult* result) {

    switch (request->protocol_auth_method) {
        case PROTO_AUTH_MD5:
            /* PostgreSQL MD5: md5(md5(password + username) + salt) */
            /* We need stored md5(password + username) */
            if (stored->format != CRED_FORMAT_PG_MD5) {
                result->success = false;
                result->reason = "Incompatible credential format";
                return ERR_AUTH_FORMAT_MISMATCH;
            }

            uint8_t expected[16];
            compute_pg_md5_response(stored->hash, request->username,
                                    request->credentials.hashed_password.salt,
                                    expected);

            /* Client sent "md5" + hex(hash) */
            uint8_t client_hash[16];
            hex_decode(request->credentials.hashed_password.password_hash + 3,
                      32, client_hash);

            result->success = constant_time_compare(expected, client_hash, 16);
            break;

        case PROTO_AUTH_MYSQL_NATIVE:
            /* mysql_native_password: double SHA1 with scramble */
            if (stored->format != CRED_FORMAT_MYSQL_NATIVE) {
                result->success = false;
                return ERR_AUTH_FORMAT_MISMATCH;
            }

            /* stored = SHA1(SHA1(password)) */
            /* client sends: SHA1(scramble + stored) XOR SHA1(password) */
            uint8_t expected_response[20];
            compute_mysql_native_response(
                stored->hash,  /* SHA1(SHA1(password)) */
                request->credentials.hashed_password.salt,  /* scramble */
                expected_response
            );

            result->success = constant_time_compare(
                expected_response,
                request->credentials.hashed_password.password_hash,
                20
            );
            break;

        case PROTO_AUTH_SASL:
            /* SCRAM - stateful, multi-round */
            return handle_scram_exchange(request, stored, result);
    }

    return OK;
}
```

### SCRAM-SHA-256 Implementation

c

```c
/*
 * SCRAM-SHA-256 exchange (PostgreSQL primary, MySQL supports via plugin).
 * 
 * Advantage: Neither side sends password or password-equivalent.
 * Both sides prove knowledge independently.
 */
typedef struct SCRAMState {
    /* From client-first-message */
    char*               client_nonce;
    char*               username;

    /* From server-first-message */
    char*               server_nonce;
    char*               combined_nonce;
    uint8_t*            salt;
    size_t              salt_len;
    int                 iterations;

    /* Computed values */
    uint8_t             salted_password[32];
    uint8_t             client_key[32];
    uint8_t             server_key[32];
    uint8_t             stored_key[32];

    /* Messages for signature */
    char*               client_first_bare;
    char*               server_first;
    char*               client_final_without_proof;

} SCRAMState;

int handle_scram_exchange(UnifiedAuthRequest* request,
                          StoredCredential* stored,
                          VerifyResult* result) {

    SCRAMState* state = get_or_create_scram_state(request);

    if (request->credentials.sasl.client_first) {
        /* Step 1: Parse client-first-message */
        parse_client_first(request->credentials.sasl.client_first,
                          request->credentials.sasl.client_first_len,
                          state);

        /* Generate server-first-message */
        state->server_nonce = generate_nonce(24);
        state->combined_nonce = concat(state->client_nonce, state->server_nonce);

        /* Get salt and iteration count from stored credential */
        /* For SCRAM, we store: salt, iteration_count, StoredKey, ServerKey */
        state->salt = stored->scram.salt;
        state->salt_len = stored->scram.salt_len;
        state->iterations = stored->scram.iterations;

        /* Build server-first-message */
        char* server_first = format_server_first(state);
        state->server_first = server_first;

        /* Send via protocol */
        send_scram_message(request, SCRAM_SERVER_FIRST, server_first);

        result->continue_auth = true;
        return OK;

    } else if (request->credentials.sasl.client_final) {
        /* Step 2: Parse client-final-message, verify proof */
        char* client_final = (char*)request->credentials.sasl.client_final;

        /* Parse channel binding, nonce, proof */
        char* channel_binding;
        char* nonce;
        uint8_t client_proof[32];
        parse_client_final(client_final, &channel_binding, &nonce, client_proof);

        /* Verify nonce */
        if (strcmp(nonce, state->combined_nonce) != 0) {
            result->success = false;
            result->reason = "Nonce mismatch";
            return ERR_AUTH_FAILED;
        }

        /* Build AuthMessage */
        char* auth_message = concat3(
            state->client_first_bare, ",",
            state->server_first, ",",
            get_client_final_without_proof(client_final)
        );

        /* Compute expected ClientSignature */
        /* ClientSignature = HMAC(StoredKey, AuthMessage) */
        uint8_t client_signature[32];
        hmac_sha256(stored->scram.stored_key, 32,
                   (uint8_t*)auth_message, strlen(auth_message),
                   client_signature);

        /* Recover ClientKey from proof */
        /* ClientProof = ClientKey XOR ClientSignature */
        uint8_t recovered_client_key[32];
        xor_bytes(client_proof, client_signature, recovered_client_key, 32);

        /* Verify: H(ClientKey) should equal StoredKey */
        uint8_t computed_stored_key[32];
        sha256(recovered_client_key, 32, computed_stored_key);

        if (!constant_time_compare(computed_stored_key, 
                                   stored->scram.stored_key, 32)) {
            result->success = false;
            result->reason = "Invalid client proof";
            return ERR_AUTH_FAILED;
        }

        /* Compute ServerSignature for client to verify us */
        /* ServerSignature = HMAC(ServerKey, AuthMessage) */
        uint8_t server_signature[32];
        hmac_sha256(stored->scram.server_key, 32,
                   (uint8_t*)auth_message, strlen(auth_message),
                   server_signature);

        /* Send server-final-message */
        char* server_final = format_server_final(server_signature);
        send_scram_message(request, SCRAM_SERVER_FINAL, server_final);

        result->success = true;
        return OK;
    }

    return ERR_PROTOCOL_ERROR;
}
```

### GSSAPI/Kerberos Handling

c

```c
/*
 * GSSAPI/Kerberos authentication.
 * Used by PostgreSQL (gss auth), MySQL (auth_gssapi_client plugin),
 * and MSSQL (integrated auth/SSPI).
 */
int handle_gssapi_auth(UnifiedAuthRequest* request,
                       VerifyResult* result) {

    gss_ctx_id_t context = GSS_C_NO_CONTEXT;
    gss_buffer_desc input_token;
    gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
    gss_name_t client_name = GSS_C_NO_NAME;
    OM_uint32 major, minor;

    /* Get our service credential */
    gss_cred_id_t server_cred = get_server_gss_credential();

    input_token.value = request->credentials.gssapi.token;
    input_token.length = request->credentials.gssapi.token_length;

    /* May need multiple rounds */
    do {
        major = gss_accept_sec_context(
            &minor,
            &context,
            server_cred,
            &input_token,
            GSS_C_NO_CHANNEL_BINDINGS,
            &client_name,
            NULL,  /* mech_type */
            &output_token,
            NULL,  /* ret_flags */
            NULL,  /* time_rec */
            NULL   /* delegated_cred */
        );

        if (output_token.length > 0) {
            /* Send continuation to client */
            send_gssapi_continue(request, &output_token);
            gss_release_buffer(&minor, &output_token);

            if (major == GSS_S_CONTINUE_NEEDED) {
                /* Receive next token from client */
                receive_gssapi_token(request, &input_token);
            }
        }

    } while (major == GSS_S_CONTINUE_NEEDED);

    if (GSS_ERROR(major)) {
        char* error_msg = get_gss_error(major, minor);
        result->success = false;
        result->reason = error_msg;

        if (context != GSS_C_NO_CONTEXT) {
            gss_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
        }
        return ERR_AUTH_FAILED;
    }

    /* Extract principal name */
    gss_buffer_desc name_buffer;
    major = gss_display_name(&minor, client_name, &name_buffer, NULL);

    if (GSS_ERROR(major)) {
        result->success = false;
        result->reason = "Failed to get principal name";
        return ERR_AUTH_FAILED;
    }

    char* principal = strndup(name_buffer.value, name_buffer.length);
    gss_release_buffer(&minor, &name_buffer);
    gss_release_name(&minor, &client_name);

    /* Map Kerberos principal to database user */
    UserMapping* mapping = lookup_kerberos_mapping(principal);

    if (!mapping) {
        /* Check if username matches principal */
        if (!principal_matches_user(principal, request->username)) {
            result->success = false;
            result->reason = "Principal does not match requested user";
            free(principal);
            return ERR_AUTH_FAILED;
        }
    } else {
        /* Use mapped user */
        request->username = strdup(mapping->database_user);
    }

    result->success = true;
    result->auth_principal = principal;
    result->gss_context = context;  /* Keep for session encryption */

    return OK;
}
```

## Session Establishment

### Session Creation Flow

c

```c
/*
 * After successful authentication, create session.
 */
int create_authenticated_session(UnifiedAuthRequest* request,
                                 VerifyResult* auth_result,
                                 Session** session) {

    /* Lookup user in security catalog */
    User* user = lookup_user_by_name(request->username);

    if (!user) {
        /* User authenticated but doesn't exist locally */
        if (policy_allows_auto_create()) {
            user = auto_create_user(request, auth_result);
        } else {
            return ERR_USER_NOT_FOUND;
        }
    }

    /* Check user status */
    if (user->status != USER_ACTIVE) {
        return ERR_USER_BLOCKED;
    }

    /* Create session record */
    *session = allocate_session();
    (*session)->session_id = generate_uuid_v7();
    (*session)->user_uuid = user->uuid;
    (*session)->username = strdup(user->username);
    (*session)->source_protocol = request->source_protocol;
    (*session)->protocol_state = request->protocol_state;
    (*session)->database = request->database ? 
                           strdup(request->database) : 
                           strdup(user->default_database);

    /* TLS session info */
    (*session)->tls_established = request->tls_established;
    if (request->tls_established) {
        (*session)->tls_version = get_tls_version(request);
        (*session)->tls_cipher = get_tls_cipher(request);
    }

    /* Auth info */
    (*session)->auth_method = auth_result->auth_method;
    (*session)->authenticated_at = now();
    (*session)->expires_at = now() + session_timeout();

    /* Kerberos-specific */
    if (auth_result->gss_context) {
        (*session)->gss_context = auth_result->gss_context;
        /* Can enable GSSAPI encryption/integrity on session */
    }

    /* Client info */
    (*session)->client_address = dup_sockaddr(request->client_addr);
    (*session)->client_application = request->client_application ?
                                     strdup(request->client_application) : NULL;

    /* Load permissions into cache */
    load_user_permissions_to_cache(user->uuid);

    /* Register session */
    register_session(*session);

    /* Audit log */
    audit_session_created(*session, auth_result);

    return OK;
}
```

### Protocol-Specific Session Setup

c

```c
/*
 * PostgreSQL: Send AuthenticationOk and parameter status.
 */
int postgres_complete_authentication(PostgresConnectionState* conn,
                                     Session* session) {

    /* Send AuthenticationOk (R with code 0) */
    send_auth_result(conn, 0);

    /* Send BackendKeyData for cancel requests */
    send_backend_key_data(conn, session->session_id, session->cancel_key);

    /* Send ParameterStatus messages */
    send_parameter_status(conn, "server_version", get_server_version());
    send_parameter_status(conn, "server_encoding", "UTF8");
    send_parameter_status(conn, "client_encoding", 
                         conn->startup_params.client_encoding ?: "UTF8");
    send_parameter_status(conn, "DateStyle", "ISO, MDY");
    send_parameter_status(conn, "TimeZone", get_session_timezone(session));
    send_parameter_status(conn, "integer_datetimes", "on");
    send_parameter_status(conn, "standard_conforming_strings", "on");

    /* Send ReadyForQuery */
    send_ready_for_query(conn, 'I');  /* 'I' = idle */

    conn->phase = PHASE_READY;
    return OK;
}

/*
 * MySQL: Send OK packet.
 */
int mysql_complete_authentication(MySQLConnectionState* conn,
                                  Session* session) {

    MySQLOKPacket ok = {0};
    ok.header = 0x00;  /* OK header */
    ok.affected_rows = 0;
    ok.last_insert_id = 0;
    ok.status_flags = SERVER_STATUS_AUTOCOMMIT;
    ok.warnings = 0;

    send_mysql_packet(conn, &ok);

    conn->phase = PHASE_READY;
    return OK;
}

/*
 * MSSQL: Send LOGINACK and DONE tokens.
 */
int tds_complete_authentication(TDSConnectionState* conn,
                                Session* session) {

    TDSTokenStream tokens;
    init_token_stream(&tokens);

    /* LOGINACK token */
    TDSLoginAck loginack = {0};
    loginack.interface = 1;  /* SQL Server */
    loginack.tds_version = TDS_VERSION_7_4;
    strcpy(loginack.prog_name, "MyDB");
    loginack.prog_version = get_server_version_tds();
    add_token(&tokens, TDS_LOGINACK_TOKEN, &loginack);

    /* ENVCHANGE tokens for database, language, etc. */
    if (session->database) {
        add_envchange(&tokens, ENV_DATABASE, session->database, "master");
    }
    add_envchange(&tokens, ENV_LANGUAGE, "us_english", "");
    add_envchange(&tokens, ENV_PACKETSIZE, "4096", "4096");

    /* DONE token */
    TDSDone done = {0};
    done.status = DONE_FINAL;
    done.cur_cmd = 0;
    done.done_row_count = 0;
    add_token(&tokens, TDS_DONE_TOKEN, &done);

    send_tds_response(conn, &tokens);

    conn->phase = PHASE_READY;
    return OK;
}
```

## Outbound Connection Security (UDR)

### UDR as Client
```
When your database connects TO another database:
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│                    OUTBOUND CONNECTION REQUEST                   │
├─────────────────────────────────────────────────────────────────┤
│  UDR or replication needs to connect to external database       │
│  Credentials stored in sys.databases_credentials (encrypted)    │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│                    RETRIEVE CREDENTIALS                          │
├─────────────────────────────────────────────────────────────────┤
│  1. Lookup database entry by UUID or name                       │
│  2. Decrypt credentials using Credential Store Key              │
│  3. Load into secure memory                                     │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│                    ESTABLISH TLS CONNECTION                      │
├─────────────────────────────────────────────────────────────────┤
│  1. TCP connect                                                 │
│  2. TLS handshake (verify server certificate)                   │
│  3. Optional: present client certificate                        │
└─────────────────────────────────────────────────────────────────┘
    │
    v
┌─────────────────────────────────────────────────────────────────┐
│                    PROTOCOL-SPECIFIC AUTH                        │
├─────────────────────────────────────────────────────────────────┤
│  Speak the target protocol's authentication                     │
│  As a CLIENT, not a server                                      │
└─────────────────────────────────────────────────────────────────┘
```

### Outbound Connection Configuration

sql

```sql
CREATE TABLE sys.outbound_connection_config (
    connection_id           UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    connection_name         VARCHAR(128) NOT NULL UNIQUE,

    -- Target
    protocol                ENUM('POSTGRES', 'MYSQL', 'MSSQL', 'FIREBIRD') NOT NULL,
    hostname                VARCHAR(256) NOT NULL,
    port                    INTEGER NOT NULL,
    database                VARCHAR(128),

    -- Authentication
    auth_method             ENUM('PASSWORD', 'KERBEROS', 'CERTIFICATE') NOT NULL,
    username                VARCHAR(128),
    password_encrypted      BYTEA,  -- encrypted with CSK

    -- Kerberos
    service_principal       VARCHAR(256),
    keytab_path             VARCHAR(512),

    -- Client certificate
    client_cert_path        VARCHAR(512),
    client_key_path         VARCHAR(512),
    client_key_password_encrypted BYTEA,

    -- TLS settings
    tls_mode                ENUM('DISABLED', 'PREFERRED', 'REQUIRED', 'VERIFY_CA', 'VERIFY_FULL') NOT NULL DEFAULT 'VERIFY_FULL',
    ca_cert_path            VARCHAR(512),

    -- Connection pooling
    pool_min_size           INTEGER DEFAULT 1,
    pool_max_size           INTEGER DEFAULT 10,
    pool_idle_timeout_ms    INTEGER DEFAULT 60000,

    -- Timeouts
    connect_timeout_ms      INTEGER DEFAULT 5000,
    query_timeout_ms        INTEGER DEFAULT 30000,

    -- Status
    status                  ENUM('ACTIVE', 'DISABLED') NOT NULL DEFAULT 'ACTIVE',

    -- Audit
    created_at              TIMESTAMP NOT NULL DEFAULT now(),
    created_by              UUID
);
```

### Outbound Client Implementation

c

```c
/*
 * Connect to external database as client.
 */
int connect_to_external_database(const char* connection_name,
                                 ExternalConnection** conn) {

    /* Lookup configuration */
    OutboundConfig* config = get_outbound_config(connection_name);
    if (!config) {
        return ERR_CONNECTION_NOT_FOUND;
    }

    /* Allocate connection */
    *conn = allocate_external_connection();
    (*conn)->protocol = config->protocol;

    /* TCP connect */
    int sock = tcp_connect(config->hostname, config->port, 
                          config->connect_timeout_ms);
    if (sock < 0) {
        return ERR_CONNECT_FAILED;
    }
    (*conn)->socket = sock;

    /* TLS if required */
    if (config->tls_mode != TLS_DISABLED) {
        SSL_CTX* ctx = create_client_ssl_context();

        /* Load CA cert for server verification */
        if (config->ca_cert_path) {
            SSL_CTX_load_verify_locations(ctx, config->ca_cert_path, NULL);
        }

        /* Load client cert if using cert auth */
        if (config->auth_method == AUTH_CERTIFICATE) {
            SSL_CTX_use_certificate_file(ctx, config->client_cert_path, 
                                         SSL_FILETYPE_PEM);

            /* Decrypt client key password */
            char* key_password = decrypt_credential(
                config->client_key_password_encrypted);
            SSL_CTX_set_default_passwd_cb_userdata(ctx, key_password);

            SSL_CTX_use_PrivateKey_file(ctx, config->client_key_path,
                                        SSL_FILETYPE_PEM);

            secure_zero(key_password, strlen(key_password));
            free(key_password);
        }

        (*conn)->ssl = SSL_new(ctx);
        SSL_set_fd((*conn)->ssl, sock);

        /* For VERIFY_FULL, check hostname */
        if (config->tls_mode == TLS_VERIFY_FULL) {
            SSL_set1_host((*conn)->ssl, config->hostname);
        }

        /* Perform TLS handshake */
        int rc = client_tls_handshake(*conn, config);
        if (rc != OK) {
            close_external_connection(*conn);
            return rc;
        }
    }

    /* Protocol-specific authentication */
    int rc;
    switch (config->protocol) {
        case PROTO_POSTGRES:
            rc = postgres_client_authenticate(*conn, config);
            break;
        case PROTO_MYSQL:
            rc = mysql_client_authenticate(*conn, config);
            break;
        case PROTO_MSSQL:
            rc = tds_client_authenticate(*conn, config);
            break;
        case PROTO_FIREBIRD:
            rc = firebird_client_authenticate(*conn, config);
            break;
    }

    if (rc != OK) {
        close_external_connection(*conn);
        return rc;
    }

    return OK;
}

/*
 * PostgreSQL client authentication.
 */
int postgres_client_authenticate(ExternalConnection* conn,
                                 OutboundConfig* config) {

    /* Send StartupMessage */
    StartupMessage startup = {0};
    startup.protocol_version = PG_PROTOCOL_VERSION;
    add_startup_param(&startup, "user", config->username);
    if (config->database) {
        add_startup_param(&startup, "database", config->database);
    }
    add_startup_param(&startup, "application_name", "mydb_udr");

    send_startup_message(conn, &startup);

    /* Read server response */
    while (true) {
        char msg_type;
        void* msg_data;
        size_t msg_len;

        recv_pg_message(conn, &msg_type, &msg_data, &msg_len);

        switch (msg_type) {
            case 'R':  /* AuthenticationRequest */
                int auth_type = ntohl(*(int32_t*)msg_data);

                switch (auth_type) {
                    case AUTH_OK:
                        /* Authentication complete */
                        break;

                    case AUTH_CLEARTEXT_PASSWORD:
                        /* Send cleartext password */
                        char* password = decrypt_credential(
                            config->password_encrypted);
                        send_password_message(conn, password);
                        secure_zero(password, strlen(password));
                        free(password);
                        break;

                    case AUTH_MD5_PASSWORD:
                        /* Send MD5 hashed password */
                        uint8_t* salt = ((uint8_t*)msg_data) + 4;
                        char* password = decrypt_credential(
                            config->password_encrypted);
                        char* md5_hash = compute_pg_md5_password(
                            config->username, password, salt);
                        send_password_message(conn, md5_hash);
                        secure_zero(password, strlen(password));
                        free(password);
                        free(md5_hash);
                        break;

                    case AUTH_SASL:
                        /* SCRAM-SHA-256 */
                        return postgres_client_scram(conn, config);

                    case AUTH_GSSAPI:
                        return postgres_client_gssapi(conn, config);

                    default:
                        return ERR_UNSUPPORTED_AUTH;
                }
                break;

            case 'E':  /* ErrorResponse */
                parse_error_response(msg_data, &conn->last_error);
                return ERR_AUTH_FAILED;

            case 'S':  /* ParameterStatus */
                /* Store server parameters */
                parse_parameter_status(msg_data, conn);
                break;

            case 'K':  /* BackendKeyData */
                parse_backend_key(msg_data, conn);
                break;

            case 'Z':  /* ReadyForQuery */
                /* Connection established */
                conn->transaction_status = ((char*)msg_data)[0];
                return OK;
        }
    }
}
```

## Channel Binding

### Channel Binding for TLS

c

```c
/*
 * Channel binding ties authentication to the TLS channel,
 * preventing man-in-the-middle attacks.
 * 
 * PostgreSQL supports tls-server-end-point.
 * MSSQL supports Extended Protection.
 */

typedef enum ChannelBindingType {
    CB_NONE,
    CB_TLS_UNIQUE,              /* TLS 1.2 and earlier */
    CB_TLS_SERVER_END_POINT,    /* Certificate hash */
    CB_TLS_EXPORTER,            /* TLS 1.3 */
} ChannelBindingType;

int get_channel_binding_data(SSL* ssl, 
                             ChannelBindingType type,
                             uint8_t* binding_data,
                             size_t* binding_len) {

    switch (type) {
        case CB_TLS_UNIQUE:
            /* Get TLS Finished message */
            *binding_len = SSL_get_finished(ssl, binding_data, 64);
            if (*binding_len == 0) {
                return ERR_NO_CHANNEL_BINDING;
            }
            break;

        case CB_TLS_SERVER_END_POINT:
            /* Hash of server's certificate */
            X509* cert = SSL_get_peer_certificate(ssl);
            if (!cert) {
                return ERR_NO_CERTIFICATE;
            }

            /* Get signature algorithm to determine hash */
            int sig_nid = X509_get_signature_nid(cert);
            const EVP_MD* md;

            if (sig_nid == NID_md5 || sig_nid == NID_sha1) {
                md = EVP_sha256();  /* Upgrade weak hashes */
            } else {
                md = EVP_get_digestbynid(
                    OBJ_find_sigid_algs(sig_nid, NULL, NULL));
            }

            uint8_t cert_der[4096];
            int cert_len = i2d_X509(cert, NULL);
            uint8_t* p = cert_der;
            i2d_X509(cert, &p);

            EVP_Digest(cert_der, cert_len, binding_data, 
                      (unsigned int*)binding_len, md, NULL);

            X509_free(cert);
            break;

        case CB_TLS_EXPORTER:
            /* TLS 1.3 exporter */
            if (SSL_version(ssl) < TLS1_3_VERSION) {
                return ERR_TLS_VERSION_TOO_LOW;
            }

            const char* label = "EXPORTER-Channel-Binding";
            int rc = SSL_export_keying_material(
                ssl, binding_data, 32,
                label, strlen(label),
                NULL, 0, 0
            );
            if (rc != 1) {
                return ERR_EXPORT_FAILED;
            }
            *binding_len = 32;
            break;

        case CB_NONE:
        default:
            *binding_len = 0;
            break;
    }

    return OK;
}

/*
 * Verify channel binding in SCRAM authentication.
 */
int verify_scram_channel_binding(SCRAMState* state,
                                 const char* client_channel_binding,
                                 SSL* ssl) {

    /* Decode client's channel binding from base64 */
    uint8_t client_cb[64];
    size_t client_cb_len;
    base64_decode(client_channel_binding, client_cb, &client_cb_len);

    /* Parse gs2-header */
    /* "p=tls-server-end-point,," or "n,," or "y,," */
    char cb_type = client_cb[0];

    switch (cb_type) {
        case 'p':
            /* Client supports and sent channel binding */
            {
                /* Get our channel binding data */
                uint8_t server_cb[64];
                size_t server_cb_len;
                get_channel_binding_data(ssl, CB_TLS_SERVER_END_POINT,
                                        server_cb, &server_cb_len);

                /* Client sent "p=tls-server-end-point,,<data>" */
                /* Verify the data matches */
                uint8_t* client_cb_data = memchr(client_cb + 2, ',', 
                                                 client_cb_len - 2);
                client_cb_data++;  /* skip second comma */

                if (!constant_time_compare(client_cb_data, server_cb, 
                                          server_cb_len)) {
                    return ERR_CHANNEL_BINDING_MISMATCH;
                }
            }
            break;

        case 'y':
            /* Client supports but server didn't advertise */
            /* This is OK if we didn't require it */
            break;

        case 'n':
            /* Client doesn't support channel binding */
            if (policy_requires_channel_binding()) {
                return ERR_CHANNEL_BINDING_REQUIRED;
            }
            break;
    }

    return OK;
}
```

## Protocol Error Responses

### Unified Error Handling

c

```c
/*
 * Map internal errors to protocol-specific error responses.
 */
typedef struct ProtocolError {
    int                 internal_code;

    /* PostgreSQL */
    char                pg_severity[16];
    char                pg_code[6];      /* SQLSTATE */
    char                pg_message[256];

    /* MySQL */
    uint16_t            mysql_errno;
    char                mysql_sqlstate[6];
    char                mysql_message[256];

    /* MSSQL */
    uint32_t            tds_error_number;
    uint8_t             tds_state;
    uint8_t             tds_class;       /* severity */
    char                tds_message[256];

} ProtocolError;

static const ProtocolError error_mappings[] = {
    {
        .internal_code = ERR_AUTH_FAILED,
        .pg_severity = "FATAL", .pg_code = "28P01",
        .pg_message = "password authentication failed for user \"%s\"",
        .mysql_errno = 1045, .mysql_sqlstate = "28000",
        .mysql_message = "Access denied for user '%s'@'%s'",
        .tds_error_number = 18456, .tds_state = 1, .tds_class = 14,
        .tds_message = "Login failed for user '%s'",
    },
    {
        .internal_code = ERR_USER_NOT_FOUND,
        .pg_severity = "FATAL", .pg_code = "28000",
        .pg_message = "role \"%s\" does not exist",
        .mysql_errno = 1045, .mysql_sqlstate = "28000",
        .mysql_message = "Access denied for user '%s'@'%s'",
        .tds_error_number = 18456, .tds_state = 5, .tds_class = 14,
        .tds_message = "Login failed for user '%s'",
    },
    {
        .internal_code = ERR_USER_BLOCKED,
        .pg_severity = "FATAL", .pg_code = "28000",
        .pg_message = "role \"%s\" is blocked",
        .mysql_errno = 3118, .mysql_sqlstate = "HY000",
        .mysql_message = "Access denied for user '%s'. Account is blocked",
        .tds_error_number = 18486, .tds_state = 1, .tds_class = 14,
        .tds_message = "Login failed for user '%s'. Account is locked",
    },
    {
        .internal_code = ERR_TLS_REQUIRED,
        .pg_severity = "FATAL", .pg_code = "08P01",
        .pg_message = "SSL connection is required",
        .mysql_errno = 3159, .mysql_sqlstate = "HY000",
        .mysql_message = "Connections using insecure transport are prohibited",
        .tds_error_number = 20, .tds_state = 0, .tds_class = 20,
        .tds_message = "Encryption is required to connect to this server",
    },
    {
        .internal_code = ERR_DATABASE_NOT_FOUND,
        .pg_severity = "FATAL", .pg_code = "3D000",
        .pg_message = "database \"%s\" does not exist",
        .mysql_errno = 1049, .mysql_sqlstate = "42000",
        .mysql_message = "Unknown database '%s'",
        .tds_error_number = 4060, .tds_state = 1, .tds_class = 11,
        .tds_message = "Cannot open database \"%s\"",
    },
    /* ... more mappings ... */
};

int send_protocol_error(void* conn_state, 
                        WireProtocol protocol,
                        int internal_code,
                        const char* detail) {

    const ProtocolError* err = find_error_mapping(internal_code);
    if (!err) {
        err = &generic_error;
    }

    switch (protocol) {
        case PROTO_POSTGRES:
            return send_pg_error(conn_state, err, detail);

        case PROTO_MYSQL:
            return send_mysql_error(conn_state, err, detail);

        case PROTO_MSSQL:
            return send_tds_error(conn_state, err, detail);

        case PROTO_FIREBIRD:
            return send_fb_error(conn_state, err, detail);
    }
}

int send_pg_error(PostgresConnectionState* conn,
                  const ProtocolError* err,
                  const char* detail) {

    ErrorResponse resp;
    init_error_response(&resp);

    add_error_field(&resp, 'S', err->pg_severity);
    add_error_field(&resp, 'V', err->pg_severity);  /* non-localized */
    add_error_field(&resp, 'C', err->pg_code);

    /* Format message with detail */
    char message[512];
    snprintf(message, sizeof(message), err->pg_message, detail);
    add_error_field(&resp, 'M', message);

    send_pg_message(conn, 'E', &resp);

    return OK;
}
```

## Session Security Properties

### Session Security Context

sql

```sql
CREATE TABLE sys.session_security_context (
    session_id              UUID PRIMARY KEY,

    -- Authentication info
    auth_method             VARCHAR(64) NOT NULL,
    auth_provider_id        UUID REFERENCES sys.auth_providers(provider_id),
    authenticated_at        TIMESTAMP NOT NULL,
    auth_expiry             TIMESTAMP,

    -- TLS info
    tls_version             VARCHAR(16),
    tls_cipher              VARCHAR(64),
    tls_certificate_cn      VARCHAR(256),
    tls_certificate_issuer  VARCHAR(256),

    -- Channel binding
    channel_binding_used    BOOLEAN DEFAULT FALSE,
    channel_binding_type    VARCHAR(32),

    -- Kerberos/GSSAPI context
    gss_principal           VARCHAR(256),
    gss_delegated_creds     BOOLEAN DEFAULT FALSE,
    gss_integrity           BOOLEAN DEFAULT FALSE,
    gss_confidentiality     BOOLEAN DEFAULT FALSE,

    -- Protocol info
    wire_protocol           VARCHAR(16) NOT NULL,
    protocol_version        VARCHAR(32),

    -- Derived security level
    security_level          ENUM('MINIMAL', 'BASIC', 'STANDARD', 'HIGH', 'MAXIMUM') NOT NULL,

    FOREIGN KEY (session_id) REFERENCES sys.session_cache(session_id)
);
```

### Security Level Calculation

c

```c
/*
 * Calculate session security level based on auth and encryption.
 */
SecurityLevel calculate_session_security_level(SessionSecurityContext* ctx) {
    int score = 0;

    /* TLS contribution */
    if (ctx->tls_version) {
        if (strcmp(ctx->tls_version, "TLSv1.3") == 0) {
            score += 30;
        } else if (strcmp(ctx->tls_version, "TLSv1.2") == 0) {
            score += 20;
        } else {
            score += 5;  /* Older TLS */
        }

        /* Strong ciphers */
        if (is_strong_cipher(ctx->tls_cipher)) {
            score += 10;
        }
    }

    /* Auth method contribution */
    switch (ctx->auth_method_type) {
        case AUTH_TRUST:
            score += 0;
            break;
        case AUTH_PASSWORD_CLEARTEXT:
            score += 5;
            break;
        case AUTH_PASSWORD_MD5:
            score += 10;
            break;
        case AUTH_PASSWORD_SCRAM:
            score += 25;
            break;
        case AUTH_CERTIFICATE:
            score += 30;
            break;
        case AUTH_KERBEROS:
            score += 30;
            break;
    }

    /* Channel binding bonus */
    if (ctx->channel_binding_used) {
        score += 15;
    }

    /* GSSAPI encryption */
    if (ctx->gss_confidentiality) {
        score += 10;
    }

    /* Map score to level */
    if (score >= 70) return SECURITY_MAXIMUM;
    if (score >= 55) return SECURITY_HIGH;
    if (score >= 35) return SECURITY_STANDARD;
    if (score >= 15) return SECURITY_BASIC;
    return SECURITY_MINIMAL;
}
```

### Protocol Security Policies

sql

```sql
-- Per-protocol security requirements
CREATE TABLE sys.protocol_security_policy (
    policy_id               UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    protocol                ENUM('POSTGRES', 'MYSQL', 'MSSQL', 'FIREBIRD', 'ANY') NOT NULL,

    -- TLS requirements
    require_tls             BOOLEAN DEFAULT TRUE,
    min_tls_version         ENUM('TLS10', 'TLS11', 'TLS12', 'TLS13') DEFAULT 'TLS12',
    require_client_cert     BOOLEAN DEFAULT FALSE,

    -- Auth requirements
    allowed_auth_methods    VARCHAR(256)[],  -- e.g., ['SCRAM-SHA-256', 'GSSAPI']
    prohibited_auth_methods VARCHAR(256)[],  -- e.g., ['MD5', 'CLEARTEXT']

    -- Channel binding
    require_channel_binding BOOLEAN DEFAULT FALSE,

    -- Minimum security level
    min_security_level      ENUM('MINIMAL', 'BASIC', 'STANDARD', 'HIGH', 'MAXIMUM') DEFAULT 'STANDARD',

    -- Source restrictions
    allowed_sources         INET[],  -- NULL = any
    blocked_sources         INET[],

    -- Time restrictions
    allowed_hours_start     TIME,
    allowed_hours_end       TIME,
    allowed_days            INTEGER[],  -- 0=Sun, 1=Mon, etc.

    -- Status
    status                  ENUM('ACTIVE', 'DISABLED') NOT NULL DEFAULT 'ACTIVE',
    priority                INTEGER NOT NULL DEFAULT 100,

    UNIQUE (protocol, priority),
    INDEX idx_protocol (protocol)
);
```

## Audit Integration

### Protocol-Level Audit Events

sql

```sql
CREATE TABLE sys.protocol_audit_log (
    audit_id                UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    event_timestamp         TIMESTAMP NOT NULL DEFAULT now(),

    -- Protocol info
    protocol                VARCHAR(16) NOT NULL,
    protocol_version        VARCHAR(32),

    -- Connection info
    client_address          INET NOT NULL,
    client_port             INTEGER,
    server_address          INET,
    server_port             INTEGER,

    -- Event type
    event_type              ENUM(
                                'CONNECTION_ATTEMPT',
                                'TLS_HANDSHAKE',
                                'TLS_HANDSHAKE_FAILED',
                                'AUTH_START',
                                'AUTH_CHALLENGE',
                                'AUTH_RESPONSE',
                                'AUTH_SUCCESS',
                                'AUTH_FAILURE',
                                'SESSION_START',
                                'SESSION_END',
                                'PROTOCOL_ERROR'
                            ) NOT NULL,

    -- User info (if known)
    username                VARCHAR(128),
    user_uuid               UUID,

    -- Event details
    auth_method             VARCHAR(64),
    tls_version             VARCHAR(16),
    tls_cipher              VARCHAR(64),
    error_code              INTEGER,
    error_message           TEXT,

    -- Session (if established)
    session_id              UUID,

    -- Additional context
    details                 JSONB,

    INDEX idx_timestamp (event_timestamp),
    INDEX idx_client (client_address, event_timestamp),
    INDEX idx_user (username, event_timestamp),
    INDEX idx_event (event_type, event_timestamp)
);
```

## Policy Summary

| Policy Key                                 | Type     | Default                   | Description                    |
| ------------------------------------------ | -------- | ------------------------- | ------------------------------ |
| `protocol.postgres.enabled`                | BOOLEAN  | TRUE                      | Enable PostgreSQL protocol     |
| `protocol.postgres.port`                   | INTEGER  | 5432                      | PostgreSQL listener port       |
| `protocol.postgres.tls_mode`               | ENUM     | PREFERRED                 | TLS requirement                |
| `protocol.postgres.auth_methods`           | STRING[] | ['scram-sha-256','md5']   | Allowed auth methods           |
| `protocol.mysql.enabled`                   | BOOLEAN  | TRUE                      | Enable MySQL protocol          |
| `protocol.mysql.port`                      | INTEGER  | 3306                      | MySQL listener port            |
| `protocol.mysql.tls_mode`                  | ENUM     | PREFERRED                 | TLS requirement                |
| `protocol.mysql.auth_plugins`              | STRING[] | ['caching_sha2_password'] | Allowed auth plugins           |
| `protocol.mssql.enabled`                   | BOOLEAN  | TRUE                      | Enable MSSQL/TDS protocol      |
| `protocol.mssql.port`                      | INTEGER  | 1433                      | MSSQL listener port            |
| `protocol.mssql.tls_mode`                  | ENUM     | REQUIRED                  | TLS requirement                |
| `protocol.firebird.enabled`                | BOOLEAN  | TRUE                      | Enable Firebird protocol       |
| `protocol.firebird.port`                   | INTEGER  | 3050                      | Firebird listener port         |
| `protocol.default.min_tls_version`         | ENUM     | TLS12                     | Minimum TLS version            |
| `protocol.default.require_channel_binding` | BOOLEAN  | FALSE                     | Require channel binding        |
| `protocol.default.min_security_level`      | ENUM     | STANDARD                  | Minimum session security       |
| `protocol.password.verify_strategy`        | ENUM     | PREFER_SCRAM              | Password verification strategy |
| `protocol.password.store_multi_format`     | BOOLEAN  | FALSE                     | Store multiple hash formats    |
| `protocol.outbound.default_tls_mode`       | ENUM     | VERIFY_FULL               | TLS for outbound connections   |
| `protocol.outbound.connect_timeout_ms`     | INTEGER  | 5000                      | Outbound connect timeout       |

Security audit and compliance reporting ties together all the subsystems we've designed. This provides the unified visibility needed for compliance certifications, incident response, and forensic analysis.

## Audit Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           AUDIT EVENT SOURCES                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │   Auth      │ │  Cluster    │ │  Encryption │ │  Protocol   │           │
│  │   Events    │ │   Events    │ │   Events    │ │   Events    │           │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘           │
│         │               │               │               │                   │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │    DDL      │ │    DML      │ │   Access    │ │   Admin     │           │
│  │   Events    │ │   Events    │ │   Events    │ │   Events    │           │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘           │
│         │               │               │               │                   │
│         └───────────────┴───────┬───────┴───────────────┘                   │
│                                 │                                           │
│                                 v                                           │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                      AUDIT EVENT COLLECTOR                            │  │
│  │                                                                       │  │
│  │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │  │
│  │   │  Normalize  │  │   Enrich    │  │   Filter    │                  │  │
│  │   │   Event     │→ │   Context   │→ │  by Policy  │                  │  │
│  │   └─────────────┘  └─────────────┘  └─────────────┘                  │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                 │                                           │
│                    ┌────────────┴────────────┐                             │
│                    v                         v                              │
│  ┌─────────────────────────────┐  ┌─────────────────────────────┐          │
│  │      AUDIT LOG STORE        │  │    REAL-TIME STREAMING      │          │
│  │                             │  │                             │          │
│  │  ┌─────────┐ ┌─────────┐   │  │  ┌─────────┐ ┌─────────┐   │          │
│  │  │ Active  │ │ Archive │   │  │  │  SIEM   │ │ Alerts  │   │          │
│  │  │  Logs   │ │  Logs   │   │  │  │ Export  │ │ Engine  │   │          │
│  │  └─────────┘ └─────────┘   │  │  └─────────┘ └─────────┘   │          │
│  └─────────────────────────────┘  └─────────────────────────────┘          │
│                    │                         │                              │
│                    v                         v                              │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     COMPLIANCE REPORTING                             │   │
│  │                                                                      │   │
│  │   ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │   │
│  │   │  SOC 2   │ │  HIPAA   │ │ PCI-DSS  │ │   GDPR   │ │  Custom  │  │   │
│  │   │ Reports  │ │ Reports  │ │ Reports  │ │ Reports  │ │ Reports  │  │   │
│  │   └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Unified Audit Event Schema

### Core Event Structure

sql

```sql
CREATE TABLE sys.audit_events (
    -- Event identification
    event_id                UUID PRIMARY KEY,
    event_sequence          BIGINT NOT NULL,  -- monotonic within database
    event_timestamp         TIMESTAMP NOT NULL,
    event_timestamp_utc     TIMESTAMP NOT NULL,

    -- Event classification
    event_category          audit_category NOT NULL,
    event_type              VARCHAR(64) NOT NULL,
    event_subtype           VARCHAR(64),
    event_severity          audit_severity NOT NULL,

    -- Outcome
    event_outcome           ENUM('SUCCESS', 'FAILURE', 'UNKNOWN') NOT NULL,
    error_code              INTEGER,
    error_message           TEXT,

    -- Actor (who)
    actor_type              ENUM('USER', 'SYSTEM', 'REPLICATION', 'SCHEDULER', 'EXTERNAL') NOT NULL,
    actor_user_uuid         UUID,
    actor_username          VARCHAR(128),
    actor_role_uuid         UUID,
    actor_role_name         VARCHAR(128),
    actor_application       VARCHAR(256),
    actor_session_id        UUID,

    -- Source (where from)
    source_address          INET,
    source_port             INTEGER,
    source_hostname         VARCHAR(256),
    source_protocol         VARCHAR(16),

    -- Target (what)
    target_type             VARCHAR(64),  -- DATABASE, TABLE, USER, KEY, etc.
    target_uuid             UUID,
    target_name             VARCHAR(256),
    target_database_uuid    UUID,
    target_database_name    VARCHAR(128),
    target_schema_name      VARCHAR(128),

    -- Action details
    action_statement        TEXT,  -- SQL statement (may be truncated/redacted)
    action_parameters       JSONB, -- structured action parameters

    -- Result details
    result_rows_affected    BIGINT,
    result_details          JSONB,

    -- Context
    transaction_id          UUID,
    transaction_state       VARCHAR(16),
    security_context        JSONB,  -- TLS, auth method, etc.

    -- Cluster context
    cluster_uuid            UUID,
    database_uuid           UUID NOT NULL,
    node_uuid               UUID NOT NULL,
    node_role               VARCHAR(32),  -- PRIMARY, REPLICA, SECURITY_DB

    -- Tamper detection
    event_hash              BYTEA NOT NULL,
    previous_event_hash     BYTEA,  -- chain hash for integrity

    -- Indexing
    INDEX idx_timestamp (event_timestamp),
    INDEX idx_category_type (event_category, event_type, event_timestamp),
    INDEX idx_actor (actor_user_uuid, event_timestamp),
    INDEX idx_target (target_type, target_uuid, event_timestamp),
    INDEX idx_outcome (event_outcome, event_timestamp),
    INDEX idx_severity (event_severity, event_timestamp),
    INDEX idx_session (actor_session_id, event_timestamp),
    INDEX idx_transaction (transaction_id)
);

-- Partitioning by time for manageability
-- Implementation: PARTITION BY RANGE (event_timestamp)
```

### Event Categories and Types

sql

```sql
CREATE TYPE audit_category AS ENUM (
    'AUTHENTICATION',
    'AUTHORIZATION',
    'DATA_ACCESS',
    'DATA_MODIFICATION',
    'SCHEMA_CHANGE',
    'SECURITY_ADMIN',
    'SYSTEM_ADMIN',
    'CLUSTER_ADMIN',
    'ENCRYPTION',
    'REPLICATION',
    'BACKUP_RESTORE',
    'POLICY_CHANGE',
    'COMPLIANCE',
    'SYSTEM'
);

CREATE TYPE audit_severity AS ENUM (
    'DEBUG',       -- Detailed debugging info
    'INFO',        -- Normal operations
    'NOTICE',      -- Notable but normal events
    'WARNING',     -- Potential issues
    'ERROR',       -- Operation failures
    'CRITICAL',    -- Security-relevant failures
    'ALERT',       -- Immediate attention required
    'EMERGENCY'    -- System-wide security breach
);

-- Event type taxonomy
CREATE TABLE sys.audit_event_types (
    event_category          audit_category NOT NULL,
    event_type              VARCHAR(64) NOT NULL,
    event_subtype           VARCHAR(64),

    description             TEXT,
    default_severity        audit_severity NOT NULL,

    -- Compliance mapping
    soc2_control            VARCHAR(32)[],
    hipaa_control           VARCHAR(32)[],
    pci_dss_requirement     VARCHAR(32)[],
    gdpr_article            VARCHAR(32)[],

    -- Retention requirements
    min_retention_days      INTEGER NOT NULL DEFAULT 90,
    compliance_critical     BOOLEAN DEFAULT FALSE,

    PRIMARY KEY (event_category, event_type, event_subtype)
);

-- Seed event types
INSERT INTO sys.audit_event_types VALUES
-- Authentication events
('AUTHENTICATION', 'LOGIN_ATTEMPT', NULL, 
    'User login attempt', 'INFO',
    ARRAY['CC6.1'], ARRAY['164.312(d)'], ARRAY['8.2.1'], ARRAY['Article 32'],
    90, TRUE),
('AUTHENTICATION', 'LOGIN_SUCCESS', NULL,
    'Successful user login', 'INFO',
    ARRAY['CC6.1'], ARRAY['164.312(d)'], ARRAY['8.2.1'], ARRAY['Article 32'],
    90, TRUE),
('AUTHENTICATION', 'LOGIN_FAILURE', NULL,
    'Failed user login', 'WARNING',
    ARRAY['CC6.1', 'CC6.8'], ARRAY['164.312(d)'], ARRAY['8.2.1', '10.2.4'], ARRAY['Article 32'],
    365, TRUE),
('AUTHENTICATION', 'LOGOUT', NULL,
    'User logout', 'INFO',
    ARRAY['CC6.1'], NULL, ARRAY['8.2.1'], NULL,
    90, FALSE),
('AUTHENTICATION', 'SESSION_TIMEOUT', NULL,
    'Session expired', 'INFO',
    ARRAY['CC6.1'], NULL, ARRAY['8.2.1'], NULL,
    90, FALSE),
('AUTHENTICATION', 'PASSWORD_CHANGE', NULL,
    'User password changed', 'NOTICE',
    ARRAY['CC6.1'], ARRAY['164.312(d)'], ARRAY['8.2.4'], ARRAY['Article 32'],
    365, TRUE),
('AUTHENTICATION', 'ACCOUNT_LOCKED', NULL,
    'Account locked due to failures', 'WARNING',
    ARRAY['CC6.1', 'CC6.8'], ARRAY['164.312(d)'], ARRAY['8.2.1'], ARRAY['Article 32'],
    365, TRUE),
('AUTHENTICATION', 'ACCOUNT_UNLOCKED', NULL,
    'Account unlocked by admin', 'NOTICE',
    ARRAY['CC6.1'], ARRAY['164.312(d)'], ARRAY['8.2.1'], ARRAY['Article 32'],
    365, TRUE),

-- Authorization events
('AUTHORIZATION', 'PRIVILEGE_GRANTED', NULL,
    'Privilege granted to user/role', 'NOTICE',
    ARRAY['CC6.3'], ARRAY['164.312(a)(1)'], ARRAY['7.1', '7.2'], ARRAY['Article 32'],
    365, TRUE),
('AUTHORIZATION', 'PRIVILEGE_REVOKED', NULL,
    'Privilege revoked from user/role', 'NOTICE',
    ARRAY['CC6.3'], ARRAY['164.312(a)(1)'], ARRAY['7.1', '7.2'], ARRAY['Article 32'],
    365, TRUE),
('AUTHORIZATION', 'ROLE_ASSIGNED', NULL,
    'Role assigned to user', 'NOTICE',
    ARRAY['CC6.3'], ARRAY['164.312(a)(1)'], ARRAY['7.1'], ARRAY['Article 32'],
    365, TRUE),
('AUTHORIZATION', 'ROLE_REMOVED', NULL,
    'Role removed from user', 'NOTICE',
    ARRAY['CC6.3'], ARRAY['164.312(a)(1)'], ARRAY['7.1'], ARRAY['Article 32'],
    365, TRUE),
('AUTHORIZATION', 'ACCESS_DENIED', NULL,
    'Access denied due to insufficient privileges', 'WARNING',
    ARRAY['CC6.3'], ARRAY['164.312(a)(1)'], ARRAY['7.1'], ARRAY['Article 32'],
    180, TRUE),

-- Data access events
('DATA_ACCESS', 'SELECT', NULL,
    'Data read operation', 'DEBUG',
    ARRAY['CC6.1'], ARRAY['164.312(b)'], ARRAY['10.2.1'], ARRAY['Article 32'],
    30, FALSE),
('DATA_ACCESS', 'SELECT', 'SENSITIVE',
    'Sensitive data read operation', 'INFO',
    ARRAY['CC6.1'], ARRAY['164.312(b)'], ARRAY['10.2.1'], ARRAY['Article 32'],
    365, TRUE),
('DATA_ACCESS', 'EXPORT', NULL,
    'Data export operation', 'NOTICE',
    ARRAY['CC6.1', 'CC6.7'], ARRAY['164.312(b)'], ARRAY['10.2.1'], ARRAY['Article 32'],
    365, TRUE),

-- Data modification events
('DATA_MODIFICATION', 'INSERT', NULL,
    'Data insert operation', 'DEBUG',
    ARRAY['CC6.1'], ARRAY['164.312(b)'], ARRAY['10.2.1'], NULL,
    30, FALSE),
('DATA_MODIFICATION', 'UPDATE', NULL,
    'Data update operation', 'DEBUG',
    ARRAY['CC6.1'], ARRAY['164.312(b)'], ARRAY['10.2.1'], NULL,
    30, FALSE),
('DATA_MODIFICATION', 'DELETE', NULL,
    'Data delete operation', 'INFO',
    ARRAY['CC6.1'], ARRAY['164.312(b)'], ARRAY['10.2.1'], ARRAY['Article 17'],
    90, FALSE),
('DATA_MODIFICATION', 'TRUNCATE', NULL,
    'Table truncate operation', 'NOTICE',
    ARRAY['CC6.1'], ARRAY['164.312(b)'], ARRAY['10.2.1'], ARRAY['Article 17'],
    365, TRUE),

-- Schema change events
('SCHEMA_CHANGE', 'CREATE_TABLE', NULL,
    'Table created', 'INFO',
    ARRAY['CC6.1', 'CC8.1'], NULL, ARRAY['10.2.7'], NULL,
    365, FALSE),
('SCHEMA_CHANGE', 'ALTER_TABLE', NULL,
    'Table modified', 'INFO',
    ARRAY['CC6.1', 'CC8.1'], NULL, ARRAY['10.2.7'], NULL,
    365, FALSE),
('SCHEMA_CHANGE', 'DROP_TABLE', NULL,
    'Table dropped', 'NOTICE',
    ARRAY['CC6.1', 'CC8.1'], NULL, ARRAY['10.2.7'], NULL,
    365, TRUE),
('SCHEMA_CHANGE', 'CREATE_INDEX', NULL,
    'Index created', 'INFO',
    ARRAY['CC6.1'], NULL, ARRAY['10.2.7'], NULL,
    180, FALSE),
('SCHEMA_CHANGE', 'DROP_INDEX', NULL,
    'Index dropped', 'INFO',
    ARRAY['CC6.1'], NULL, ARRAY['10.2.7'], NULL,
    180, FALSE),

-- Security admin events
('SECURITY_ADMIN', 'USER_CREATED', NULL,
    'User account created', 'NOTICE',
    ARRAY['CC6.2'], ARRAY['164.312(a)(1)'], ARRAY['8.1.1'], ARRAY['Article 32'],
    365, TRUE),
('SECURITY_ADMIN', 'USER_MODIFIED', NULL,
    'User account modified', 'NOTICE',
    ARRAY['CC6.2'], ARRAY['164.312(a)(1)'], ARRAY['8.1.1'], ARRAY['Article 32'],
    365, TRUE),
('SECURITY_ADMIN', 'USER_DELETED', NULL,
    'User account deleted', 'NOTICE',
    ARRAY['CC6.2'], ARRAY['164.312(a)(1)'], ARRAY['8.1.4'], ARRAY['Article 32'],
    365, TRUE),
('SECURITY_ADMIN', 'USER_BLOCKED', NULL,
    'User account blocked', 'WARNING',
    ARRAY['CC6.2'], ARRAY['164.312(a)(1)'], ARRAY['8.1.4'], ARRAY['Article 32'],
    365, TRUE),
('SECURITY_ADMIN', 'ROLE_CREATED', NULL,
    'Security role created', 'NOTICE',
    ARRAY['CC6.3'], ARRAY['164.312(a)(1)'], ARRAY['7.1'], ARRAY['Article 32'],
    365, TRUE),
('SECURITY_ADMIN', 'ROLE_MODIFIED', NULL,
    'Security role modified', 'NOTICE',
    ARRAY['CC6.3'], ARRAY['164.312(a)(1)'], ARRAY['7.1'], ARRAY['Article 32'],
    365, TRUE),
('SECURITY_ADMIN', 'ROLE_DELETED', NULL,
    'Security role deleted', 'NOTICE',
    ARRAY['CC6.3'], ARRAY['164.312(a)(1)'], ARRAY['7.1'], ARRAY['Article 32'],
    365, TRUE),
('SECURITY_ADMIN', 'AUTH_PROVIDER_REGISTERED', NULL,
    'Authentication provider registered', 'NOTICE',
    ARRAY['CC6.1'], ARRAY['164.312(d)'], ARRAY['8.2'], ARRAY['Article 32'],
    365, TRUE),
('SECURITY_ADMIN', 'AUTH_PROVIDER_MODIFIED', NULL,
    'Authentication provider modified', 'NOTICE',
    ARRAY['CC6.1'], ARRAY['164.312(d)'], ARRAY['8.2'], ARRAY['Article 32'],
    365, TRUE),
('SECURITY_ADMIN', 'AUTH_PROVIDER_DISABLED', NULL,
    'Authentication provider disabled', 'WARNING',
    ARRAY['CC6.1'], ARRAY['164.312(d)'], ARRAY['8.2'], ARRAY['Article 32'],
    365, TRUE),

-- Cluster admin events
('CLUSTER_ADMIN', 'SECURITY_DB_ADDED', NULL,
    'Security database added to cluster', 'NOTICE',
    ARRAY['CC6.6', 'A1.2'], NULL, ARRAY['10.2.7'], NULL,
    365, TRUE),
('CLUSTER_ADMIN', 'SECURITY_DB_REMOVED', NULL,
    'Security database removed from cluster', 'WARNING',
    ARRAY['CC6.6', 'A1.2'], NULL, ARRAY['10.2.7'], NULL,
    365, TRUE),
('CLUSTER_ADMIN', 'DATABASE_REGISTERED', NULL,
    'Database registered in cluster', 'INFO',
    ARRAY['CC6.6'], NULL, ARRAY['10.2.7'], NULL,
    365, FALSE),
('CLUSTER_ADMIN', 'DATABASE_DEREGISTERED', NULL,
    'Database deregistered from cluster', 'NOTICE',
    ARRAY['CC6.6'], NULL, ARRAY['10.2.7'], NULL,
    365, TRUE),
('CLUSTER_ADMIN', 'QUORUM_LOST', NULL,
    'Cluster lost quorum', 'ALERT',
    ARRAY['A1.2'], NULL, ARRAY['10.2.6'], NULL,
    365, TRUE),
('CLUSTER_ADMIN', 'QUORUM_RESTORED', NULL,
    'Cluster quorum restored', 'NOTICE',
    ARRAY['A1.2'], NULL, ARRAY['10.2.6'], NULL,
    365, TRUE),
('CLUSTER_ADMIN', 'FENCING_ACTIVATED', NULL,
    'Node entered fenced mode', 'WARNING',
    ARRAY['A1.2'], NULL, ARRAY['10.2.6'], NULL,
    365, TRUE),
('CLUSTER_ADMIN', 'FENCING_DEACTIVATED', NULL,
    'Node exited fenced mode', 'INFO',
    ARRAY['A1.2'], NULL, ARRAY['10.2.6'], NULL,
    365, TRUE),
('CLUSTER_ADMIN', 'POLICY_CHANGED', NULL,
    'Cluster policy changed', 'NOTICE',
    ARRAY['CC6.1'], NULL, ARRAY['10.2.7'], NULL,
    365, TRUE),

-- Encryption events
('ENCRYPTION', 'KEY_GENERATED', NULL,
    'Encryption key generated', 'INFO',
    ARRAY['CC6.1', 'CC6.7'], ARRAY['164.312(a)(2)(iv)'], ARRAY['3.5', '3.6'], ARRAY['Article 32'],
    365, TRUE),
('ENCRYPTION', 'KEY_ROTATED', NULL,
    'Encryption key rotated', 'INFO',
    ARRAY['CC6.1', 'CC6.7'], ARRAY['164.312(a)(2)(iv)'], ARRAY['3.5', '3.6'], ARRAY['Article 32'],
    365, TRUE),
('ENCRYPTION', 'KEY_EXPORTED', NULL,
    'Encryption key exported', 'WARNING',
    ARRAY['CC6.1', 'CC6.7'], ARRAY['164.312(a)(2)(iv)'], ARRAY['3.5', '3.6'], ARRAY['Article 32'],
    365, TRUE),
('ENCRYPTION', 'KEY_IMPORTED', NULL,
    'Encryption key imported', 'NOTICE',
    ARRAY['CC6.1', 'CC6.7'], ARRAY['164.312(a)(2)(iv)'], ARRAY['3.5', '3.6'], ARRAY['Article 32'],
    365, TRUE),
('ENCRYPTION', 'KEY_DESTROYED', NULL,
    'Encryption key destroyed', 'WARNING',
    ARRAY['CC6.1', 'CC6.7'], ARRAY['164.312(a)(2)(iv)'], ARRAY['3.5', '3.6'], ARRAY['Article 32'],
    365, TRUE),
('ENCRYPTION', 'DATABASE_ENCRYPTED', NULL,
    'Database encryption enabled', 'NOTICE',
    ARRAY['CC6.7'], ARRAY['164.312(a)(2)(iv)'], ARRAY['3.4'], ARRAY['Article 32'],
    365, TRUE),
('ENCRYPTION', 'DATABASE_DECRYPTED', NULL,
    'Database encryption disabled', 'ALERT',
    ARRAY['CC6.7'], ARRAY['164.312(a)(2)(iv)'], ARRAY['3.4'], ARRAY['Article 32'],
    365, TRUE),
('ENCRYPTION', 'HSM_CONNECTED', NULL,
    'HSM connection established', 'INFO',
    ARRAY['CC6.7'], ARRAY['164.312(a)(2)(iv)'], ARRAY['3.5'], ARRAY['Article 32'],
    365, FALSE),
('ENCRYPTION', 'HSM_DISCONNECTED', NULL,
    'HSM connection lost', 'WARNING',
    ARRAY['CC6.7'], ARRAY['164.312(a)(2)(iv)'], ARRAY['3.5'], ARRAY['Article 32'],
    365, TRUE),
('ENCRYPTION', 'KMS_CONNECTED', NULL,
    'External KMS connection established', 'INFO',
    ARRAY['CC6.7'], NULL, ARRAY['3.5'], ARRAY['Article 32'],
    365, FALSE),
('ENCRYPTION', 'KMS_DISCONNECTED', NULL,
    'External KMS connection lost', 'WARNING',
    ARRAY['CC6.7'], NULL, ARRAY['3.5'], ARRAY['Article 32'],
    365, TRUE),

-- Backup/restore events
('BACKUP_RESTORE', 'BACKUP_STARTED', NULL,
    'Backup operation started', 'INFO',
    ARRAY['A1.2'], ARRAY['164.308(a)(7)'], ARRAY['9.5'], NULL,
    365, TRUE),
('BACKUP_RESTORE', 'BACKUP_COMPLETED', NULL,
    'Backup operation completed', 'INFO',
    ARRAY['A1.2'], ARRAY['164.308(a)(7)'], ARRAY['9.5'], NULL,
    365, TRUE),
('BACKUP_RESTORE', 'BACKUP_FAILED', NULL,
    'Backup operation failed', 'ERROR',
    ARRAY['A1.2'], ARRAY['164.308(a)(7)'], ARRAY['9.5'], NULL,
    365, TRUE),
('BACKUP_RESTORE', 'RESTORE_STARTED', NULL,
    'Restore operation started', 'NOTICE',
    ARRAY['A1.2'], ARRAY['164.308(a)(7)'], ARRAY['9.5'], NULL,
    365, TRUE),
('BACKUP_RESTORE', 'RESTORE_COMPLETED', NULL,
    'Restore operation completed', 'NOTICE',
    ARRAY['A1.2'], ARRAY['164.308(a)(7)'], ARRAY['9.5'], NULL,
    365, TRUE),
('BACKUP_RESTORE', 'RESTORE_FAILED', NULL,
    'Restore operation failed', 'ERROR',
    ARRAY['A1.2'], ARRAY['164.308(a)(7)'], ARRAY['9.5'], NULL,
    365, TRUE),

-- Compliance-specific events
('COMPLIANCE', 'SENSITIVE_DATA_ACCESSED', NULL,
    'Sensitive/PII data accessed', 'INFO',
    ARRAY['CC6.1'], ARRAY['164.312(b)'], ARRAY['10.2.1'], ARRAY['Article 32', 'Article 30'],
    365, TRUE),
('COMPLIANCE', 'DATA_EXPORTED', NULL,
    'Data exported from system', 'NOTICE',
    ARRAY['CC6.7'], ARRAY['164.312(b)'], ARRAY['10.2.1'], ARRAY['Article 32'],
    365, TRUE),
('COMPLIANCE', 'DATA_ANONYMIZED', NULL,
    'Data anonymization performed', 'INFO',
    NULL, NULL, NULL, ARRAY['Article 17', 'Article 25'],
    365, TRUE),
('COMPLIANCE', 'DATA_RETENTION_APPLIED', NULL,
    'Data retention policy applied', 'INFO',
    ARRAY['CC6.5'], NULL, ARRAY['3.1'], ARRAY['Article 5', 'Article 17'],
    365, TRUE),
('COMPLIANCE', 'RIGHT_TO_ERASURE', NULL,
    'Right to erasure (GDPR Art. 17) exercised', 'NOTICE',
    NULL, NULL, NULL, ARRAY['Article 17'],
    365, TRUE),
('COMPLIANCE', 'DATA_PORTABILITY', NULL,
    'Data portability (GDPR Art. 20) exercised', 'NOTICE',
    NULL, NULL, NULL, ARRAY['Article 20'],
    365, TRUE),
('COMPLIANCE', 'CONSENT_RECORDED', NULL,
    'User consent recorded', 'INFO',
    NULL, NULL, NULL, ARRAY['Article 6', 'Article 7'],
    365, TRUE),
('COMPLIANCE', 'CONSENT_WITHDRAWN', NULL,
    'User consent withdrawn', 'NOTICE',
    NULL, NULL, NULL, ARRAY['Article 7'],
    365, TRUE);
```

## Audit Event Collection

### Event Generation

c

```c
/*
 * Audit event generation interface.
 */
typedef struct AuditEventBuilder {
    AuditEvent*         event;
    EngineContext*      ctx;
} AuditEventBuilder;

/*
 * Create new audit event.
 */
AuditEventBuilder* audit_event_new(
    AuditCategory       category,
    const char*         event_type,
    const char*         event_subtype
) {
    AuditEventBuilder* builder = allocate_builder();
    builder->event = allocate_audit_event();

    builder->event->event_id = generate_uuid_v7();
    builder->event->event_sequence = atomic_increment(&global_audit_sequence);
    builder->event->event_timestamp = current_timestamp();
    builder->event->event_timestamp_utc = current_timestamp_utc();

    builder->event->event_category = category;
    builder->event->event_type = strdup(event_type);
    builder->event->event_subtype = event_subtype ? strdup(event_subtype) : NULL;

    /* Lookup default severity */
    builder->event->event_severity = get_default_severity(category, event_type, event_subtype);

    /* Set node context */
    builder->event->cluster_uuid = get_cluster_uuid();
    builder->event->database_uuid = get_current_database_uuid();
    builder->event->node_uuid = get_node_uuid();
    builder->event->node_role = strdup(get_node_role());

    return builder;
}

/*
 * Builder methods.
 */
AuditEventBuilder* audit_set_outcome(AuditEventBuilder* b, 
                                      AuditOutcome outcome,
                                      int error_code,
                                      const char* error_message) {
    b->event->event_outcome = outcome;
    b->event->error_code = error_code;
    b->event->error_message = error_message ? strdup(error_message) : NULL;
    return b;
}

AuditEventBuilder* audit_set_actor_user(AuditEventBuilder* b,
                                         uuid_t user_uuid,
                                         const char* username,
                                         uuid_t session_id) {
    b->event->actor_type = ACTOR_USER;
    uuid_copy(b->event->actor_user_uuid, user_uuid);
    b->event->actor_username = strdup(username);
    uuid_copy(b->event->actor_session_id, session_id);
    return b;
}

AuditEventBuilder* audit_set_actor_system(AuditEventBuilder* b,
                                           const char* component) {
    b->event->actor_type = ACTOR_SYSTEM;
    b->event->actor_application = strdup(component);
    return b;
}

AuditEventBuilder* audit_set_source(AuditEventBuilder* b,
                                     struct sockaddr* addr,
                                     const char* protocol) {
    if (addr) {
        b->event->source_address = dup_inet_addr(addr);
        b->event->source_port = get_port(addr);
    }
    b->event->source_protocol = protocol ? strdup(protocol) : NULL;
    return b;
}

AuditEventBuilder* audit_set_target(AuditEventBuilder* b,
                                     const char* target_type,
                                     uuid_t target_uuid,
                                     const char* target_name) {
    b->event->target_type = strdup(target_type);
    if (target_uuid) {
        uuid_copy(b->event->target_uuid, target_uuid);
    }
    b->event->target_name = target_name ? strdup(target_name) : NULL;
    return b;
}

AuditEventBuilder* audit_set_statement(AuditEventBuilder* b,
                                        const char* statement) {
    /* Apply redaction policy before storing */
    char* redacted = redact_sensitive_values(statement);
    b->event->action_statement = redacted;
    return b;
}

AuditEventBuilder* audit_set_parameters(AuditEventBuilder* b,
                                         const char* json_params) {
    b->event->action_parameters = parse_json(json_params);
    return b;
}

AuditEventBuilder* audit_set_security_context(AuditEventBuilder* b,
                                               SessionSecurityContext* ctx) {
    /* Build JSON security context */
    JsonBuilder* jb = json_builder_new();
    json_add_string(jb, "auth_method", ctx->auth_method);
    json_add_string(jb, "tls_version", ctx->tls_version);
    json_add_string(jb, "tls_cipher", ctx->tls_cipher);
    json_add_bool(jb, "channel_binding", ctx->channel_binding_used);
    json_add_string(jb, "security_level", 
                   security_level_to_string(ctx->security_level));

    b->event->security_context = json_builder_finish(jb);
    return b;
}

/*
 * Submit event to audit system.
 */
int audit_submit(AuditEventBuilder* b) {
    /* Compute event hash for tamper detection */
    compute_event_hash(b->event);

    /* Chain to previous event */
    b->event->previous_event_hash = get_previous_event_hash();

    /* Check audit policy */
    if (!should_log_event(b->event)) {
        free_audit_event(b->event);
        free_builder(b);
        return OK;  /* Filtered by policy, not an error */
    }

    /* Submit to collector */
    int rc = audit_collector_submit(b->event);

    if (rc != OK) {
        /* Audit logging failure - this is critical */
        handle_audit_failure(b->event, rc);
    }

    free_builder(b);
    return rc;
}

/*
 * Convenience macros for common events.
 */
#define AUDIT_LOGIN_SUCCESS(user_uuid, username, session_id, addr, protocol) \
    audit_submit( \
        audit_set_source( \
            audit_set_actor_user( \
                audit_set_outcome( \
                    audit_event_new(AUDIT_AUTHENTICATION, "LOGIN_SUCCESS", NULL), \
                    OUTCOME_SUCCESS, 0, NULL), \
                user_uuid, username, session_id), \
            addr, protocol))

#define AUDIT_LOGIN_FAILURE(username, addr, protocol, error_code, error_msg) \
    audit_submit( \
        audit_set_source( \
            audit_set_actor_user( \
                audit_set_outcome( \
                    audit_event_new(AUDIT_AUTHENTICATION, "LOGIN_FAILURE", NULL), \
                    OUTCOME_FAILURE, error_code, error_msg), \
                NULL_UUID, username, NULL_UUID), \
            addr, protocol))
```

### Statement Auditing

c

```c
/*
 * Audit SQL statement execution.
 */
int audit_statement_execution(
    StatementContext*   stmt_ctx,
    ExecutionResult*    result
) {
    /* Check if this statement type should be audited */
    AuditPolicy* policy = get_effective_audit_policy(
        stmt_ctx->session,
        stmt_ctx->target_objects
    );

    if (!policy_requires_audit(policy, stmt_ctx)) {
        return OK;  /* Not audited */
    }

    /* Determine category and type */
    AuditCategory category;
    const char* event_type;
    const char* event_subtype = NULL;

    switch (stmt_ctx->statement_type) {
        case STMT_SELECT:
            category = AUDIT_DATA_ACCESS;
            event_type = "SELECT";
            /* Check if accessing sensitive data */
            if (accesses_sensitive_columns(stmt_ctx)) {
                event_subtype = "SENSITIVE";
            }
            break;

        case STMT_INSERT:
            category = AUDIT_DATA_MODIFICATION;
            event_type = "INSERT";
            break;

        case STMT_UPDATE:
            category = AUDIT_DATA_MODIFICATION;
            event_type = "UPDATE";
            break;

        case STMT_DELETE:
            category = AUDIT_DATA_MODIFICATION;
            event_type = "DELETE";
            break;

        case STMT_CREATE_TABLE:
            category = AUDIT_SCHEMA_CHANGE;
            event_type = "CREATE_TABLE";
            break;

        /* ... other statement types ... */
    }

    /* Build and submit event */
    AuditEventBuilder* builder = audit_event_new(category, event_type, event_subtype);

    audit_set_actor_user(builder,
                        stmt_ctx->session->user_uuid,
                        stmt_ctx->session->username,
                        stmt_ctx->session->session_id);

    audit_set_source(builder,
                    stmt_ctx->session->client_addr,
                    stmt_ctx->session->source_protocol);

    /* Set target (first object, or make composite for multi-table) */
    if (stmt_ctx->target_objects_count == 1) {
        ObjectRef* obj = &stmt_ctx->target_objects[0];
        audit_set_target(builder, obj->type, obj->uuid, obj->qualified_name);
    } else {
        /* Multiple objects - store in parameters */
        char* targets_json = serialize_target_objects(stmt_ctx->target_objects,
                                                       stmt_ctx->target_objects_count);
        audit_set_target(builder, "MULTIPLE", NULL_UUID, NULL);
        audit_set_parameters(builder, targets_json);
        free(targets_json);
    }

    /* Statement text (potentially redacted) */
    if (policy->log_statement_text) {
        audit_set_statement(builder, stmt_ctx->statement_text);
    }

    /* Outcome */
    if (result->success) {
        audit_set_outcome(builder, OUTCOME_SUCCESS, 0, NULL);
        builder->event->result_rows_affected = result->rows_affected;
    } else {
        audit_set_outcome(builder, OUTCOME_FAILURE, 
                         result->error_code, result->error_message);
    }

    /* Transaction context */
    builder->event->transaction_id = stmt_ctx->transaction_id;
    builder->event->transaction_state = get_transaction_state_string(stmt_ctx);

    /* Security context */
    audit_set_security_context(builder, stmt_ctx->session->security_context);

    return audit_submit(builder);
}
```

### Sensitive Data Detection

sql

```sql
CREATE TABLE sys.sensitive_data_classification (
    classification_id       UUID PRIMARY KEY DEFAULT gen_uuid_v7(),

    -- Target
    database_uuid           UUID,
    schema_name             VARCHAR(128),
    table_name              VARCHAR(128),
    column_name             VARCHAR(128),

    -- Classification
    sensitivity_level       ENUM('PUBLIC', 'INTERNAL', 'CONFIDENTIAL', 
                                 'HIGHLY_CONFIDENTIAL', 'RESTRICTED') NOT NULL,
    data_category           VARCHAR(64),  -- PII, PHI, PCI, etc.

    -- Compliance tagging
    pii_type                VARCHAR(64),  -- SSN, DOB, EMAIL, etc.
    phi_type                VARCHAR(64),  -- DIAGNOSIS, TREATMENT, etc.
    pci_type                VARCHAR(64),  -- PAN, CVV, etc.

    -- Audit behavior
    audit_on_access         BOOLEAN DEFAULT TRUE,
    audit_on_modification   BOOLEAN DEFAULT TRUE,
    log_values              BOOLEAN DEFAULT FALSE,  -- careful with this!
    mask_in_logs            BOOLEAN DEFAULT TRUE,

    -- Auto-detection pattern (for discovery)
    detection_pattern       VARCHAR(256),
    detection_confidence    FLOAT,

    -- Status
    status                  ENUM('ACTIVE', 'DISABLED') DEFAULT 'ACTIVE',
    verified_by             UUID,
    verified_at             TIMESTAMP,

    INDEX idx_table (database_uuid, schema_name, table_name),
    INDEX idx_column (database_uuid, schema_name, table_name, column_name),
    INDEX idx_category (data_category),
    INDEX idx_sensitivity (sensitivity_level)
);
```

c

```c
/*
 * Check if statement accesses sensitive data.
 */
bool accesses_sensitive_columns(StatementContext* stmt_ctx) {
    for (int i = 0; i < stmt_ctx->accessed_columns_count; i++) {
        ColumnRef* col = &stmt_ctx->accessed_columns[i];

        SensitiveClassification* class = lookup_classification(
            stmt_ctx->database_uuid,
            col->schema_name,
            col->table_name,
            col->column_name
        );

        if (class && class->status == CLASSIFICATION_ACTIVE) {
            if (class->sensitivity_level >= SENSITIVITY_CONFIDENTIAL) {
                return true;
            }
        }
    }
    return false;
}

/*
 * Redact sensitive values from SQL statement.
 */
char* redact_sensitive_values(const char* statement) {
    /* Parse statement to identify literals */
    ParseResult* parsed = parse_sql(statement);

    if (!parsed || parsed->error) {
        /* Can't parse - redact all string literals conservatively */
        return redact_all_literals(statement);
    }

    StringBuilder* result = sb_new();
    size_t pos = 0;

    for (int i = 0; i < parsed->literal_count; i++) {
        Literal* lit = &parsed->literals[i];

        /* Copy text before this literal */
        sb_append_n(result, statement + pos, lit->start_pos - pos);

        /* Determine if this should be redacted */
        bool should_redact = false;

        if (lit->type == LITERAL_STRING) {
            /* Check if it looks like sensitive data */
            should_redact = looks_like_sensitive(lit->value) ||
                           is_password_context(parsed, lit);
        }

        if (should_redact) {
            sb_append(result, "'[REDACTED]'");
        } else {
            sb_append_n(result, statement + lit->start_pos, 
                       lit->end_pos - lit->start_pos);
        }

        pos = lit->end_pos;
    }

    /* Copy remainder */
    sb_append(result, statement + pos);

    return sb_finish(result);
}

/*
 * Check if value looks like sensitive data.
 */
bool looks_like_sensitive(const char* value) {
    /* SSN pattern: XXX-XX-XXXX */
    if (matches_pattern(value, "^\\d{3}-\\d{2}-\\d{4}$")) {
        return true;
    }

    /* Credit card pattern */
    if (matches_pattern(value, "^\\d{13,19}$") && passes_luhn(value)) {
        return true;
    }

    /* Email-like pattern in sensitive context */
    if (matches_pattern(value, "^[^@]+@[^@]+\\.[^@]+$")) {
        return true;
    }

    /* Add more patterns as needed */

    return false;
}
```

## Audit Log Integrity

### Chain Hashing

c

```c
/*
 * Compute hash for audit event.
 * Uses SHA-256 with chaining to previous event.
 */
void compute_event_hash(AuditEvent* event) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    /* Hash fixed fields */
    SHA256_Update(&ctx, &event->event_id, sizeof(uuid_t));
    SHA256_Update(&ctx, &event->event_sequence, sizeof(int64_t));
    SHA256_Update(&ctx, &event->event_timestamp, sizeof(timestamp_t));
    SHA256_Update(&ctx, &event->event_category, sizeof(int));

    /* Hash strings */
    hash_string(&ctx, event->event_type);
    hash_string(&ctx, event->event_subtype);
    hash_string(&ctx, event->actor_username);
    hash_string(&ctx, event->target_name);
    hash_string(&ctx, event->action_statement);

    /* Hash outcome */
    SHA256_Update(&ctx, &event->event_outcome, sizeof(int));
    SHA256_Update(&ctx, &event->error_code, sizeof(int));

    /* Hash node identity */
    SHA256_Update(&ctx, &event->database_uuid, sizeof(uuid_t));
    SHA256_Update(&ctx, &event->node_uuid, sizeof(uuid_t));

    /* Include previous hash for chaining */
    if (event->previous_event_hash) {
        SHA256_Update(&ctx, event->previous_event_hash, 32);
    }

    /* Finalize */
    event->event_hash = secure_alloc(32);
    SHA256_Final(event->event_hash, &ctx);
}

/*
 * Verify audit log integrity.
 */
int verify_audit_chain(
    uuid_t              database_uuid,
    int64_t             start_sequence,
    int64_t             end_sequence,
    VerificationResult* result
) {
    result->events_checked = 0;
    result->chain_valid = true;
    result->first_invalid_sequence = 0;

    uint8_t* previous_hash = NULL;

    /* Iterate through events in sequence order */
    AuditEventIterator* iter = audit_iterator_new(
        database_uuid, start_sequence, end_sequence);

    AuditEvent* event;
    while ((event = audit_iterator_next(iter)) != NULL) {
        result->events_checked++;

        /* Verify this event's hash */
        uint8_t computed_hash[32];
        uint8_t* stored_previous = event->previous_event_hash;

        /* Temporarily set previous hash for computation */
        event->previous_event_hash = previous_hash;
        compute_event_hash_verify(event, computed_hash);
        event->previous_event_hash = stored_previous;

        /* Compare hashes */
        if (memcmp(computed_hash, event->event_hash, 32) != 0) {
            result->chain_valid = false;
            result->first_invalid_sequence = event->event_sequence;
            result->invalid_event_id = event->event_id;
            result->error = "Event hash mismatch - possible tampering";
            break;
        }

        /* Verify chain linkage */
        if (previous_hash) {
            if (!event->previous_event_hash ||
                memcmp(previous_hash, event->previous_event_hash, 32) != 0) {
                result->chain_valid = false;
                result->first_invalid_sequence = event->event_sequence;
                result->invalid_event_id = event->event_id;
                result->error = "Chain linkage broken - possible tampering";
                break;
            }
        }

        /* Save this hash for next iteration */
        if (!previous_hash) {
            previous_hash = secure_alloc(32);
        }
        memcpy(previous_hash, event->event_hash, 32);
    }

    if (previous_hash) {
        secure_free(previous_hash, 32);
    }

    audit_iterator_free(iter);
    return OK;
}
```

### Audit Log Signing

sql

```sql
CREATE TABLE sys.audit_log_signatures (
    signature_id            UUID PRIMARY KEY DEFAULT gen_uuid_v7(),

    -- Scope
    database_uuid           UUID NOT NULL,
    start_sequence          BIGINT NOT NULL,
    end_sequence            BIGINT NOT NULL,

    -- Hash of covered events
    events_hash             BYTEA NOT NULL,  -- hash of all event hashes
    event_count             INTEGER NOT NULL,

    -- Signature
    signature               BYTEA NOT NULL,
    signing_algorithm       VARCHAR(32) NOT NULL,  -- 'RSA-SHA256', 'ECDSA-P256'
    signing_key_id          VARCHAR(64) NOT NULL,

    -- Timestamp
    signed_at               TIMESTAMP NOT NULL,
    timestamp_token         BYTEA,  -- RFC 3161 timestamp token
    timestamp_authority     VARCHAR(256),

    -- Verification
    verified_at             TIMESTAMP,
    verification_status     ENUM('PENDING', 'VALID', 'INVALID'),

    UNIQUE (database_uuid, end_sequence),
    INDEX idx_database (database_uuid, start_sequence, end_sequence)
);
```

c

```c
/*
 * Sign a block of audit events.
 */
int sign_audit_block(
    uuid_t              database_uuid,
    int64_t             start_sequence,
    int64_t             end_sequence,
    SigningKey*         key,
    AuditBlockSignature* signature
) {
    /* Compute aggregate hash of all events in range */
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    int event_count = 0;
    AuditEventIterator* iter = audit_iterator_new(
        database_uuid, start_sequence, end_sequence);

    AuditEvent* event;
    while ((event = audit_iterator_next(iter)) != NULL) {
        SHA256_Update(&ctx, event->event_hash, 32);
        event_count++;
    }
    audit_iterator_free(iter);

    uint8_t events_hash[32];
    SHA256_Final(events_hash, &ctx);

    /* Sign the hash */
    size_t sig_len = 0;
    uint8_t* sig_data = NULL;

    switch (key->algorithm) {
        case SIGN_RSA_SHA256:
            sig_data = rsa_sign_sha256(key->private_key, events_hash, 32, &sig_len);
            break;
        case SIGN_ECDSA_P256:
            sig_data = ecdsa_sign_p256(key->private_key, events_hash, 32, &sig_len);
            break;
    }

    if (!sig_data) {
        return ERR_SIGNING_FAILED;
    }

    /* Get timestamp token from TSA */
    TimestampToken* tst = NULL;
    if (policy.audit.use_timestamp_authority) {
        tst = get_timestamp_token(events_hash, 32);
    }

    /* Build signature record */
    signature->signature_id = generate_uuid_v7();
    uuid_copy(signature->database_uuid, database_uuid);
    signature->start_sequence = start_sequence;
    signature->end_sequence = end_sequence;
    memcpy(signature->events_hash, events_hash, 32);
    signature->event_count = event_count;
    signature->signature = sig_data;
    signature->signature_len = sig_len;
    signature->signing_algorithm = key->algorithm;
    signature->signing_key_id = strdup(key->key_id);
    signature->signed_at = current_timestamp();
    if (tst) {
        signature->timestamp_token = tst->token;
        signature->timestamp_token_len = tst->token_len;
        signature->timestamp_authority = strdup(tst->authority);
    }

    return OK;
}
```

## Audit Log Management

### Retention Policies

sql

```sql
CREATE TABLE sys.audit_retention_policy (
    policy_id               UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    policy_name             VARCHAR(128) NOT NULL UNIQUE,

    -- Scope
    applies_to_category     audit_category,  -- NULL = all categories
    applies_to_type         VARCHAR(64),     -- NULL = all types
    applies_to_severity     audit_severity,  -- NULL = all severities

    -- Retention settings
    retention_days          INTEGER NOT NULL,
    archive_before_delete   BOOLEAN DEFAULT TRUE,

    -- Archive settings
    archive_format          ENUM('JSON', 'PARQUET', 'CSV') DEFAULT 'PARQUET',
    archive_compression     ENUM('NONE', 'GZIP', 'ZSTD', 'LZ4') DEFAULT 'ZSTD',
    archive_encryption      BOOLEAN DEFAULT TRUE,
    archive_location        VARCHAR(512),  -- path or S3 bucket

    -- Compliance override (can't delete if compliance requires longer)
    respect_compliance_minimum BOOLEAN DEFAULT TRUE,

    -- Status
    status                  ENUM('ACTIVE', 'DISABLED') NOT NULL DEFAULT 'ACTIVE',
    priority                INTEGER NOT NULL DEFAULT 100,

    -- Audit
    created_at              TIMESTAMP NOT NULL DEFAULT now(),
    created_by              UUID,
    last_executed           TIMESTAMP,

    INDEX idx_status (status)
);

-- Default policies
INSERT INTO sys.audit_retention_policy (policy_name, applies_to_category, 
    retention_days, archive_before_delete) VALUES
('default_policy', NULL, 90, TRUE),
('authentication_extended', 'AUTHENTICATION', 365, TRUE),
('security_admin_extended', 'SECURITY_ADMIN', 730, TRUE),
('encryption_extended', 'ENCRYPTION', 730, TRUE),
('compliance_events', 'COMPLIANCE', 2555, TRUE);  -- 7 years
```

### Retention Execution

c

```c
/*
 * Execute audit log retention.
 */
int execute_audit_retention(void) {
    timestamp_t now = current_timestamp();

    /* Get all active policies, ordered by priority */
    RetentionPolicy* policies = get_active_retention_policies();

    /* Build retention plan */
    RetentionPlan* plan = build_retention_plan(policies, now);

    for (int i = 0; i < plan->partition_count; i++) {
        PartitionRetention* pr = &plan->partitions[i];

        if (pr->action == RETENTION_DELETE) {
            /* Check compliance minimums first */
            if (has_compliance_events(pr->partition_id)) {
                int min_retention = get_max_compliance_retention(pr->partition_id);
                if (pr->age_days < min_retention) {
                    log_info("Skipping partition %s - compliance requires %d more days",
                            pr->partition_name, min_retention - pr->age_days);
                    continue;
                }
            }

            if (pr->archive_first) {
                /* Archive partition before delete */
                int rc = archive_audit_partition(pr);
                if (rc != OK) {
                    log_error("Failed to archive partition %s: %s",
                             pr->partition_name, get_error_string(rc));
                    continue;  /* Don't delete if archive failed */
                }
            }

            /* Sign the partition before deletion */
            if (policy.audit.sign_before_delete) {
                AuditBlockSignature sig;
                sign_audit_block(pr->database_uuid, 
                               pr->start_sequence, pr->end_sequence,
                               get_audit_signing_key(), &sig);
                store_signature(&sig);
            }

            /* Delete partition */
            int rc = drop_audit_partition(pr->partition_id);
            if (rc != OK) {
                log_error("Failed to delete partition %s: %s",
                         pr->partition_name, get_error_string(rc));
            } else {
                log_info("Deleted audit partition %s (%d events)",
                        pr->partition_name, pr->event_count);

                /* Audit the deletion */
                AUDIT_EVENT("AUDIT_PARTITION_DELETED", 
                           "partition=%s events=%d archived=%s",
                           pr->partition_name, pr->event_count,
                           pr->archive_first ? "yes" : "no");
            }
        }
    }

    free_retention_plan(plan);
    return OK;
}
```

### Archive Format

c

```c
/*
 * Archive audit partition to storage.
 */
int archive_audit_partition(PartitionRetention* pr) {
    /* Determine archive path */
    char archive_path[512];
    build_archive_path(pr, archive_path, sizeof(archive_path));

    /* Create archive writer */
    ArchiveWriter* writer;
    switch (pr->policy->archive_format) {
        case ARCHIVE_JSON:
            writer = json_archive_writer_new(archive_path);
            break;
        case ARCHIVE_PARQUET:
            writer = parquet_archive_writer_new(archive_path);
            break;
        case ARCHIVE_CSV:
            writer = csv_archive_writer_new(archive_path);
            break;
    }

    /* Apply compression */
    if (pr->policy->archive_compression != COMPRESS_NONE) {
        writer = compressed_writer_wrap(writer, pr->policy->archive_compression);
    }

    /* Apply encryption */
    if (pr->policy->archive_encryption) {
        EncryptionKey* key = get_audit_archive_key();
        writer = encrypted_writer_wrap(writer, key);
    }

    /* Write events */
    AuditEventIterator* iter = audit_iterator_new(
        pr->database_uuid, pr->start_sequence, pr->end_sequence);

    AuditEvent* event;
    int count = 0;
    while ((event = audit_iterator_next(iter)) != NULL) {
        int rc = archive_writer_write(writer, event);
        if (rc != OK) {
            log_error("Failed to write event %s to archive", 
                     uuid_to_string(event->event_id));
            archive_writer_abort(writer);
            audit_iterator_free(iter);
            return rc;
        }
        count++;
    }

    audit_iterator_free(iter);

    /* Finalize archive */
    int rc = archive_writer_finish(writer);
    if (rc != OK) {
        return rc;
    }

    /* Record archive metadata */
    ArchiveRecord record = {
        .archive_id = generate_uuid_v7(),
        .database_uuid = pr->database_uuid,
        .partition_id = pr->partition_id,
        .start_sequence = pr->start_sequence,
        .end_sequence = pr->end_sequence,
        .event_count = count,
        .archive_path = strdup(archive_path),
        .archive_format = pr->policy->archive_format,
        .compressed = pr->policy->archive_compression != COMPRESS_NONE,
        .encrypted = pr->policy->archive_encryption,
        .archived_at = current_timestamp(),
        .archive_size_bytes = get_file_size(archive_path),
        .checksum = compute_file_checksum(archive_path)
    };

    store_archive_record(&record);

    log_info("Archived %d events to %s (%.2f MB)",
            count, archive_path, record.archive_size_bytes / (1024.0 * 1024.0));

    return OK;
}
```

## Real-Time Streaming

### SIEM Integration

sql

```sql
CREATE TABLE sys.audit_stream_config (
    stream_id               UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    stream_name             VARCHAR(128) NOT NULL UNIQUE,

    -- Destination
    destination_type        ENUM('SYSLOG', 'KAFKA', 'WEBHOOK', 'FILE', 
                                 'SPLUNK', 'ELASTICSEARCH', 'CLOUDWATCH') NOT NULL,
    destination_config      JSONB NOT NULL,

    -- Filter
    include_categories      audit_category[],
    exclude_categories      audit_category[],
    min_severity            audit_severity DEFAULT 'INFO',
    include_types           VARCHAR(64)[],
    exclude_types           VARCHAR(64)[],
    custom_filter           TEXT,  -- SQL WHERE clause

    -- Format
    output_format           ENUM('JSON', 'CEF', 'LEEF', 'SYSLOG_RFC5424') DEFAULT 'JSON',
    include_statement       BOOLEAN DEFAULT FALSE,
    include_parameters      BOOLEAN DEFAULT TRUE,

    -- Delivery
    batch_size              INTEGER DEFAULT 100,
    batch_timeout_ms        INTEGER DEFAULT 1000,
    retry_count             INTEGER DEFAULT 3,
    retry_delay_ms          INTEGER DEFAULT 1000,

    -- TLS
    tls_enabled             BOOLEAN DEFAULT TRUE,
    tls_ca_cert             TEXT,
    tls_client_cert         TEXT,
    tls_client_key_encrypted BYTEA,

    -- Authentication
    auth_type               ENUM('NONE', 'BASIC', 'BEARER', 'API_KEY', 'MTLS'),
    auth_credentials_encrypted BYTEA,

    -- Status
    status                  ENUM('ACTIVE', 'PAUSED', 'ERROR') NOT NULL DEFAULT 'ACTIVE',
    last_sent_sequence      BIGINT,
    last_sent_at            TIMESTAMP,
    error_count             INTEGER DEFAULT 0,
    last_error              TEXT,

    INDEX idx_status (status)
);
```

### Stream Processor

c

```c
/*
 * Audit stream processor.
 * Runs as background thread, sends events to configured destinations.
 */
typedef struct StreamProcessor {
    uuid_t              stream_id;
    StreamConfig*       config;

    /* Destination connection */
    void*               dest_connection;
    DestinationType     dest_type;

    /* Batching */
    AuditEvent**        batch;
    int                 batch_count;
    timestamp_t         batch_started;

    /* State */
    int64_t             last_sequence;
    bool                running;

    /* Stats */
    int64_t             events_sent;
    int64_t             events_failed;
    int64_t             bytes_sent;

} StreamProcessor;

void stream_processor_run(StreamProcessor* sp) {
    sp->running = true;

    while (sp->running) {
        /* Get next batch of events */
        int64_t start_seq = sp->last_sequence + 1;
        AuditEventBatch* batch = get_audit_events_since(
            start_seq, sp->config->batch_size);

        if (batch->count == 0) {
            /* No new events, wait */
            usleep(10000);  /* 10ms */
            continue;
        }

        /* Filter events */
        for (int i = 0; i < batch->count; i++) {
            AuditEvent* event = batch->events[i];

            if (!event_matches_filter(event, sp->config)) {
                continue;
            }

            add_to_batch(sp, event);
        }

        /* Check if batch should be sent */
        if (should_send_batch(sp)) {
            int rc = send_batch(sp);

            if (rc == OK) {
                sp->last_sequence = batch->events[batch->count - 1]->event_sequence;
                update_stream_state(sp);
                clear_batch(sp);
            } else {
                handle_send_failure(sp, rc);
            }
        }

        free_batch(batch);
    }
}

int send_batch(StreamProcessor* sp) {
    /* Format events */
    char* payload;
    size_t payload_len;

    switch (sp->config->output_format) {
        case FORMAT_JSON:
            payload = format_events_json(sp->batch, sp->batch_count, &payload_len);
            break;
        case FORMAT_CEF:
            payload = format_events_cef(sp->batch, sp->batch_count, &payload_len);
            break;
        case FORMAT_LEEF:
            payload = format_events_leef(sp->batch, sp->batch_count, &payload_len);
            break;
        case FORMAT_SYSLOG_RFC5424:
            payload = format_events_syslog(sp->batch, sp->batch_count, &payload_len);
            break;
    }

    /* Send to destination */
    int rc;
    switch (sp->dest_type) {
        case DEST_SYSLOG:
            rc = send_syslog(sp->dest_connection, payload, payload_len);
            break;
        case DEST_KAFKA:
            rc = send_kafka(sp->dest_connection, payload, payload_len);
            break;
        case DEST_WEBHOOK:
            rc = send_webhook(sp->dest_connection, payload, payload_len);
            break;
        case DEST_SPLUNK:
            rc = send_splunk_hec(sp->dest_connection, payload, payload_len);
            break;
        case DEST_ELASTICSEARCH:
            rc = send_elasticsearch(sp->dest_connection, payload, payload_len);
            break;
        /* ... other destinations ... */
    }

    free(payload);

    if (rc == OK) {
        sp->events_sent += sp->batch_count;
        sp->bytes_sent += payload_len;
    }

    return rc;
}
```

### CEF Format

c

```c
/*
 * Format events in Common Event Format (CEF) for SIEM integration.
 * Format: CEF:Version|Device Vendor|Device Product|Device Version|
 *         Signature ID|Name|Severity|Extension
 */
char* format_event_cef(AuditEvent* event) {
    StringBuilder* sb = sb_new();

    /* CEF header */
    sb_append(sb, "CEF:0|MyDB|DatabaseEngine|1.0|");

    /* Signature ID (event type) */
    sb_append_format(sb, "%s.%s|", 
                    audit_category_to_string(event->event_category),
                    event->event_type);

    /* Event name */
    sb_append_format(sb, "%s %s|",
                    audit_category_to_string(event->event_category),
                    event->event_type);

    /* Severity (0-10 scale) */
    int cef_severity = severity_to_cef(event->event_severity);
    sb_append_format(sb, "%d|", cef_severity);

    /* Extension (key=value pairs) */

    /* Timestamps */
    sb_append_format(sb, "rt=%lld ", timestamp_to_millis(event->event_timestamp));

    /* Actor */
    if (event->actor_username) {
        sb_append_format(sb, "suser=%s ", escape_cef(event->actor_username));
    }
    if (event->source_address) {
        sb_append_format(sb, "src=%s ", inet_ntoa(event->source_address));
    }
    if (event->source_port) {
        sb_append_format(sb, "spt=%d ", event->source_port);
    }

    /* Target */
    if (event->target_type) {
        sb_append_format(sb, "cs1Label=TargetType cs1=%s ", event->target_type);
    }
    if (event->target_name) {
        sb_append_format(sb, "cs2Label=TargetName cs2=%s ", 
                        escape_cef(event->target_name));
    }

    /* Outcome */
    sb_append_format(sb, "outcome=%s ", 
                    event->event_outcome == OUTCOME_SUCCESS ? "success" : "failure");

    /* Database context */
    if (event->target_database_name) {
        sb_append_format(sb, "cs3Label=Database cs3=%s ", 
                        event->target_database_name);
    }

    /* Session */
    if (!uuid_is_null(event->actor_session_id)) {
        sb_append_format(sb, "cs4Label=SessionID cs4=%s ",
                        uuid_to_string(event->actor_session_id));
    }

    /* Statement (if included and not too long) */
    if (event->action_statement && strlen(event->action_statement) < 1000) {
        sb_append_format(sb, "msg=%s ", escape_cef(event->action_statement));
    }

    return sb_finish(sb);
}
```

## Alert Engine

### Alert Rules

sql

```sql
CREATE TABLE sys.audit_alert_rules (
    rule_id                 UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    rule_name               VARCHAR(128) NOT NULL UNIQUE,

    -- Trigger conditions
    event_category          audit_category,
    event_type              VARCHAR(64),
    event_outcome           ENUM('SUCCESS', 'FAILURE', 'ANY') DEFAULT 'ANY',
    min_severity            audit_severity DEFAULT 'WARNING',

    -- Pattern matching
    condition_sql           TEXT,  -- SQL expression returning boolean

    -- Aggregation (for rate-based alerts)
    aggregation_type        ENUM('NONE', 'COUNT', 'DISTINCT_COUNT', 'RATE') DEFAULT 'NONE',
    aggregation_field       VARCHAR(64),  -- e.g., 'actor_username', 'source_address'
    aggregation_window_seconds INTEGER,
    aggregation_threshold   INTEGER,

    -- Alert configuration
    alert_severity          ENUM('LOW', 'MEDIUM', 'HIGH', 'CRITICAL') NOT NULL,
    alert_title             VARCHAR(256) NOT NULL,
    alert_description       TEXT,

    -- Actions
    notification_channels   UUID[],  -- references sys.notification_channels
    create_incident         BOOLEAN DEFAULT FALSE,
    auto_block_user         BOOLEAN DEFAULT FALSE,
    auto_block_address      BOOLEAN DEFAULT FALSE,
    custom_action           TEXT,  -- SQL to execute

    -- Deduplication
    dedup_window_seconds    INTEGER DEFAULT 300,
    dedup_key               VARCHAR(64),  -- field to deduplicate on

    -- Status
    status                  ENUM('ACTIVE', 'DISABLED', 'TESTING') NOT NULL DEFAULT 'ACTIVE',

    -- Audit
    created_at              TIMESTAMP NOT NULL DEFAULT now(),
    created_by              UUID,
    last_triggered          TIMESTAMP,
    trigger_count           BIGINT DEFAULT 0,

    INDEX idx_status (status),
    INDEX idx_category_type (event_category, event_type)
);

-- Pre-built security alerts
INSERT INTO sys.audit_alert_rules (rule_name, event_category, event_type,
    aggregation_type, aggregation_field, aggregation_window_seconds, 
    aggregation_threshold, alert_severity, alert_title, alert_description) VALUES

('brute_force_login', 'AUTHENTICATION', 'LOGIN_FAILURE',
    'COUNT', 'source_address', 300, 10,
    'HIGH', 'Brute Force Login Attempt Detected',
    'More than 10 failed login attempts from same IP in 5 minutes'),

('privilege_escalation', 'AUTHORIZATION', 'PRIVILEGE_GRANTED',
    'NONE', NULL, NULL, NULL,
    'MEDIUM', 'Privilege Escalation',
    'New privileges granted to user'),

('mass_data_access', 'DATA_ACCESS', 'SELECT',
    'COUNT', 'actor_user_uuid', 60, 1000,
    'HIGH', 'Unusual Data Access Pattern',
    'User accessed more than 1000 records in 1 minute'),

('after_hours_admin', 'SECURITY_ADMIN', NULL,
    'NONE', NULL, NULL, NULL,
    'MEDIUM', 'After Hours Administrative Activity',
    'Administrative action performed outside business hours'),

('encryption_key_export', 'ENCRYPTION', 'KEY_EXPORTED',
    'NONE', NULL, NULL, NULL,
    'CRITICAL', 'Encryption Key Exported',
    'An encryption key was exported from the system'),

('multiple_failed_logins_user', 'AUTHENTICATION', 'LOGIN_FAILURE',
    'COUNT', 'actor_username', 600, 5,
    'MEDIUM', 'Multiple Failed Logins for User',
    'User has 5+ failed login attempts in 10 minutes');
```

### Alert Processing

c

```c
/*
 * Process audit events against alert rules.
 */
typedef struct AlertContext {
    AuditEvent*         event;
    AlertRule*          rule;
    AggregationState*   agg_state;
} AlertContext;

void process_event_for_alerts(AuditEvent* event) {
    /* Get applicable rules */
    AlertRule** rules = get_matching_rules(event->event_category,
                                           event->event_type,
                                           event->event_severity);

    for (int i = 0; rules[i] != NULL; i++) {
        AlertRule* rule = rules[i];

        if (rule->status != RULE_ACTIVE) {
            continue;
        }

        /* Check basic conditions */
        if (rule->event_outcome != OUTCOME_ANY &&
            rule->event_outcome != event->event_outcome) {
            continue;
        }

        /* Check custom SQL condition */
        if (rule->condition_sql) {
            if (!evaluate_condition(rule->condition_sql, event)) {
                continue;
            }
        }

        /* Handle aggregation */
        if (rule->aggregation_type != AGG_NONE) {
            if (!check_aggregation_threshold(rule, event)) {
                continue;  /* Threshold not yet reached */
            }
        }

        /* Check deduplication */
        if (is_duplicate_alert(rule, event)) {
            continue;
        }

        /* Fire alert */
        fire_alert(rule, event);
    }
}

bool check_aggregation_threshold(AlertRule* rule, AuditEvent* event) {
    /* Get aggregation key value */
    const char* key_value = get_field_value(event, rule->aggregation_field);
    if (!key_value) {
        return false;
    }

    /* Get or create aggregation state */
    char state_key[256];
    snprintf(state_key, sizeof(state_key), "alert:%s:%s",
            uuid_to_string(rule->rule_id), key_value);

    AggregationState* state = get_aggregation_state(state_key);
    if (!state) {
        state = create_aggregation_state(rule, key_value);
    }

    /* Update state */
    timestamp_t window_start = current_timestamp() - 
                               (rule->aggregation_window_seconds * 1000000LL);

    /* Expire old entries */
    expire_old_entries(state, window_start);

    /* Add new entry */
    switch (rule->aggregation_type) {
        case AGG_COUNT:
            state->count++;
            break;

        case AGG_DISTINCT_COUNT:
            /* Count distinct values of aggregation field */
            add_distinct_value(state, key_value);
            break;

        case AGG_RATE:
            /* Events per second */
            state->count++;
            state->rate = state->count / (double)rule->aggregation_window_seconds;
            break;
    }

    save_aggregation_state(state_key, state);

    /* Check threshold */
    int current_value = (rule->aggregation_type == AGG_RATE) ? 
                        (int)state->rate : state->count;

    return current_value >= rule->aggregation_threshold;
}

void fire_alert(AlertRule* rule, AuditEvent* event) {
    /* Create alert record */
    Alert* alert = allocate_alert();
    alert->alert_id = generate_uuid_v7();
    alert->rule_id = rule->rule_id;
    alert->triggered_at = current_timestamp();
    alert->severity = rule->alert_severity;
    alert->title = format_alert_title(rule, event);
    alert->description = format_alert_description(rule, event);
    alert->source_event_id = event->event_id;

    /* Store context */
    alert->context = build_alert_context(rule, event);

    /* Persist alert */
    store_alert(alert);

    /* Update rule stats */
    rule->last_triggered = current_timestamp();
    rule->trigger_count++;
    update_rule_stats(rule);

    /* Execute actions */
    for (int i = 0; i < rule->notification_channel_count; i++) {
        send_notification(rule->notification_channels[i], alert);
    }

    if (rule->auto_block_user && event->actor_user_uuid) {
        block_user(event->actor_user_uuid, "Auto-blocked by alert rule");
    }

    if (rule->auto_block_address && event->source_address) {
        block_address(event->source_address, "Auto-blocked by alert rule");
    }

    if (rule->create_incident) {
        create_security_incident(alert);
    }

    if (rule->custom_action) {
        execute_custom_action(rule->custom_action, event, alert);
    }

    /* Audit the alert */
    AUDIT_ALERT_FIRED(alert);
}
```

### Notification Channels

sql

```sql
CREATE TABLE sys.notification_channels (
    channel_id              UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    channel_name            VARCHAR(128) NOT NULL UNIQUE,
    channel_type            ENUM('EMAIL', 'SLACK', 'PAGERDUTY', 'WEBHOOK', 
                                 'SMS', 'TEAMS', 'OPSGENIE') NOT NULL,

    -- Configuration
    config                  JSONB NOT NULL,

    -- Templates
    title_template          TEXT,
    body_template           TEXT,

    -- Rate limiting
    max_per_hour            INTEGER DEFAULT 100,

    -- Status
    status                  ENUM('ACTIVE', 'DISABLED', 'ERROR') NOT NULL DEFAULT 'ACTIVE',
    last_used               TIMESTAMP,
    error_count             INTEGER DEFAULT 0,
    last_error              TEXT,

    INDEX idx_status (status)
);

-- Example configurations
INSERT INTO sys.notification_channels (channel_name, channel_type, config) VALUES
('security-team-email', 'EMAIL', '{
    "smtp_host": "smtp.example.com",
    "smtp_port": 587,
    "smtp_tls": true,
    "from": "dbsecurity@example.com",
    "to": ["security@example.com"],
    "cc": ["dba@example.com"]
}'),

('security-slack', 'SLACK', '{
    "webhook_url": "https://hooks.slack.com/services/xxx/yyy/zzz",
    "channel": "#security-alerts",
    "username": "DB Security Bot",
    "icon_emoji": ":lock:"
}'),

('pagerduty-critical', 'PAGERDUTY', '{
    "routing_key": "xxxxxxxxxxxx",
    "severity": "critical"
}');
```

## Compliance Reporting

### Report Definitions

sql

```sql
CREATE TABLE sys.compliance_report_definitions (
    report_id               UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    report_name             VARCHAR(128) NOT NULL UNIQUE,
    report_framework        VARCHAR(32) NOT NULL,  -- SOC2, HIPAA, PCI-DSS, GDPR

    -- Description
    description             TEXT,
    control_reference       VARCHAR(64),

    -- Query
    report_query            TEXT NOT NULL,

    -- Parameters
    default_period_days     INTEGER DEFAULT 30,

    -- Output
    output_columns          JSONB,  -- column definitions
    output_format           ENUM('TABLE', 'SUMMARY', 'TREND') DEFAULT 'TABLE',

    -- Scheduling
    schedule                VARCHAR(64),  -- cron expression
    auto_generate           BOOLEAN DEFAULT FALSE,

    INDEX idx_framework (report_framework)
);
```

### Pre-Built Compliance Reports

sql

```sql
-- SOC 2 Reports
INSERT INTO sys.compliance_report_definitions (report_name, report_framework,
    control_reference, description, report_query) VALUES

('soc2_user_access_review', 'SOC2', 'CC6.1',
    'User access listing with privileges for periodic review',
    'SELECT 
        u.username,
        u.created_at,
        u.last_login_at,
        u.status,
        array_agg(DISTINCT r.role_name) as roles,
        array_agg(DISTINCT p.privilege_name) as privileges
    FROM sys.users u
    LEFT JOIN sys.user_roles ur ON u.user_uuid = ur.user_uuid
    LEFT JOIN sys.roles r ON ur.role_uuid = r.role_uuid
    LEFT JOIN sys.user_privileges p ON u.user_uuid = p.user_uuid
    GROUP BY u.user_uuid, u.username, u.created_at, u.last_login_at, u.status
    ORDER BY u.username'),

('soc2_access_changes', 'SOC2', 'CC6.2',
    'User and role changes during reporting period',
    'SELECT 
        event_timestamp,
        event_type,
        actor_username,
        target_name as affected_user_or_role,
        event_outcome,
        action_parameters
    FROM sys.audit_events
    WHERE event_category IN (''SECURITY_ADMIN'', ''AUTHORIZATION'')
    AND event_timestamp >= :start_date
    AND event_timestamp < :end_date
    ORDER BY event_timestamp'),

('soc2_failed_logins', 'SOC2', 'CC6.8',
    'Failed authentication attempts',
    'SELECT 
        DATE(event_timestamp) as date,
        actor_username,
        source_address,
        COUNT(*) as failure_count
    FROM sys.audit_events
    WHERE event_category = ''AUTHENTICATION''
    AND event_type = ''LOGIN_FAILURE''
    AND event_timestamp >= :start_date
    AND event_timestamp < :end_date
    GROUP BY DATE(event_timestamp), actor_username, source_address
    HAVING COUNT(*) >= 3
    ORDER BY date, failure_count DESC'),

('soc2_encryption_status', 'SOC2', 'CC6.7',
    'Database encryption status and key rotation',
    'SELECT 
        d.database_name,
        e.encryption_mode,
        e.encryption_status,
        k.key_name,
        k.last_rotated_at,
        k.next_rotation_at,
        CASE WHEN k.next_rotation_at < CURRENT_TIMESTAMP THEN ''OVERDUE''
             WHEN k.next_rotation_at < CURRENT_TIMESTAMP + interval ''30 days'' THEN ''DUE_SOON''
             ELSE ''OK'' END as rotation_status
    FROM sys.databases d
    LEFT JOIN sys.database_encryption e ON d.database_uuid = e.database_uuid
    LEFT JOIN sys.master_key_config k ON e.database_key_id = k.key_id');

-- HIPAA Reports
INSERT INTO sys.compliance_report_definitions (report_name, report_framework,
    control_reference, description, report_query) VALUES

('hipaa_phi_access', 'HIPAA', '164.312(b)',
    'Access to tables containing PHI',
    'SELECT 
        e.event_timestamp,
        e.actor_username,
        e.source_address,
        e.target_name as table_accessed,
        e.result_rows_affected,
        e.action_statement
    FROM sys.audit_events e
    INNER JOIN sys.sensitive_data_classification s 
        ON s.table_name = e.target_name
    WHERE s.data_category = ''PHI''
    AND e.event_category = ''DATA_ACCESS''
    AND e.event_timestamp >= :start_date
    AND e.event_timestamp < :end_date
    ORDER BY e.event_timestamp'),

('hipaa_access_controls', 'HIPAA', '164.312(a)(1)',
    'Access control configuration status',
    'SELECT 
        u.username,
        u.status,
        auth.provider_name as auth_method,
        u.password_last_changed,
        u.mfa_enabled,
        CASE WHEN u.password_last_changed < CURRENT_TIMESTAMP - interval ''90 days''
             THEN ''EXPIRED'' ELSE ''OK'' END as password_status
    FROM sys.users u
    LEFT JOIN sys.user_auth_methods uam ON u.user_uuid = uam.user_uuid
    LEFT JOIN sys.auth_providers auth ON uam.provider_id = auth.provider_id
    ORDER BY u.username'),

('hipaa_audit_controls', 'HIPAA', '164.312(b)',
    'Audit logging configuration and status',
    'SELECT 
        ''Audit Logging'' as control,
        CASE WHEN COUNT(*) > 0 THEN ''ENABLED'' ELSE ''DISABLED'' END as status
    FROM sys.audit_events
    WHERE event_timestamp >= CURRENT_TIMESTAMP - interval ''1 hour''
    UNION ALL
    SELECT 
        ''Audit Retention'',
        retention_days || '' days'' 
    FROM sys.audit_retention_policy WHERE status = ''ACTIVE''
    UNION ALL
    SELECT
        ''Audit Integrity'',
        CASE WHEN verified THEN ''VERIFIED'' ELSE ''UNVERIFIED'' END
    FROM sys.audit_integrity_status');

-- PCI-DSS Reports
INSERT INTO sys.compliance_report_definitions (report_name, report_framework,
    control_reference, description, report_query) VALUES

('pci_cardholder_access', 'PCI-DSS', '7.1',
    'Users with access to cardholder data',
    'SELECT 
        u.username,
        u.status,
        array_agg(DISTINCT s.table_name) as pci_tables_accessible,
        array_agg(DISTINCT p.privilege_name) as privileges
    FROM sys.users u
    INNER JOIN sys.user_privileges p ON u.user_uuid = p.user_uuid
    INNER JOIN sys.sensitive_data_classification s 
        ON p.object_uuid = s.table_uuid
    WHERE s.data_category = ''PCI''
    GROUP BY u.user_uuid, u.username, u.status
    ORDER BY u.username'),

('pci_encryption_status', 'PCI-DSS', '3.4',
    'Encryption of stored cardholder data',
    'SELECT 
        s.table_name,
        s.column_name,
        s.pci_type,
        CASE WHEN ce.encryption_enabled THEN ''ENCRYPTED'' 
             ELSE ''NOT ENCRYPTED'' END as encryption_status,
        ce.encryption_algorithm
    FROM sys.sensitive_data_classification s
    LEFT JOIN sys.column_encryption ce ON s.column_uuid = ce.column_uuid
    WHERE s.data_category = ''PCI''
    ORDER BY s.table_name, s.column_name'),

('pci_access_logging', 'PCI-DSS', '10.2.1',
    'Individual access to cardholder data',
    'SELECT 
        e.event_timestamp,
        e.actor_username,
        e.target_name,
        e.event_type,
        e.result_rows_affected
    FROM sys.audit_events e
    INNER JOIN sys.sensitive_data_classification s 
        ON s.table_name = e.target_name
    WHERE s.data_category = ''PCI''
    AND e.event_category IN (''DATA_ACCESS'', ''DATA_MODIFICATION'')
    AND e.event_timestamp >= :start_date
    AND e.event_timestamp < :end_date
    ORDER BY e.event_timestamp');

-- GDPR Reports
INSERT INTO sys.compliance_report_definitions (report_name, report_framework,
    control_reference, description, report_query) VALUES

('gdpr_data_inventory', 'GDPR', 'Article 30',
    'Records of processing activities - PII inventory',
    'SELECT 
        s.database_name,
        s.schema_name,
        s.table_name,
        s.column_name,
        s.pii_type,
        s.sensitivity_level,
        CASE WHEN ce.encryption_enabled THEN ''Yes'' ELSE ''No'' END as encrypted,
        CASE WHEN rp.retention_days IS NOT NULL 
             THEN rp.retention_days || '' days'' ELSE ''Not set'' END as retention
    FROM sys.sensitive_data_classification s
    LEFT JOIN sys.column_encryption ce ON s.column_uuid = ce.column_uuid
    LEFT JOIN sys.retention_policies rp ON s.table_uuid = rp.table_uuid
    WHERE s.pii_type IS NOT NULL
    ORDER BY s.database_name, s.table_name, s.column_name'),

('gdpr_access_requests', 'GDPR', 'Article 15',
    'Data subject access requests processed',
    'SELECT 
        event_timestamp,
        result_details->>''data_subject_id'' as data_subject,
        event_type,
        actor_username as processed_by,
        result_details->>''response_time_hours'' as response_time
    FROM sys.audit_events
    WHERE event_category = ''COMPLIANCE''
    AND event_type IN (''DATA_PORTABILITY'', ''RIGHT_TO_ERASURE'')
    AND event_timestamp >= :start_date
    AND event_timestamp < :end_date
    ORDER BY event_timestamp'),

('gdpr_consent_tracking', 'GDPR', 'Article 7',
    'Consent records and changes',
    'SELECT 
        event_timestamp,
        result_details->>''data_subject_id'' as data_subject,
        event_type,
        result_details->>''consent_purpose'' as purpose,
        result_details->>''consent_given'' as consent_given
    FROM sys.audit_events
    WHERE event_category = ''COMPLIANCE''
    AND event_type IN (''CONSENT_RECORDED'', ''CONSENT_WITHDRAWN'')
    AND event_timestamp >= :start_date
    AND event_timestamp < :end_date
    ORDER BY event_timestamp');
```

### Report Generation

c

```c
/*
 * Generate compliance report.
 */
int generate_compliance_report(
    uuid_t                  report_def_id,
    ReportParameters*       params,
    ComplianceReport**      report
) {
    /* Get report definition */
    ReportDefinition* def = get_report_definition(report_def_id);
    if (!def) {
        return ERR_REPORT_NOT_FOUND;
    }

    /* Create report instance */
    *report = allocate_report();
    (*report)->report_id = generate_uuid_v7();
    (*report)->definition_id = report_def_id;
    (*report)->report_name = strdup(def->report_name);
    (*report)->framework = strdup(def->report_framework);
    (*report)->control_reference = def->control_reference ? 
                                   strdup(def->control_reference) : NULL;
    (*report)->generated_at = current_timestamp();
    (*report)->generated_by = params->requesting_user;
    (*report)->period_start = params->start_date;
    (*report)->period_end = params->end_date;

    /* Execute report query */
    PreparedStatement* stmt = prepare_statement(def->report_query);

    /* Bind parameters */
    bind_timestamp(stmt, "start_date", params->start_date);
    bind_timestamp(stmt, "end_date", params->end_date);

    /* Execute */
    ResultSet* rs = execute_query(stmt);

    if (!rs) {
        (*report)->status = REPORT_ERROR;
        (*report)->error_message = strdup(get_last_error());
        return ERR_REPORT_QUERY_FAILED;
    }

    /* Build report content */
    (*report)->column_count = rs->column_count;
    (*report)->columns = dup_column_info(rs->columns, rs->column_count);
    (*report)->row_count = 0;
    (*report)->rows = NULL;

    /* Copy rows */
    ReportRow* last_row = NULL;
    while (resultset_next(rs)) {
        ReportRow* row = allocate_report_row(rs->column_count);

        for (int i = 0; i < rs->column_count; i++) {
            row->values[i] = resultset_get_string(rs, i);
        }

        if (last_row) {
            last_row->next = row;
        } else {
            (*report)->rows = row;
        }
        last_row = row;
        (*report)->row_count++;
    }

    resultset_close(rs);
    statement_close(stmt);

    /* Calculate summary statistics */
    calculate_report_summary(*report);

    /* Sign report */
    sign_report(*report);

    /* Store report */
    store_compliance_report(*report);

    /* Audit report generation */
    AUDIT_COMPLIANCE_REPORT_GENERATED(*report);

    return OK;
}

/*
 * Sign report for integrity.
 */
void sign_report(ComplianceReport* report) {
    /* Compute report hash */
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    /* Hash metadata */
    SHA256_Update(&ctx, &report->report_id, sizeof(uuid_t));
    SHA256_Update(&ctx, &report->generated_at, sizeof(timestamp_t));
    SHA256_Update(&ctx, &report->period_start, sizeof(timestamp_t));
    SHA256_Update(&ctx, &report->period_end, sizeof(timestamp_t));

    /* Hash content */
    ReportRow* row = report->rows;
    while (row) {
        for (int i = 0; i < report->column_count; i++) {
            if (row->values[i]) {
                SHA256_Update(&ctx, row->values[i], strlen(row->values[i]));
            }
        }
        row = row->next;
    }

    uint8_t hash[32];
    SHA256_Final(hash, &ctx);

    /* Sign hash */
    SigningKey* key = get_report_signing_key();
    report->signature = sign_data(key, hash, 32, &report->signature_len);
    report->signing_key_id = strdup(key->key_id);
}
```

### Compliance Dashboard Data

sql

```sql
-- Compliance status overview
CREATE VIEW sys.compliance_dashboard AS
SELECT 
    framework,
    total_controls,
    passing_controls,
    failing_controls,
    not_tested_controls,
    ROUND(passing_controls * 100.0 / NULLIF(total_controls, 0), 1) as compliance_percent,
    last_assessment_date,
    next_assessment_date
FROM (
    SELECT 
        r.report_framework as framework,
        COUNT(DISTINCT r.control_reference) as total_controls,
        COUNT(DISTINCT CASE WHEN s.status = 'PASS' THEN r.control_reference END) as passing_controls,
        COUNT(DISTINCT CASE WHEN s.status = 'FAIL' THEN r.control_reference END) as failing_controls,
        COUNT(DISTINCT CASE WHEN s.status IS NULL THEN r.control_reference END) as not_tested_controls,
        MAX(s.assessed_at) as last_assessment_date,
        MAX(s.assessed_at) + interval '90 days' as next_assessment_date
    FROM sys.compliance_report_definitions r
    LEFT JOIN sys.control_assessment_status s 
        ON r.control_reference = s.control_reference
        AND r.report_framework = s.framework
    GROUP BY r.report_framework
) sub;

-- Control assessment tracking
CREATE TABLE sys.control_assessment_status (
    assessment_id           UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    framework               VARCHAR(32) NOT NULL,
    control_reference       VARCHAR(64) NOT NULL,

    status                  ENUM('PASS', 'FAIL', 'PARTIAL', 'NOT_APPLICABLE') NOT NULL,
    assessed_at             TIMESTAMP NOT NULL,
    assessed_by             UUID,

    evidence_report_ids     UUID[],  -- links to generated reports
    notes                   TEXT,
    remediation_required    BOOLEAN DEFAULT FALSE,
    remediation_deadline    DATE,
    remediation_status      VARCHAR(64),

    UNIQUE (framework, control_reference, assessed_at),
    INDEX idx_framework (framework),
    INDEX idx_status (status)
);
```

## Forensic Analysis

### Event Correlation

sql

```sql
-- Correlate events across sessions and time
CREATE FUNCTION sys.correlate_events(
    p_start_time TIMESTAMP,
    p_end_time TIMESTAMP,
    p_correlation_window_seconds INTEGER DEFAULT 60
) RETURNS TABLE (
    correlation_id UUID,
    event_count INTEGER,
    actors TEXT[],
    event_types TEXT[],
    targets TEXT[],
    first_event TIMESTAMP,
    last_event TIMESTAMP,
    severity TEXT
) AS $$
BEGIN
    RETURN QUERY
    WITH event_groups AS (
        SELECT 
            e.*,
            SUM(CASE WHEN prev_time IS NULL 
                     OR event_timestamp - prev_time > (p_correlation_window_seconds || ' seconds')::interval
                THEN 1 ELSE 0 END) OVER (ORDER BY event_timestamp) as group_id
        FROM (
            SELECT 
                *,
                LAG(event_timestamp) OVER (
                    PARTITION BY source_address 
                    ORDER BY event_timestamp
                ) as prev_time
            FROM sys.audit_events
            WHERE event_timestamp >= p_start_time
            AND event_timestamp < p_end_time
        ) e
    )
    SELECT 
        gen_random_uuid() as correlation_id,
        COUNT(*)::INTEGER as event_count,
        array_agg(DISTINCT actor_username) FILTER (WHERE actor_username IS NOT NULL) as actors,
        array_agg(DISTINCT event_type) as event_types,
        array_agg(DISTINCT target_name) FILTER (WHERE target_name IS NOT NULL) as targets,
        MIN(event_timestamp) as first_event,
        MAX(event_timestamp) as last_event,
        MAX(event_severity::text) as severity
    FROM event_groups
    GROUP BY group_id, source_address
    HAVING COUNT(*) > 1
    ORDER BY first_event;
END;
$$ LANGUAGE plpgsql;
```

### Timeline Reconstruction

c

```c
/*
 * Reconstruct activity timeline for forensic analysis.
 */
int build_activity_timeline(
    TimelineQuery*      query,
    ActivityTimeline**  timeline
) {
    *timeline = allocate_timeline();
    (*timeline)->query = dup_query(query);
    (*timeline)->generated_at = current_timestamp();

    /* Build main query */
    StringBuilder* sql = sb_new();
    sb_append(sql, "SELECT * FROM sys.audit_events WHERE 1=1 ");

    /* Apply filters */
    if (query->user_uuid) {
        sb_append(sql, "AND actor_user_uuid = :user_uuid ");
    }
    if (query->session_id) {
        sb_append(sql, "AND actor_session_id = :session_id ");
    }
    if (query->source_address) {
        sb_append(sql, "AND source_address = :source_address ");
    }
    if (query->target_object) {
        sb_append(sql, "AND (target_name = :target OR target_uuid = :target_uuid) ");
    }
    if (query->categories) {
        sb_append(sql, "AND event_category = ANY(:categories) ");
    }

    sb_append(sql, "AND event_timestamp >= :start_time ");
    sb_append(sql, "AND event_timestamp < :end_time ");
    sb_append(sql, "ORDER BY event_timestamp ");

    /* Execute query */
    PreparedStatement* stmt = prepare_statement(sb_finish(sql));
    bind_parameters(stmt, query);

    ResultSet* rs = execute_query(stmt);

    /* Build timeline entries */
    TimelineEntry* prev_entry = NULL;
    while (resultset_next(rs)) {
        TimelineEntry* entry = allocate_timeline_entry();

        entry->timestamp = resultset_get_timestamp(rs, "event_timestamp");
        entry->event_id = resultset_get_uuid(rs, "event_id");
        entry->category = resultset_get_string(rs, "event_category");
        entry->event_type = resultset_get_string(rs, "event_type");
        entry->actor = resultset_get_string(rs, "actor_username");
        entry->target = resultset_get_string(rs, "target_name");
        entry->outcome = resultset_get_string(rs, "event_outcome");
        entry->details = resultset_get_json(rs, "action_parameters");

        /* Calculate time delta from previous event */
        if (prev_entry) {
            entry->time_delta_ms = 
                (entry->timestamp - prev_entry->timestamp) / 1000;
        }

        /* Link entries */
        if (prev_entry) {
            prev_entry->next = entry;
            entry->prev = prev_entry;
        } else {
            (*timeline)->entries = entry;
        }
        prev_entry = entry;
        (*timeline)->entry_count++;
    }

    resultset_close(rs);
    statement_close(stmt);

    /* Enrich timeline with additional context */
    enrich_timeline(*timeline);

    return OK;
}

/*
 * Enrich timeline with context (session info, related events, etc.)
 */
void enrich_timeline(ActivityTimeline* timeline) {
    TimelineEntry* entry = timeline->entries;

    while (entry) {
        /* Add session context if not already loaded */
        if (entry->session_id && !entry->session_context) {
            entry->session_context = get_session_info(entry->session_id);
        }

        /* Find related events (same transaction, same target, etc.) */
        entry->related_events = find_related_events(entry);

        /* Add threat intelligence context if available */
        if (entry->source_address) {
            entry->threat_intel = lookup_threat_intel(entry->source_address);
        }

        entry = entry->next;
    }
}
```

### Anomaly Detection

sql

```sql
-- Baseline normal behavior
CREATE TABLE sys.user_behavior_baseline (
    user_uuid               UUID NOT NULL,
    metric_name             VARCHAR(64) NOT NULL,
    period_type             ENUM('HOURLY', 'DAILY', 'WEEKLY') NOT NULL,
    period_value            INTEGER NOT NULL,  -- hour of day, day of week, etc.

    -- Statistics
    mean_value              FLOAT NOT NULL,
    stddev_value            FLOAT NOT NULL,
    min_value               FLOAT,
    max_value               FLOAT,
    sample_count            INTEGER NOT NULL,

    -- Metadata
    calculated_at           TIMESTAMP NOT NULL,
    data_start              TIMESTAMP NOT NULL,
    data_end                TIMESTAMP NOT NULL,

    PRIMARY KEY (user_uuid, metric_name, period_type, period_value)
);

-- Detected anomalies
CREATE TABLE sys.behavior_anomalies (
    anomaly_id              UUID PRIMARY KEY DEFAULT gen_uuid_v7(),
    detected_at             TIMESTAMP NOT NULL DEFAULT now(),

    -- Subject
    user_uuid               UUID,
    source_address          INET,

    -- Anomaly details
    anomaly_type            VARCHAR(64) NOT NULL,
    metric_name             VARCHAR(64) NOT NULL,
    observed_value          FLOAT NOT NULL,
    expected_value          FLOAT NOT NULL,
    deviation_score         FLOAT NOT NULL,  -- number of stddevs

    -- Context
    period_type             VARCHAR(16),
    period_value            INTEGER,
    related_events          UUID[],

    -- Status
    status                  ENUM('NEW', 'INVESTIGATING', 'FALSE_POSITIVE', 
                                 'CONFIRMED', 'RESOLVED') DEFAULT 'NEW',
    reviewed_by             UUID,
    reviewed_at             TIMESTAMP,
    notes                   TEXT,

    INDEX idx_detected (detected_at),
    INDEX idx_user (user_uuid, detected_at),
    INDEX idx_status (status)
);
```

c

```c
/*
 * Detect anomalies in user behavior.
 */
void detect_user_anomalies(uuid_t user_uuid, AuditEvent* event) {
    /* Get current metrics */
    UserMetrics current = calculate_current_metrics(user_uuid, event);

    /* Get baselines */
    int hour_of_day = get_hour(event->event_timestamp);
    int day_of_week = get_day_of_week(event->event_timestamp);

    /* Check each metric against baseline */
    const char* metrics[] = {
        "login_count", "query_count", "rows_accessed", 
        "distinct_tables", "failed_operations", NULL
    };

    for (int i = 0; metrics[i]; i++) {
        Baseline* baseline = get_baseline(user_uuid, metrics[i], 
                                          PERIOD_HOURLY, hour_of_day);
        if (!baseline || baseline->sample_count < MIN_SAMPLES) {
            continue;
        }

        float observed = get_metric_value(&current, metrics[i]);
        float deviation = (observed - baseline->mean_value) / baseline->stddev_value;

        if (fabs(deviation) > ANOMALY_THRESHOLD) {
            /* Anomaly detected */
            BehaviorAnomaly anomaly = {
                .anomaly_id = generate_uuid_v7(),
                .detected_at = current_timestamp(),
                .user_uuid = user_uuid,
                .anomaly_type = deviation > 0 ? "HIGH_ACTIVITY" : "LOW_ACTIVITY",
                .metric_name = metrics[i],
                .observed_value = observed,
                .expected_value = baseline->mean_value,
                .deviation_score = deviation,
                .period_type = "HOURLY",
                .period_value = hour_of_day,
                .related_events = {event->event_id},
                .status = ANOMALY_NEW
            };

            store_anomaly(&anomaly);

            /* Trigger alert if severe */
            if (fabs(deviation) > SEVERE_ANOMALY_THRESHOLD) {
                trigger_anomaly_alert(&anomaly);
            }
        }
    }
}

/*
 * Update behavior baselines (run periodically).
 */
void update_behavior_baselines(void) {
    /* Get list of active users */
    UserList* users = get_active_users_last_30_days();

    for (int i = 0; i < users->count; i++) {
        uuid_t user_uuid = users->users[i];

        /* Calculate hourly baselines */
        for (int hour = 0; hour < 24; hour++) {
            UserMetrics metrics = calculate_hourly_metrics(user_uuid, hour, 30);

            update_baseline(user_uuid, "login_count", PERIOD_HOURLY, hour,
                           metrics.login_count_mean, metrics.login_count_stddev,
                           metrics.sample_count);

            update_baseline(user_uuid, "query_count", PERIOD_HOURLY, hour,
                           metrics.query_count_mean, metrics.query_count_stddev,
                           metrics.sample_count);

            /* ... other metrics ... */
        }

        /* Calculate daily baselines */
        for (int day = 0; day < 7; day++) {
            UserMetrics metrics = calculate_daily_metrics(user_uuid, day, 12);
            /* ... update baselines ... */
        }
    }
}
```

## Policy Configuration Summary

| Policy Key                            | Type    | Default                            | Description                      |
| ------------------------------------- | ------- | ---------------------------------- | -------------------------------- |
| `audit.enabled`                       | BOOLEAN | TRUE                               | Enable audit logging             |
| `audit.default_categories`            | ENUM[]  | [AUTH, SECURITY_ADMIN, ENCRYPTION] | Categories to audit by default   |
| `audit.log_statement_text`            | BOOLEAN | FALSE                              | Include SQL in audit events      |
| `audit.redact_sensitive`              | BOOLEAN | TRUE                               | Redact sensitive values          |
| `audit.chain_hash_enabled`            | BOOLEAN | TRUE                               | Enable tamper-evident chaining   |
| `audit.sign_enabled`                  | BOOLEAN | FALSE                              | Sign audit blocks                |
| `audit.sign_interval_events`          | INTEGER | 10000                              | Events between signatures        |
| `audit.retention.default_days`        | INTEGER | 90                                 | Default retention period         |
| `audit.retention.compliance_override` | BOOLEAN | TRUE                               | Respect compliance minimums      |
| `audit.archive.enabled`               | BOOLEAN | TRUE                               | Archive before delete            |
| `audit.archive.format`                | ENUM    | PARQUET                            | Archive file format              |
| `audit.archive.encryption`            | BOOLEAN | TRUE                               | Encrypt archives                 |
| `audit.stream.enabled`                | BOOLEAN | FALSE                              | Enable SIEM streaming            |
| `audit.stream.format`                 | ENUM    | JSON                               | Stream output format             |
| `audit.alert.enabled`                 | BOOLEAN | TRUE                               | Enable alert processing          |
| `audit.alert.dedup_window_seconds`    | INTEGER | 300                                | Alert deduplication window       |
| `audit.anomaly.enabled`               | BOOLEAN | FALSE                              | Enable anomaly detection         |
| `audit.anomaly.threshold_stddev`      | FLOAT   | 3.0                                | Anomaly detection threshold      |
| `audit.compliance.auto_reports`       | BOOLEAN | FALSE                              | Auto-generate compliance reports |
| `audit.compliance.report_schedule`    | STRING  | 0 0 1 * *                          | Report generation schedule       |
