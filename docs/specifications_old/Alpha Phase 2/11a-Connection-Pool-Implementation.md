# Connection Pool Implementation

Complete implementation specification for the Remote Database UDR connection
pool. This is the canonical reference for pool behavior and concurrency.

See also: 11-Remote-Database-UDR-Specification.md

## 1. Goals
- Reduce handshake cost by reusing authenticated connections.
- Provide bounded concurrency and predictable timeouts.
- Detect stale or broken connections and replace them safely.
- Provide accurate pool stats for monitoring and conformance tests.

## 2. Pool Keying and Isolation
Each pool is keyed by:
- Server definition (host, port, dbname)
- UDR module (protocol)
- User mapping (credentials + auth method)
- TLS settings (mode, CA, client cert)
- Session options that alter server behavior (search_path, sql_mode, time_zone)

Connections for different user mappings or session options MUST NOT be reused
across mappings. This prevents permission leaks and inconsistent session state.

## 3. Configuration
Pool config is stored with the FOREIGN SERVER entry and can be overridden per
connection in options_json. All durations are milliseconds.

Required fields:
- max_connections (default 16)
- min_idle (default 0)
- max_idle (default 16)
- connect_timeout_ms (default 5000)
- acquire_timeout_ms (default 10000)
- idle_timeout_ms (default 60000)
- max_lifetime_ms (default 0, unlimited)
- health_check_interval_ms (default 30000)
- health_check_query (default "SELECT 1")
- reset_on_release (default true)
- max_in_flight_per_conn (default 1)
- backoff_initial_ms (default 200)
- backoff_max_ms (default 5000)

Optional tuning:
- prefer_prepared (default true)
- prepare_cache_capacity (default 256 per connection)
- session_init_sql (optional SQL executed on connect)
- tcp_keepalive (bool; protocol-specific)

## 4. Connection Lifecycle

### 4.1 State Machine
Each connection instance is tracked by state:
- NEW: created but not connected
- CONNECTING: handshake in progress
- READY: idle and reusable
- IN_USE: assigned to a client request
- FAILED: broken; must be closed
- CLOSING: closing/cleanup in progress

State transitions:
- NEW -> CONNECTING -> READY
- READY -> IN_USE -> READY
- IN_USE -> FAILED -> CLOSING
- READY -> CLOSING (idle timeout, max_lifetime)

### 4.2 Acquire Algorithm
1. Check for READY connection in the pool.
2. If none and total < max_connections, create a new connection.
3. If at max, wait until acquire_timeout_ms.
4. On timeout, return a pool-timeout error.

Notes:
- Connections in FAILED are not reusable and must be closed.
- On acquire, the connection MUST be validated if idle_timeout_ms expired or
  if the connection has been unused for > health_check_interval_ms.

### 4.3 Release Algorithm
1. If transaction is open, issue ROLLBACK (or protocol equivalent).
2. If reset_on_release, run connection reset sequence:
   - PostgreSQL: DISCARD ALL
   - MySQL: COM_RESET_CONNECTION
   - MSSQL: sp_reset_connection
   - Firebird: rollback + reset session variables (if supported)
3. If reset fails, mark FAILED and close.
4. Return to READY state.

### 4.4 Health Check
A per-pool health thread runs every health_check_interval_ms:
- Pings idle connections with health_check_query.
- Drops connections that fail or exceed max_lifetime_ms.
- Recreates connections until min_idle is satisfied.

Failures use exponential backoff:
- backoff_initial_ms doubled per failure (capped at backoff_max_ms).

## 5. Error Classification
- Network errors (disconnect, timeout, TLS failure) => FAILED and drop.
- Protocol errors (invalid message, auth failure) => FAILED and drop.
- Statement errors (syntax, constraint, remote SQLSTATE) => keep connection.
- Serialization or deadlock errors => keep connection and return error.

## 6. Thread Safety
- Pools are shared across sessions; operations are guarded by a mutex.
- Acquire and release use condition variables to wake waiters.
- Per-connection send/recv is single-threaded; no concurrent use.

## 7. Metrics (Required)
Expose per-pool metrics:
- total_created
- total_closed
- total_failed
- total_acquired
- total_timeouts
- total_wait_ms
- active_count
- idle_count
- wait_queue_depth
- last_error_code, last_error_message

These map to sys.performance and driver conformance checks.

## 8. Cancellation
Cancellation is protocol-specific but MUST be wired to the pool:
- PostgreSQL: CancelRequest using backend PID/secret key.
- MySQL: COM_PROCESS_KILL or KILL QUERY thread_id.
- MSSQL: Attention signal; requires a separate socket in TDS.
- Firebird: op_cancel or protocol equivalent.

Cancellation attempts do NOT automatically drop the connection unless the
protocol reports an error state.

## 9. Conformance Tests (Minimum)
- Acquire/release under load (max_connections enforced).
- Idle timeout closes connections after idle_timeout_ms.
- max_lifetime_ms forces recycle.
- Health check replaces stale connections.
- Transaction rollback on release.
- Cancellation flow does not leak connections.

