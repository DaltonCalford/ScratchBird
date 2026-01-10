# Alpha 3 Test Plan

## 1. Overview

This document defines the comprehensive test plan for Alpha 3 (Service/Daemon Mode), covering:
- Protocol compliance testing for all 5 wire protocols
- Authentication method testing across 11 auth mechanisms
- Load and stress testing scenarios
- Security penetration testing
- Performance benchmarking

**Test Coverage Target:** 100% of Alpha 3 features
**Estimated Test Cases:** ~500 functional + ~100 performance + ~50 security

---

## 2. Test Environment

### 2.1 Hardware Requirements

| Environment | CPU | RAM | Storage | Network |
|-------------|-----|-----|---------|---------|
| Unit Tests | 2 cores | 4 GB | 20 GB SSD | localhost |
| Integration | 4 cores | 16 GB | 100 GB SSD | 1 Gbps |
| Load Testing | 8 cores | 32 GB | 500 GB NVMe | 10 Gbps |
| Penetration | 4 cores | 16 GB | 100 GB SSD | Isolated VLAN |

### 2.2 Software Requirements

```
- OS: Ubuntu 22.04 LTS / Debian 12 / RHEL 9
- Compilers: GCC 12+, Clang 15+
- Testing: Google Test, Catch2
- Load Testing: pgbench, sysbench, custom tools
- Security: OWASP ZAP, Nmap, sqlmap, custom fuzzers
- Monitoring: Prometheus, Grafana
```

### 2.3 Test Database Configurations

| Config | Description | Use Case |
|--------|-------------|----------|
| `tiny` | 1 MB buffer pool, 10 connections | Unit tests |
| `small` | 256 MB buffer pool, 50 connections | Integration |
| `medium` | 2 GB buffer pool, 200 connections | Load tests |
| `large` | 16 GB buffer pool, 1000 connections | Stress tests |

---

## 3. Protocol Compliance Test Suites

### 3.1 PostgreSQL Protocol (Port 5432)

#### 3.1.1 Connection Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| PG-CONN-001 | StartupMessage with valid credentials | AuthenticationOk + ReadyForQuery |
| PG-CONN-002 | StartupMessage with invalid password | ErrorResponse (28P01) |
| PG-CONN-003 | StartupMessage with unknown user | ErrorResponse (28000) |
| PG-CONN-004 | StartupMessage with invalid database | ErrorResponse (3D000) |
| PG-CONN-005 | SSL negotiation (SSLRequest) | 'S' response, TLS handshake |
| PG-CONN-006 | Cancel request (CancelRequest) | Query cancelled |
| PG-CONN-007 | Protocol version mismatch | ErrorResponse or upgrade |
| PG-CONN-008 | Connection with MD5 auth | MD5 challenge-response |
| PG-CONN-009 | Connection with SCRAM-SHA-256 | SASL exchange |
| PG-CONN-010 | Max connections exceeded | ErrorResponse (53300) |

#### 3.1.2 Query Protocol Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| PG-QUERY-001 | Simple Query (single SELECT) | RowDescription + DataRow(s) + CommandComplete |
| PG-QUERY-002 | Simple Query (multiple statements) | Multiple result sets |
| PG-QUERY-003 | Empty query | EmptyQueryResponse |
| PG-QUERY-004 | Syntax error | ErrorResponse (42601) |
| PG-QUERY-005 | Query with NULL values | DataRow with -1 length |
| PG-QUERY-006 | Query exceeding row limit | Proper truncation |
| PG-QUERY-007 | Query timeout | ErrorResponse (57014) |
| PG-QUERY-008 | Query cancel during execution | ErrorResponse (57014) |

#### 3.1.3 Extended Query Protocol Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| PG-EXT-001 | Parse + Bind + Execute | ParseComplete + BindComplete + results |
| PG-EXT-002 | Named prepared statement | Reusable statement |
| PG-EXT-003 | Parameter binding (all types) | Correct type conversion |
| PG-EXT-004 | Describe statement | ParameterDescription + RowDescription |
| PG-EXT-005 | Describe portal | RowDescription |
| PG-EXT-006 | Close statement | CloseComplete |
| PG-EXT-007 | Execute with row limit | PortalSuspended at limit |
| PG-EXT-008 | Sync after error | ReadyForQuery (error cleared) |
| PG-EXT-009 | Binary format parameters | Correct binary encoding |
| PG-EXT-010 | Binary format results | Correct binary decoding |

#### 3.1.4 COPY Protocol Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| PG-COPY-001 | COPY TO STDOUT (text) | CopyOutResponse + CopyData + CopyDone |
| PG-COPY-002 | COPY FROM STDIN (text) | CopyInResponse, data accepted |
| PG-COPY-003 | COPY with binary format | Binary COPY protocol |
| PG-COPY-004 | COPY with CSV format | CSV parsing/generation |
| PG-COPY-005 | COPY cancel | CopyFail accepted |

#### 3.1.5 Type Serialization Tests

| Test ID | Type | Test Cases |
|---------|------|------------|
| PG-TYPE-001 | BOOLEAN | true, false, NULL |
| PG-TYPE-002 | SMALLINT | -32768, 0, 32767, NULL |
| PG-TYPE-003 | INTEGER | min, max, NULL |
| PG-TYPE-004 | BIGINT | min, max, NULL |
| PG-TYPE-005 | REAL | ±3.4E38, NaN, Inf, NULL |
| PG-TYPE-006 | DOUBLE | ±1.7E308, NaN, Inf, NULL |
| PG-TYPE-007 | NUMERIC | precision 1-131072, scale 0-16383 |
| PG-TYPE-008 | VARCHAR | empty, ASCII, UTF-8, max length |
| PG-TYPE-009 | TEXT | multi-MB strings |
| PG-TYPE-010 | BYTEA | empty, binary data, hex format |
| PG-TYPE-011 | DATE | min, max, BC dates |
| PG-TYPE-012 | TIME | 00:00:00 to 23:59:59.999999 |
| PG-TYPE-013 | TIMESTAMP | full range with microseconds |
| PG-TYPE-014 | TIMESTAMPTZ | timezone handling |
| PG-TYPE-015 | INTERVAL | all interval components |
| PG-TYPE-016 | UUID | valid and NULL |
| PG-TYPE-017 | JSON/JSONB | objects, arrays, nested |
| PG-TYPE-018 | ARRAY | 1D, multi-dimensional |
| PG-TYPE-019 | INET/CIDR | IPv4, IPv6 |
| PG-TYPE-020 | Range types | int4range, tsrange, etc. |

### 3.2 MySQL Protocol (Port 3306)

#### 3.2.1 Connection Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| MY-CONN-001 | Handshake with mysql_native_password | OK packet |
| MY-CONN-002 | Handshake with caching_sha2_password | OK packet |
| MY-CONN-003 | Invalid password | ERR packet (1045) |
| MY-CONN-004 | SSL/TLS connection | Encrypted connection |
| MY-CONN-005 | COM_QUIT | Connection closed |
| MY-CONN-006 | COM_PING | OK packet |
| MY-CONN-007 | COM_RESET_CONNECTION | Connection reset |
| MY-CONN-008 | COM_INIT_DB | Database switched |
| MY-CONN-009 | Capability negotiation | Correct flags |
| MY-CONN-010 | Character set selection | UTF8MB4 default |

#### 3.2.2 Query Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| MY-QUERY-001 | COM_QUERY simple SELECT | Result set |
| MY-QUERY-002 | COM_QUERY INSERT | OK packet with affected rows |
| MY-QUERY-003 | COM_QUERY multi-statement | Multiple result sets |
| MY-QUERY-004 | COM_STMT_PREPARE | Prepared statement OK |
| MY-QUERY-005 | COM_STMT_EXECUTE | Binary result set |
| MY-QUERY-006 | COM_STMT_CLOSE | Statement deallocated |
| MY-QUERY-007 | COM_STMT_RESET | Bindings cleared |
| MY-QUERY-008 | COM_FIELD_LIST | Column definitions |
| MY-QUERY-009 | LOCAL INFILE | Rejected (security) |
| MY-QUERY-010 | Query timeout | ERR packet (1317) |

### 3.3 TDS Protocol - SQL Server (Port 1433, post-gold)

**Scope Note:** TDS/MSSQL testing is post-gold and is not part of Alpha validation. This section is retained for future coverage.

#### 3.3.1 Connection Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| TDS-CONN-001 | PRELOGIN exchange | PRELOGIN response |
| TDS-CONN-002 | TDS7 LOGIN | LOGINACK token |
| TDS-CONN-003 | SQL auth | Authentication success |
| TDS-CONN-004 | Windows auth (NTLM) | SSPI exchange |
| TDS-CONN-005 | Encryption negotiation | Encrypted connection |
| TDS-CONN-006 | Invalid credentials | ERROR token (18456) |
| TDS-CONN-007 | Database context switch | ENVCHANGE token |
| TDS-CONN-008 | TDS version negotiation | Compatible version |

#### 3.3.2 Query Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| TDS-QUERY-001 | SQL_BATCH simple query | COLMETADATA + ROW + DONE |
| TDS-QUERY-002 | RPC stored procedure | RETURNSTATUS + results |
| TDS-QUERY-003 | Parameterized query | sp_executesql |
| TDS-QUERY-004 | Multiple result sets | Multiple DONE tokens |
| TDS-QUERY-005 | NBCROW (null bitmap) | Correct null handling |
| TDS-QUERY-006 | Large result set | Streaming results |
| TDS-QUERY-007 | ATTENTION signal | Query cancelled |
| TDS-QUERY-008 | Transaction manager | BEGIN/COMMIT/ROLLBACK |

### 3.4 Firebird Protocol (Port 3050)

#### 3.4.1 Connection Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| FB-CONN-001 | op_connect + op_attach | op_accept + op_response |
| FB-CONN-002 | Protocol version negotiation | Compatible version |
| FB-CONN-003 | SRP authentication | SRPP exchange |
| FB-CONN-004 | Legacy authentication | Accepted with warning |
| FB-CONN-005 | Wire encryption | ARC4/ChaCha encrypted |
| FB-CONN-006 | Invalid credentials | op_response with error |
| FB-CONN-007 | op_detach | Clean disconnect |
| FB-CONN-008 | Database path validation | Security check |

#### 3.4.2 Query Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| FB-QUERY-001 | op_allocate_statement | Statement handle |
| FB-QUERY-002 | op_prepare | Prepared statement |
| FB-QUERY-003 | op_execute | Execution results |
| FB-QUERY-004 | op_fetch | Row data |
| FB-QUERY-005 | op_free_statement | Statement freed |
| FB-QUERY-006 | op_info_sql | Statement metadata |
| FB-QUERY-007 | BLOB handling | Read/write BLOBs |
| FB-QUERY-008 | ARRAY handling | Read/write arrays |

### 3.5 ScratchBird Native Protocol (Port 3092)

#### 3.5.1 Connection Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| SB-CONN-001 | STARTUP message | AUTH_REQUEST |
| SB-CONN-002 | Password authentication | AUTH_OK |
| SB-CONN-003 | Cluster PKI authentication | CLUSTER_AUTH_OK |
| SB-CONN-004 | TLS 1.3 handshake | Encrypted connection |
| SB-CONN-005 | zstd compression | Compressed frames |
| SB-CONN-006 | Protocol version check | Compatible version |
| SB-CONN-007 | Session key derivation | HKDF success |
| SB-CONN-008 | Heartbeat/keepalive | PING/PONG |

#### 3.5.2 Query Tests

| Test ID | Test Case | Expected Result |
|---------|-----------|-----------------|
| SB-QUERY-001 | QUERY message | ROW_DESCRIPTION + DATA_ROW |
| SB-QUERY-002 | PARSE + BIND + EXECUTE | Extended query flow |
| SB-QUERY-003 | SBLR bytecode transmission | Direct execution |
| SB-QUERY-004 | Streaming results | Backpressure handling |
| SB-QUERY-005 | Query plan transmission | EXPLAIN output |
| SB-QUERY-006 | Async query execution | Non-blocking results |
| SB-QUERY-007 | FEDERATED_QUERY | Cross-database query |
| SB-QUERY-008 | SUBSCRIBE/NOTIFY | Pub/sub messaging |

#### 3.5.3 All 86 Type Tests

| Test ID | Type Category | Types Tested |
|---------|---------------|--------------|
| SB-TYPE-001 | Boolean | BOOLEAN |
| SB-TYPE-002 | Integers | SMALLINT, INTEGER, BIGINT |
| SB-TYPE-003 | Floating | REAL, DOUBLE, DECIMAL |
| SB-TYPE-004 | Character | CHAR, VARCHAR, TEXT |
| SB-TYPE-005 | Binary | BLOB, BYTEA |
| SB-TYPE-006 | Date/Time | DATE, TIME, TIMESTAMP, INTERVAL |
| SB-TYPE-007 | Network | INET, CIDR, MACADDR |
| SB-TYPE-008 | Geometric | POINT, LINE, POLYGON, etc. |
| SB-TYPE-009 | Range | INT4RANGE, TSRANGE, etc. |
| SB-TYPE-010 | JSON | JSON, JSONB |
| SB-TYPE-011 | UUID | UUID |
| SB-TYPE-012 | Arrays | All array types |
| SB-TYPE-013 | Composite | User-defined types |

---

## 4. Authentication Test Matrix

### 4.1 Authentication Methods

| Method | Protocol Support | Test Priority |
|--------|------------------|---------------|
| Password (plaintext) | All | P0 |
| MD5 | PostgreSQL | P0 |
| SCRAM-SHA-256 | PostgreSQL, Native | P0 |
| mysql_native_password | MySQL | P0 |
| caching_sha2_password | MySQL | P0 |
| NTLM | TDS (post-gold) | P1 |
| Kerberos/GSSAPI | All | P1 |
| LDAP | All | P1 |
| Certificate (mTLS) | All | P1 |
| SAML 2.0 | Native | P2 |
| OAuth 2.0 | Native | P2 |
| MFA (TOTP) | Native | P2 |

### 4.2 Authentication Test Cases

| Test ID | Method | Test Case | Expected |
|---------|--------|-----------|----------|
| AUTH-001 | Password | Valid credentials | Success |
| AUTH-002 | Password | Invalid password | Failure (28P01) |
| AUTH-003 | Password | Unknown user | Failure (28000) |
| AUTH-004 | Password | Empty password | Failure |
| AUTH-005 | Password | Password with special chars | Success |
| AUTH-006 | MD5 | Correct MD5 hash | Success |
| AUTH-007 | MD5 | Incorrect hash | Failure |
| AUTH-008 | SCRAM | Full SCRAM exchange | Success |
| AUTH-009 | SCRAM | Invalid client proof | Failure |
| AUTH-010 | SCRAM | Channel binding | Success |
| AUTH-011 | Kerberos | Valid ticket | Success |
| AUTH-012 | Kerberos | Expired ticket | Failure |
| AUTH-013 | Kerberos | Invalid principal | Failure |
| AUTH-014 | LDAP | Valid LDAP bind | Success |
| AUTH-015 | LDAP | LDAP server unreachable | Graceful failure |
| AUTH-016 | LDAP | Group membership check | Success/Failure |
| AUTH-017 | Certificate | Valid client cert | Success |
| AUTH-018 | Certificate | Expired cert | Failure |
| AUTH-019 | Certificate | Revoked cert (CRL) | Failure |
| AUTH-020 | Certificate | Wrong CA | Failure |
| AUTH-021 | MFA | Valid TOTP code | Success |
| AUTH-022 | MFA | Invalid TOTP code | Failure |
| AUTH-023 | MFA | Expired TOTP code | Failure |
| AUTH-024 | MFA | Replay attack | Failure |
| AUTH-025 | OAuth | Valid access token | Success |
| AUTH-026 | OAuth | Expired token | Failure |
| AUTH-027 | OAuth | Invalid scope | Failure |

### 4.3 Authorization Test Cases

| Test ID | Test Case | Expected |
|---------|-----------|----------|
| AUTHZ-001 | SELECT on owned table | Success |
| AUTHZ-002 | SELECT without permission | Failure (42501) |
| AUTHZ-003 | INSERT without permission | Failure |
| AUTHZ-004 | Role-based access | Inherited permissions |
| AUTHZ-005 | Group-based access | Group permissions |
| AUTHZ-006 | Row-level security | Filtered results |
| AUTHZ-007 | Column-level security | Column redaction |
| AUTHZ-008 | Schema permissions | Schema access control |
| AUTHZ-009 | GRANT/REVOKE | Permission changes |
| AUTHZ-010 | DENY override | Explicit deny |

---

## 5. Load Testing Scenarios

### 5.1 Connection Load Tests

| Test ID | Scenario | Parameters | Success Criteria |
|---------|----------|------------|------------------|
| LOAD-CONN-001 | Concurrent connections | 100 simultaneous | All connect < 100ms |
| LOAD-CONN-002 | Connection storm | 1000 in 10s | 95% success |
| LOAD-CONN-003 | Connection pool cycling | 10K connect/disconnect | No leaks |
| LOAD-CONN-004 | Max connections | Reach configured max | Graceful rejection |
| LOAD-CONN-005 | Long-lived connections | 24h with activity | Stable memory |
| LOAD-CONN-006 | Idle connection timeout | Connections idle 5min | Proper cleanup |
| LOAD-CONN-007 | Reconnection storm | 500 reconnects/s | Server stable |

### 5.2 Query Load Tests

| Test ID | Scenario | Parameters | Success Criteria |
|---------|----------|------------|------------------|
| LOAD-QUERY-001 | Simple SELECT throughput | 10K queries/s | < 10ms p99 |
| LOAD-QUERY-002 | Complex JOIN throughput | 1K queries/s | < 100ms p99 |
| LOAD-QUERY-003 | INSERT throughput | 50K rows/s | Sustained |
| LOAD-QUERY-004 | UPDATE throughput | 20K rows/s | Sustained |
| LOAD-QUERY-005 | Mixed OLTP workload | 80/20 read/write | < 50ms p99 |
| LOAD-QUERY-006 | Prepared statement reuse | 100K executions | Cache effective |
| LOAD-QUERY-007 | Large result sets | 1M rows | Streaming works |
| LOAD-QUERY-008 | Concurrent transactions | 500 concurrent | No deadlocks |

### 5.3 Protocol-Specific Load Tests

| Test ID | Protocol | Scenario | Target |
|---------|----------|----------|--------|
| LOAD-PG-001 | PostgreSQL | pgbench TPC-B | 10K TPS |
| LOAD-PG-002 | PostgreSQL | COPY bulk load | 100K rows/s |
| LOAD-MY-001 | MySQL | sysbench OLTP | 10K TPS |
| LOAD-MY-002 | MySQL | Bulk INSERT | 50K rows/s |
| LOAD-TDS-001 | TDS (post-gold) | BCP bulk load | 100K rows/s |
| LOAD-FB-001 | Firebird | gbak restore | 1 GB/min |
| LOAD-SB-001 | Native | SBLR throughput | 50K TPS |
| LOAD-SB-002 | Native | Federated queries | 1K TPS |

### 5.4 Stress Test Scenarios

| Test ID | Scenario | Duration | Monitoring |
|---------|----------|----------|------------|
| STRESS-001 | 80% CPU sustained | 4 hours | No OOM, no crash |
| STRESS-002 | Memory pressure | 4 hours | Graceful degradation |
| STRESS-003 | Disk I/O saturation | 2 hours | Queue management |
| STRESS-004 | Network congestion | 2 hours | Backpressure works |
| STRESS-005 | Mixed protocol storm | 8 hours | All protocols stable |
| STRESS-006 | Checkpoint under load | 1 hour | Checkpoint completes |
| STRESS-007 | Garbage collection load | 4 hours | GC keeps up |

### 5.5 Endurance Tests

| Test ID | Scenario | Duration | Metrics |
|---------|----------|----------|---------|
| ENDURE-001 | Continuous OLTP | 72 hours | Memory stable |
| ENDURE-002 | Connection cycling | 72 hours | No leaks |
| ENDURE-003 | Log rotation | 7 days | Logs rotate |
| ENDURE-004 | Statistics collection | 7 days | Stats accurate |
| ENDURE-005 | Hot config reload | 100 reloads | No disruption |

---

## 6. Security Penetration Test Checklist

### 6.1 Network Security

| Test ID | Test Case | Tools | Risk |
|---------|-----------|-------|------|
| SEC-NET-001 | Port scanning | nmap | Info disclosure |
| SEC-NET-002 | Service fingerprinting | nmap -sV | Info disclosure |
| SEC-NET-003 | TLS version check | testssl.sh | Weak crypto |
| SEC-NET-004 | TLS cipher suite check | testssl.sh | Weak ciphers |
| SEC-NET-005 | Certificate validation | openssl | MITM |
| SEC-NET-006 | Protocol downgrade attack | Custom | Protocol bypass |
| SEC-NET-007 | Replay attack | Wireshark + custom | Session hijack |
| SEC-NET-008 | Man-in-the-middle | mitmproxy | Data intercept |
| SEC-NET-009 | DNS rebinding | Custom | Access bypass |
| SEC-NET-010 | TCP reset attack | hping3 | DoS |

### 6.2 Authentication Attacks

| Test ID | Test Case | Tools | Risk |
|---------|-----------|-------|------|
| SEC-AUTH-001 | Brute force password | hydra, custom | Account compromise |
| SEC-AUTH-002 | Credential stuffing | Custom | Account takeover |
| SEC-AUTH-003 | Password spraying | Custom | Mass compromise |
| SEC-AUTH-004 | Timing attack on auth | Custom | Username enum |
| SEC-AUTH-005 | Session fixation | Custom | Session hijack |
| SEC-AUTH-006 | Session hijacking | Custom | Impersonation |
| SEC-AUTH-007 | Token prediction | Custom | Auth bypass |
| SEC-AUTH-008 | TOTP bypass attempts | Custom | MFA bypass |
| SEC-AUTH-009 | OAuth token theft | Custom | Token misuse |
| SEC-AUTH-010 | Kerberos ticket attacks | Mimikatz concepts | Ticket abuse |

### 6.3 SQL Injection

| Test ID | Test Case | Tools | Risk |
|---------|-----------|-------|------|
| SEC-SQL-001 | Classic SQL injection | sqlmap | Data breach |
| SEC-SQL-002 | Blind SQL injection | sqlmap | Data exfil |
| SEC-SQL-003 | Time-based injection | sqlmap | Data exfil |
| SEC-SQL-004 | UNION-based injection | sqlmap | Data breach |
| SEC-SQL-005 | Stacked queries | sqlmap | Command exec |
| SEC-SQL-006 | Second-order injection | Manual | Delayed attack |
| SEC-SQL-007 | Prepared statement bypass | Custom | Param pollution |
| SEC-SQL-008 | Comment injection | Manual | Filter bypass |
| SEC-SQL-009 | Encoding bypass (UTF-8) | Manual | Filter bypass |
| SEC-SQL-010 | Stored procedure injection | Manual | Privilege escalation |

### 6.4 Protocol Fuzzing

| Test ID | Test Case | Tools | Risk |
|---------|-----------|-------|------|
| SEC-FUZZ-001 | PostgreSQL protocol fuzz | Custom fuzzer | Crash/RCE |
| SEC-FUZZ-002 | MySQL protocol fuzz | Custom fuzzer | Crash/RCE |
| SEC-FUZZ-003 | TDS protocol fuzz (post-gold) | Custom fuzzer | Crash/RCE |
| SEC-FUZZ-004 | Firebird protocol fuzz | Custom fuzzer | Crash/RCE |
| SEC-FUZZ-005 | Native protocol fuzz | Custom fuzzer | Crash/RCE |
| SEC-FUZZ-006 | Malformed packets | Custom | Crash |
| SEC-FUZZ-007 | Oversized packets | Custom | Buffer overflow |
| SEC-FUZZ-008 | Truncated packets | Custom | State corruption |
| SEC-FUZZ-009 | Out-of-order packets | Custom | State confusion |
| SEC-FUZZ-010 | Invalid type OIDs | Custom | Type confusion |

### 6.5 Authorization Bypass

| Test ID | Test Case | Tools | Risk |
|---------|-----------|-------|------|
| SEC-AUTHZ-001 | Horizontal privilege escalation | Manual | Data breach |
| SEC-AUTHZ-002 | Vertical privilege escalation | Manual | Admin access |
| SEC-AUTHZ-003 | IDOR (direct object reference) | Manual | Data access |
| SEC-AUTHZ-004 | Role confusion | Manual | Priv escalation |
| SEC-AUTHZ-005 | RLS bypass attempts | Manual | Data leak |
| SEC-AUTHZ-006 | Schema escape | Manual | Cross-schema access |
| SEC-AUTHZ-007 | System catalog access | Manual | Info disclosure |
| SEC-AUTHZ-008 | GRANT privilege abuse | Manual | Priv escalation |
| SEC-AUTHZ-009 | Ownership transfer exploit | Manual | Object hijack |
| SEC-AUTHZ-010 | PUBLIC role abuse | Manual | Default access |

### 6.6 Denial of Service

| Test ID | Test Case | Tools | Risk |
|---------|-----------|-------|------|
| SEC-DOS-001 | Connection exhaustion | Custom | Service unavailable |
| SEC-DOS-002 | Memory exhaustion | Custom | OOM crash |
| SEC-DOS-003 | CPU exhaustion (regex) | Custom | Performance |
| SEC-DOS-004 | Disk exhaustion | Custom | Storage DoS |
| SEC-DOS-005 | Slow query attack | Custom | Resource tie-up |
| SEC-DOS-006 | Lock contention | Custom | Deadlock |
| SEC-DOS-007 | Transaction bomb | Custom | Log exhaustion |
| SEC-DOS-008 | Large result set | Custom | Memory DoS |
| SEC-DOS-009 | Compression bomb | Custom | CPU DoS |
| SEC-DOS-010 | SSL renegotiation | Custom | CPU DoS |

### 6.7 Data Protection

| Test ID | Test Case | Tools | Risk |
|---------|-----------|-------|------|
| SEC-DATA-001 | Sensitive data in logs | Log analysis | Info disclosure |
| SEC-DATA-002 | Sensitive data in errors | Manual | Info disclosure |
| SEC-DATA-003 | Memory dump analysis | gdb | Data exposure |
| SEC-DATA-004 | Core dump analysis | gdb | Credential leak |
| SEC-DATA-005 | Temp file exposure | Manual | Data leak |
| SEC-DATA-006 | Backup file exposure | Manual | Data breach |
| SEC-DATA-007 | Data at rest encryption | Manual | Unencrypted data |
| SEC-DATA-008 | Key management | Manual | Key exposure |
| SEC-DATA-009 | Password storage | Manual | Weak hashing |
| SEC-DATA-010 | Connection string exposure | Manual | Credential leak |

---

## 7. Performance Benchmarks

### 7.1 Connection Performance

| Benchmark | Metric | Target | Tool |
|-----------|--------|--------|------|
| Connection latency | Time to ReadyForQuery | < 5ms (local) | Custom |
| SSL connection latency | Time with TLS handshake | < 20ms | Custom |
| Authentication latency | MD5/SCRAM overhead | < 2ms | Custom |
| Connection pool acquire | Pool checkout time | < 1ms | Custom |
| Max connections/second | New connections rate | > 1000/s | Custom |

### 7.2 Query Performance

| Benchmark | Metric | Target | Tool |
|-----------|--------|--------|------|
| Point SELECT | Single row by PK | < 0.5ms | pgbench |
| Range SELECT | 100 rows | < 2ms | pgbench |
| Complex JOIN | 4-table join | < 50ms | Custom |
| Aggregate | COUNT(*) 1M rows | < 100ms | Custom |
| INSERT single | One row | < 1ms | pgbench |
| INSERT batch | 1000 rows | < 50ms | Custom |
| UPDATE single | One row by PK | < 1ms | pgbench |
| DELETE single | One row by PK | < 1ms | pgbench |

### 7.3 Throughput Benchmarks

| Benchmark | Configuration | Target | Tool |
|-----------|---------------|--------|------|
| TPC-B (pgbench) | Scale 100, 32 clients | 10,000 TPS | pgbench |
| TPC-C (sysbench) | 10 warehouses | 5,000 TPS | sysbench |
| Bulk INSERT | COPY protocol | 100,000 rows/s | Custom |
| Bulk SELECT | Streaming | 500,000 rows/s | Custom |
| Mixed OLTP | 80/20 read/write | 15,000 TPS | Custom |

### 7.4 Latency Percentiles

| Operation | p50 | p90 | p99 | p99.9 |
|-----------|-----|-----|-----|-------|
| Point SELECT | 0.3ms | 0.5ms | 1ms | 5ms |
| Range SELECT | 1ms | 2ms | 5ms | 20ms |
| INSERT | 0.5ms | 1ms | 2ms | 10ms |
| UPDATE | 0.5ms | 1ms | 3ms | 15ms |
| Transaction (3 ops) | 2ms | 5ms | 10ms | 50ms |

### 7.5 Scalability Benchmarks

| Benchmark | Variable | Range | Measure |
|-----------|----------|-------|---------|
| Connection scaling | Connections | 10-1000 | TPS vs connections |
| CPU scaling | Cores | 1-32 | TPS vs cores |
| Data scaling | Table size | 1K-100M rows | Query time vs size |
| Concurrent scaling | Concurrent queries | 1-500 | Latency vs concurrency |
| Buffer pool scaling | Buffer size | 256MB-16GB | Cache hit rate |

### 7.6 Protocol Comparison

| Protocol | Point SELECT | INSERT | Bulk COPY |
|----------|--------------|--------|-----------|
| PostgreSQL | Baseline | Baseline | Baseline |
| MySQL | ±5% | ±5% | ±10% |
| TDS (post-gold) | ±10% | ±10% | ±15% |
| Firebird | ±10% | ±10% | ±20% |
| Native | +20% | +20% | +30% |

---

## 8. Test Automation

### 8.1 CI/CD Integration

```yaml
# .github/workflows/alpha3-tests.yml
name: Alpha 3 Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: cmake --build build --target all
      - name: Unit Tests
        run: ctest --test-dir build -L unit

  protocol-tests:
    runs-on: ubuntu-latest
    needs: unit-tests
    strategy:
      matrix:
        protocol: [postgresql, mysql, firebird, native]  # tds post-gold
    steps:
      - name: Protocol Compliance
        run: ./tests/protocol_compliance.sh ${{ matrix.protocol }}

  auth-tests:
    runs-on: ubuntu-latest
    needs: unit-tests
    steps:
      - name: Authentication Matrix
        run: ./tests/auth_matrix.sh

  load-tests:
    runs-on: [self-hosted, performance]
    needs: [protocol-tests, auth-tests]
    steps:
      - name: Load Tests
        run: ./tests/load_tests.sh
      - name: Upload Results
        uses: actions/upload-artifact@v3
        with:
          name: load-test-results
          path: results/
```

### 8.2 Test Reporting

```
Test Report Format:
├── summary.json           # Overall pass/fail counts
├── protocol/
│   ├── postgresql.xml     # JUnit format
│   ├── mysql.xml
│   ├── tds.xml (post-gold)
│   ├── firebird.xml
│   └── native.xml
├── auth/
│   └── auth_matrix.xml
├── load/
│   ├── throughput.csv
│   ├── latency.csv
│   └── graphs/
└── security/
    └── findings.json
```

---

## 9. Test Schedule

### 9.1 Phase 1: Protocol Implementation (Weeks 1-4)

| Week | Focus | Tests |
|------|-------|-------|
| 1 | PostgreSQL protocol | PG-CONN-*, PG-QUERY-* |
| 2 | MySQL protocol | MY-CONN-*, MY-QUERY-* |
| 3 | TDS + Firebird protocols (post-gold) | TDS-*, FB-* |
| 4 | Native protocol | SB-CONN-*, SB-QUERY-* |

### 9.2 Phase 2: Authentication (Weeks 5-6)

| Week | Focus | Tests |
|------|-------|-------|
| 5 | Basic auth methods | AUTH-001 to AUTH-010 |
| 6 | Advanced auth (LDAP, Kerberos, MFA) | AUTH-011 to AUTH-027 |

### 9.3 Phase 3: Load & Performance (Weeks 7-8)

| Week | Focus | Tests |
|------|-------|-------|
| 7 | Load testing | LOAD-*, STRESS-* |
| 8 | Performance benchmarks | All benchmarks |

### 9.4 Phase 4: Security (Weeks 9-10)

| Week | Focus | Tests |
|------|-------|-------|
| 9 | Penetration testing | SEC-* |
| 10 | Remediation & retest | Failed SEC-* |

---

## 10. Exit Criteria

### 10.1 Alpha 3 Release Criteria

| Category | Requirement |
|----------|-------------|
| Protocol Tests | 100% pass rate |
| Auth Tests | 100% pass rate |
| Load Tests | All targets met |
| Security Tests | No Critical/High findings |
| Performance | Within 10% of targets |
| Endurance | 72-hour stability |

### 10.2 Defect Thresholds

| Severity | Max Open | Max Total |
|----------|----------|-----------|
| Critical | 0 | 0 |
| High | 0 | 5 (all fixed) |
| Medium | 5 | 20 |
| Low | 20 | 50 |

---

## 11. Appendix: Test Data

### 11.1 Standard Test Datasets

| Dataset | Size | Description |
|---------|------|-------------|
| tiny | 1 MB | 10K rows, basic types |
| small | 100 MB | 1M rows, all types |
| medium | 1 GB | 10M rows, realistic |
| large | 10 GB | 100M rows, stress test |
| huge | 100 GB | 1B rows, scale test |

### 11.2 Test User Accounts

| Username | Password | Role | Purpose |
|----------|----------|------|---------|
| admin | admin123 | SUPERUSER | Admin tests |
| testuser | test123 | USER | Normal tests |
| readonly | read123 | READONLY | Read tests |
| limited | limit123 | LIMITED | Permission tests |
| attacker | attack123 | USER | Security tests |
