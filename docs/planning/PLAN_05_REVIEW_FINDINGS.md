# Plan 05 Review - Missing Details and Decision Points

**Plan:** Protocols, ODBC, Connection Pool
**Reviewer:** Claude Code
**Date:** 2025-12-26
**Status:** REVIEW IN PROGRESS

---

## Executive Summary

Plan 05 provides a good high-level overview and identifies concrete touchpoints. However, it lacks specific details in several critical areas that will be needed for implementation. This document identifies 14 categories of missing details and required decisions.

**Overall Assessment:**
- ✅ Scope and goals are clear
- ✅ File touchpoints identified
- ⚠️ Missing protocol-specific details
- ⚠️ Missing error handling specifications
- ⚠️ Missing configuration requirements
- ⚠️ Missing dependency clarifications

---

## Category 1: ODBC Driver Completeness

### Missing Details

**Current State:**
- Mentions SQLTables, SQLColumns, SQLGetTypeInfo
- Says "Catalog functions near SQLTables/SQLColumns/SQLGetTypeInfo (TODO)"

**Questions:**
1. Which ODBC catalog functions are required for Alpha?
   - SQLTables ✓
   - SQLColumns ✓
   - SQLGetTypeInfo ✓
   - SQLPrimaryKeys?
   - SQLForeignKeys?
   - SQLStatistics (indexes)?
   - SQLProcedures?
   - SQLProcedureColumns?
   - SQLSpecialColumns?
   - SQLTablePrivileges?
   - SQLColumnPrivileges?

2. ODBC Conformance Level:
   - Core level only?
   - Level 1?
   - Level 2?
   - Which specific functions MUST be implemented?

3. ODBC Version:
   - ODBC 3.x (recommended)
   - ODBC 2.x compatibility needed?

### Recommendation

**Add section:** "ODBC Catalog Functions - Required for Alpha"
- List exact functions with conformance level
- Specify which can return "not supported" vs must work
- Define data type mapping (ScratchBird types → ODBC SQL types)

---

## Category 2: Connection Pool Configuration

### Missing Details

**Current State:**
- Mentions reset logic
- No configuration details

**Questions:**
1. Pool sizing:
   - Minimum pool size?
   - Maximum pool size?
   - Growth strategy (on-demand, pre-allocate)?

2. Connection lifecycle:
   - Idle timeout before connection closed?
   - Maximum connection lifetime?
   - Validation query before returning from pool?

3. Health checks:
   - How to detect dead connections?
   - Retry logic for connection failures?
   - Circuit breaker pattern for cascading failures?

4. Metrics:
   - Pool utilization tracking?
   - Wait time monitoring?
   - Connection acquisition failures?

5. Per-protocol pools or unified?
   - Separate pool for each protocol adapter?
   - Or unified with protocol-agnostic connections?

### Recommendation

**Add section:** "Connection Pool Configuration and Lifecycle"
- Define all configuration parameters with defaults
- Specify health check mechanism
- Define metrics collection requirements

---

## Category 3: Native Protocol Attachment Multiplexing

### Missing Details

**Current State:**
- References Plan 16 for protocol layout
- High-level description only

**Questions:**
1. Protocol header format:
   - Exact field layout (attachment_id, txn_id positions)?
   - Field sizes (16-bit? 32-bit? 64-bit IDs)?
   - Endianness?

2. Attachment lifecycle messages:
   - CREATE ATTACHMENT message format?
   - DETACH ATTACHMENT message format?
   - Acknowledgment format?
   - Error responses?

3. Routing implementation:
   - How does server_session.cpp route by attachment_id?
   - Locking strategy for attachment context access?
   - What happens if attachment_id is invalid?

4. Transaction isolation:
   - How is transaction state per-attachment enforced?
   - What if client sends txn_id from different attachment?

5. Default attachment:
   - Created on connection or on first query?
   - Can it be detached or is it permanent?
   - What attachment_id for default (0, 1)?

### Recommendation

**Option A:** Expand in Plan 05 with exact protocol details
**Option B:** Defer all protocol details to Plan 16, just reference it
**Decision needed:** Should Plan 05 be self-contained or rely on Plan 16?

---

## Category 4: Authentication Integration

### Missing Details

**Current State:**
- Lists auth methods per protocol
- Mentions Plan 03 auth_manager

**Questions:**
1. Integration points:
   - How does MySQL mysql_native_password call auth_manager?
   - How does PostgreSQL SCRAM call auth_manager?
   - How does Firebird DPB call auth_manager?

2. User database:
   - Where are user credentials stored?
   - Same credentials for all protocols or per-protocol?
   - User@protocol differentiation (user@mysql vs user@postgres)?

3. Auth flows:
   - Does each protocol adapter validate independently?
   - Or do all adapters call common auth_manager API?
   - What's the exact API signature?

4. Password storage:
   - Plain text (for testing)?
   - Hashed (what algorithm)?
   - Per-protocol hash formats (MySQL uses different hash than PostgreSQL)?

5. Session establishment:
   - After successful auth, how is user_id propagated to ConnectionContext?
   - How are role/permission checks enforced?

### Recommendation

**Add section:** "Authentication Architecture Integration"
- Define common auth_manager API that all adapters use
- Specify user credential storage schema
- Define session establishment sequence diagram

---

## Category 5: Error Code Mapping

### Missing Details

**Current State:**
- Mentions error packets per protocol
- No mapping details

**Questions:**
1. Error code conversions:
   - ScratchBird Status enum → MySQL error codes?
   - ScratchBird Status → PostgreSQL SQLSTATE?
   - ScratchBird Status → Firebird ISC codes?
   - Do we need a central error mapping table?

2. Error message formatting:
   - Same message text for all protocols?
   - Protocol-specific error messages?
   - Localization support?

3. Severity levels:
   - How do ScratchBird error severities map to protocol severities?
   - PostgreSQL has ERROR, FATAL, PANIC - which to use when?

4. Error context:
   - Line numbers for syntax errors?
   - Column positions?
   - Object names in error messages?

### Recommendation

**Add section:** "Error Code Mapping Tables"
- Create mapping table: Status → (MySQL_code, PG_SQLSTATE, FB_ISC_code)
- Define error message formatting rules
- Specify error context requirements

---

## Category 6: SSL/TLS Support

### Missing Details

**Current State:**
- PostgreSQL SSLRequest mentioned
- Nothing about MySQL or Firebird SSL

**Questions:**
1. Protocol SSL support:
   - PostgreSQL: SSLRequest → upgrade to TLS (mentioned)
   - MySQL: SSL/TLS during capability negotiation (not mentioned)
   - Firebird: SSL/TLS support? (not mentioned)
   - Native protocol: TLS support? (not mentioned)

2. Certificate management:
   - Server certificate location?
   - Certificate validation for clients?
   - Certificate authorities?
   - Self-signed certificates for testing?

3. Cipher suites:
   - Which TLS versions supported (1.2, 1.3)?
   - Allowed cipher suites?
   - Protocol-specific cipher requirements?

4. Optional or mandatory:
   - Can clients connect without SSL?
   - Per-protocol SSL enforcement?
   - Per-user SSL requirements?

### Recommendation

**Add section:** "SSL/TLS Support"
- Define SSL support per protocol
- Specify certificate management approach
- Define configuration options for SSL enforcement

---

## Category 7: Prepared Statement Management

### Missing Details

**Current State:**
- Mentioned for ODBC and protocols
- Says "Use prepared statement cache on ConnectionContext"

**Questions:**
1. Cache structure:
   - Per-connection cache or global?
   - Cache size limits?
   - Eviction policy (LRU, TTL)?

2. Statement lifecycle:
   - When are statements prepared (first use, explicit)?
   - When are statements invalidated (schema change)?
   - Auto-close on connection close?

3. Statement naming:
   - How are prepared statements identified (name, ID)?
   - Protocol-specific naming (PostgreSQL portal vs statement)?
   - Collision handling if same name reused?

4. Parameter binding:
   - Data type validation for parameters?
   - NULL handling?
   - Array parameter support?

5. Plan caching:
   - Are execution plans cached separately from statements?
   - Plan invalidation on statistics change?

### Recommendation

**Add section:** "Prepared Statement Cache Design"
- Define cache structure and limits
- Specify lifecycle and invalidation rules
- Define parameter binding validation

---

## Category 8: COPY Protocol Completeness

### Missing Details

**Current State:**
- PostgreSQL COPY IN mentioned
- Implementation details for CopyData/CopyDone/CopyFail

**Questions:**
1. COPY direction:
   - COPY IN (client → server): mentioned ✓
   - COPY OUT (server → client): not mentioned
   - COPY BOTH (for replication): rejected in Alpha?

2. Data formats:
   - Text format support?
   - Binary format support?
   - CSV format with headers?
   - Custom delimiter support?

3. Error handling:
   - Partial import on error (rollback to savepoint)?
   - Error row reporting?
   - Continue on error vs abort?

4. Streaming:
   - Buffering strategy?
   - Memory limits for large COPY operations?
   - Progress reporting?

5. COPY FROM PROGRAM:
   - Supported or security risk?
   - Command execution permissions?

### Recommendation

**Add section:** "PostgreSQL COPY Protocol Support"
- Define COPY IN and COPY OUT support levels
- Specify supported formats
- Define error handling strategy

---

## Category 9: Transaction Model Integration

### Missing Details

**Current State:**
- "Always-in-transaction" mentioned
- Autocommit behavior described
- Immediate transaction start after COMMIT/ROLLBACK

**Questions:**
1. Transaction commands per protocol:
   - MySQL: START TRANSACTION, COMMIT, ROLLBACK
   - PostgreSQL: BEGIN, COMMIT, ROLLBACK, SAVEPOINT, RELEASE, ROLLBACK TO
   - Firebird: TPB-based transaction start
   - How do these map to ScratchBird transaction model?

2. Savepoint support:
   - Savepoints mentioned but not detailed
   - Nested transaction semantics?
   - SAVEPOINT naming across protocols?

3. Transaction isolation:
   - Default isolation level?
   - Per-protocol defaults (MySQL default, PostgreSQL default)?
   - How to set isolation level (SET TRANSACTION)?

4. Read-only transactions:
   - Supported?
   - Performance optimization for read-only?

5. Transaction timeout:
   - Idle transaction timeout?
   - Long-running transaction detection?

### Recommendation

**Add section:** "Transaction Model Protocol Mapping"
- Map each protocol's transaction commands to ScratchBird model
- Define savepoint support scope
- Specify isolation level handling

---

## Category 10: Protocol Version Compatibility

### Missing Details

**Current State:**
- Lists protocol messages
- No version requirements

**Questions:**
1. PostgreSQL protocol:
   - Protocol version 3.0 (current)?
   - Support for older versions?
   - Feature negotiation?

2. MySQL protocol:
   - Protocol version 10 (MySQL 5.x)?
   - Protocol version 41 (CLIENT_PROTOCOL_41)?
   - Which capabilities required vs optional?

3. Firebird protocol:
   - Wire protocol version (10, 11, 12, 13)?
   - Architecture value (arch_generic vs specific)?
   - Lazy response support?

4. Native protocol:
   - What is the ScratchBird native protocol version?
   - Version negotiation mechanism?
   - Backwards compatibility strategy?

### Recommendation

**Add section:** "Protocol Version Requirements"
- Specify exact protocol versions supported
- Define feature negotiation approach
- List required vs optional capabilities per protocol

---

## Category 11: Concrete Test Coverage

### Missing Details

**Current State:**
- Lists test types (integration, ODBC, pool, native)
- No specific test scenarios

**Questions:**
1. Test case count:
   - How many test cases per protocol?
   - Coverage percentage target?
   - Edge case requirements?

2. Specific scenarios:
   - Authentication success/failure paths?
   - Connection pool exhaustion?
   - Protocol version mismatch?
   - Malformed message handling?
   - Concurrent connection limit?

3. Performance tests:
   - Connection establishment time?
   - Query throughput?
   - Pool efficiency under load?

4. Compatibility tests:
   - Which client versions to test (psql 14, 15, 16)?
   - MySQL client versions (5.7, 8.0, 8.1)?
   - Firebird isql versions (3.0, 4.0, 5.0)?

### Recommendation

**Add section:** "Test Plan Matrix"
- Define test cases per protocol with scenarios
- Specify client version compatibility matrix
- Define performance benchmarks

---

## Category 12: Configuration Requirements

### Missing Details

**Current State:**
- "odbc_driver.cpp: pooling config stub"
- No other configuration details

**Questions:**
1. Server configuration:
   - Which protocols enabled by default?
   - Port assignments (PostgreSQL 5432, MySQL 3306, Firebird 3050, native?)?
   - Listen addresses (localhost only, all interfaces)?

2. Protocol-specific tuning:
   - Max packet size per protocol?
   - Timeout values?
   - Buffer sizes?

3. Feature flags:
   - SSL required/optional per protocol?
   - COPY command enabled/disabled?
   - Prepared statement cache enabled/disabled?

4. Connection limits:
   - Max connections overall?
   - Max connections per protocol?
   - Max connections per user?

5. Configuration file:
   - scratchbird.conf format?
   - Environment variables?
   - Command-line options?

### Recommendation

**Add section:** "Configuration Parameters"
- List all configurable parameters with defaults
- Define configuration file format
- Specify precedence (file vs env var vs command-line)

---

## Category 13: Dependency Clarifications

### Missing Details

**Current State:**
- References Plan 03 (auth), Plan 16 (attachment model)
- No mention of other plan dependencies

**Questions:**
1. Plan 02 (Schema/Database DDL):
   - MySQL COM_INIT_DB / USE database requires schema switching
   - Does Plan 05 depend on Plan 02B (Schema DDL opcodes)?
   - What if schema opcodes not ready?

2. Plan 04 (Domains):
   - ODBC SQLGetTypeInfo - should it report domain types?
   - Protocol type mapping for domains?
   - Dependency or can proceed without?

3. Plan 03 (Auth):
   - Is auth_manager API finalized?
   - What if Auth plan incomplete?

4. Plan 16 (Attachment Model):
   - Native protocol attachment multiplexing depends on Plan 16
   - Is Plan 16 spec complete?
   - Can proceed with single-attachment mode first?

5. Blocking vs non-blocking:
   - Can Plan 05 start if dependencies incomplete?
   - What features can be stubbed temporarily?
   - What must wait for other plans?

### Recommendation

**Add section:** "Plan Dependencies and Sequencing"
- List hard dependencies (blocks)
- List soft dependencies (can stub temporarily)
- Define implementation order options

---

## Category 14: Performance Requirements

### Missing Details

**Current State:**
- No performance targets mentioned

**Questions:**
1. Connection establishment:
   - Target time for new connection (< 100ms)?
   - Pool acquisition time target (< 1ms)?

2. Query overhead:
   - Protocol overhead acceptable range?
   - Prepared statement vs direct execution overhead?

3. Throughput:
   - Queries per second target?
   - Concurrent connection capacity?

4. Latency:
   - Round-trip time for simple query?
   - Large result set streaming performance?

5. Resource usage:
   - Memory per connection?
   - CPU overhead per protocol adapter?

### Recommendation

**Add section:** "Performance Requirements and Benchmarks"
- Define performance targets for key operations
- Specify resource usage limits
- Define benchmark methodology

---

## Summary of Recommendations

### High Priority (Must Address Before Implementation)

1. **ODBC Catalog Functions** - List exact functions required
2. **Connection Pool Configuration** - Define all config parameters
3. **Authentication Integration** - Specify auth_manager API contract
4. **Error Code Mapping** - Create complete mapping tables
5. **Plan Dependencies** - Clarify blocking vs non-blocking dependencies

### Medium Priority (Should Address for Completeness)

6. **Prepared Statement Management** - Define cache design
7. **Transaction Model Integration** - Map protocol commands to ScratchBird model
8. **Protocol Version Compatibility** - Specify exact versions
9. **Configuration Requirements** - List all parameters
10. **SSL/TLS Support** - Define scope per protocol

### Lower Priority (Can Defer or Define During Implementation)

11. **Native Protocol Details** - If Plan 16 is complete, reference it
12. **COPY Protocol Completeness** - COPY OUT can be deferred if not critical
13. **Concrete Test Coverage** - Can be refined during implementation
14. **Performance Requirements** - Can be benchmarked after initial implementation

---

## Decision Points for User

### Decision 1: ODBC Scope
**Question:** Which ODBC conformance level is required for Alpha?
- Option A: Core level only (minimal)
- Option B: Level 1 (moderate)
- Option C: Specific function list (pragmatic)

**Recommendation:** Option C - List specific functions needed for client compatibility

### Decision 2: Native Protocol Attachment Multiplexing
**Question:** Should Plan 05 include protocol details or reference Plan 16?
- Option A: Expand in Plan 05 (self-contained)
- Option B: Reference Plan 16 (avoid duplication)

**Recommendation:** Option B if Plan 16 is complete, Option A if Plan 16 incomplete

### Decision 3: Plan Dependencies
**Question:** Can Plan 05 proceed if Plan 02B (Schema DDL) incomplete?
- Option A: Block until Plan 02B complete (MySQL COM_INIT_DB needs it)
- Option B: Stub COM_INIT_DB temporarily (proceed with partial functionality)

**Recommendation:** User decision based on priority

### Decision 4: SSL/TLS Scope
**Question:** SSL support scope for Alpha?
- Option A: PostgreSQL only (as mentioned)
- Option B: All protocols (comprehensive)
- Option C: All protocols optional (configurable)

**Recommendation:** Option C - Make it configurable

### Decision 5: Performance Targets
**Question:** Are performance benchmarks required for Alpha?
- Option A: Yes - Define targets upfront
- Option B: No - Measure after implementation
- Option C: Basic targets only (< 100ms connection, etc.)

**Recommendation:** Option C - Basic targets to avoid regressions

---

## Next Steps

1. **User reviews this document**
2. **User makes decisions on Decision Points**
3. **Plan 05 updated with missing details**
4. **Cross-check with Plan 02B, Plan 03, Plan 16 for consistency**
5. **Ready for implementation**

---

**Review Status:** AWAITING USER FEEDBACK
**Last Updated:** 2025-12-26
