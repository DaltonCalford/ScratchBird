# SBCLUSTER-10: Observability and Audit

## 1. Introduction

### 1.1 Purpose
This document specifies the observability, monitoring, and audit architecture for ScratchBird clusters. Observability enables **real-time visibility** into cluster health, performance, and security events through metrics, distributed tracing, and cryptographically verifiable audit logs. A critical principle: **audit chains are immutable and cryptographically linked** - tampering with any audit event invalidates the entire chain.

### 1.2 Scope
- Metrics collection (OpenTelemetry-compatible, Prometheus exposition)
- Distributed tracing across cluster operations
- Cryptographic audit chain (hash-linked immutable logs)
- Cluster churn metrics (membership changes, leader elections)
- Peer observation and health checks
- Performance monitoring (latency, throughput, resource utilization)
- Security event auditing (authentication, authorization, data access)
- Alert generation and anomaly detection
- Log aggregation and retention policies
- Observability API (query metrics, traces, audit logs)

### 1.3 Related Documents
- **SBCLUSTER-01**: Cluster Configuration Epoch (observability policy in CCE)
- **SBCLUSTER-02**: Membership and Identity (peer observation, node roles)
- **Security Spec 08**: Audit Compliance (canonical audit event format)
- **Security Spec 08.01**: Audit Event Canonicalization
- **Security Spec 08.02**: Audit Chain Verification Checkpoints

### 1.4 Terminology
- **Observability Signal**: Telemetry data (metrics, traces, logs, audit events)
- **Metric**: Time-series numeric measurement (e.g., query_latency_ms)
- **Trace**: Distributed request path across nodes and shards
- **Span**: Single operation within a trace
- **Audit Event**: Immutable security/compliance event with cryptographic proof
- **Audit Chain**: Hash-linked sequence of audit events (Merkle chain)
- **Churn Metric**: Cluster topology change measurement (joins, leaves, elections)
- **Peer Observer**: Node monitoring the health of other cluster members
- **RED Metrics**: Rate, Error, Duration (service health indicators)
- **Cardinality**: Number of unique metric label combinations

---

## 2. Architecture Overview

### 2.1 Design Principles

1. **OpenTelemetry-Native**: Standard OTLP protocol for metrics, traces, and logs
2. **Prometheus-Compatible**: Metrics exposed in Prometheus format for scraping
3. **Cryptographic Audit Chain**: All audit events hash-linked (Merkle chain)
4. **Local-First Collection**: Each node collects its own metrics, minimal overhead
5. **Distributed Tracing**: Trace IDs propagate across cluster operations
6. **Immutable Audit Logs**: Audit events are append-only and tamper-evident
7. **Peer Observation**: Nodes monitor each other via gossip and health checks
8. **Low-Cardinality Metrics**: Avoid unbounded label values (UUIDs in labels)

### 2.2 Observability Topology

```
┌─────────────────────────────────────────────────────────────────┐
│              OBSERVABILITY CONTROL PLANE                        │
│  - Raft leader aggregates cluster-wide metrics                  │
│  - Generates alerts on anomalies                                │
│  - Manages audit chain verification checkpoints                 │
└─────┬───────────────┬───────────────┬───────────────────────────┘
      │               │               │
      ▼               ▼               ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│  NODE A     │ │  NODE B     │ │  NODE C     │
│             │ │             │ │             │
│  Local      │ │  Local      │ │  Local      │
│  Metrics    │ │  Metrics    │ │  Metrics    │
│  Agent      │ │  Agent      │ │  Agent      │
│             │ │             │ │             │
│  Audit      │ │  Audit      │ │  Audit      │
│  Chain      │ │  Chain      │ │  Chain      │
│  Writer     │ │  Writer     │ │  Writer     │
└──────┬──────┘ └──────┬──────┘ └──────┬──────┘
       │               │               │
       │  Push/Pull    │               │
       ▼               ▼               ▼
┌─────────────────────────────────────────────────────────────────┐
│           OBSERVABILITY STORAGE & ANALYSIS                      │
│  - Prometheus (metrics time-series storage)                     │
│  - Tempo/Jaeger (distributed tracing backend)                   │
│  - Loki (log aggregation)                                       │
│  - PostgreSQL (audit chain permanent storage)                   │
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 Signal Types

| Signal Type | Format | Transport | Storage Backend | Retention |
|-------------|--------|-----------|-----------------|-----------|
| **Metrics** | Prometheus/OTLP | Pull (scrape) or Push | Prometheus TSDB | 30 days (default) |
| **Traces** | OTLP | Push | Tempo/Jaeger | 7 days (default) |
| **Logs** | JSON/OTLP | Push | Loki | 14 days (default) |
| **Audit Events** | Canonical JSON + Hash | Raft replication | PostgreSQL | Permanent (compliance) |

---

## 3. Data Structures

### 3.1 Metric

ScratchBird metrics follow OpenTelemetry and Prometheus conventions:

```cpp
struct Metric {
    string name;                         // Metric name (e.g., "scratchbird_query_duration_seconds")
    MetricType type;                     // COUNTER | GAUGE | HISTOGRAM | SUMMARY
    double value;                        // Current metric value
    map<string, string> labels;          // Label key-value pairs (low cardinality!)
    timestamp_t timestamp;               // Observation time (microsecond precision)
    string help_text;                    // Human-readable description
};

enum MetricType {
    COUNTER,       // Monotonically increasing (e.g., total queries executed)
    GAUGE,         // Can go up or down (e.g., active connections)
    HISTOGRAM,     // Distribution of values (e.g., query latency buckets)
    SUMMARY        // Quantiles of values (e.g., p50, p95, p99 latency)
};
```

**Example Metric**:
```cpp
Metric query_duration = {
    .name = "scratchbird_query_duration_seconds",
    .type = MetricType::HISTOGRAM,
    .labels = {
        {"node_uuid", "node_a"},
        {"shard_id", "shard_001"},
        {"query_type", "SELECT"}
    },
    .timestamp = now_micros()
};
```

### 3.2 Trace and Span

Distributed tracing follows OpenTelemetry semantic conventions:

```cpp
struct Trace {
    UUID trace_id;                       // Unique trace identifier (16 bytes)
    UUID parent_span_id;                 // Parent span (or null for root)
    vector<Span> spans;                  // All spans in this trace
    TraceState state;                    // ACTIVE | COMPLETED | FAILED
    timestamp_t started_at;
    timestamp_t completed_at;
};

struct Span {
    UUID span_id;                        // Unique span identifier (8 bytes)
    UUID trace_id;                       // Parent trace
    UUID parent_span_id;                 // Parent span (or null)

    string operation_name;               // Operation being traced (e.g., "execute_query")
    SpanKind kind;                       // CLIENT | SERVER | PRODUCER | CONSUMER | INTERNAL

    timestamp_t start_time;              // Span start (microseconds)
    timestamp_t end_time;                // Span end (microseconds)

    map<string, string> attributes;      // Span attributes (e.g., sql.query, db.shard_id)
    vector<SpanEvent> events;            // Events within span (e.g., "cache_miss")
    SpanStatus status;                   // OK | ERROR | UNSET
    string status_message;               // Error message (if status == ERROR)
};

enum SpanKind {
    CLIENT,        // Span represents outbound call
    SERVER,        // Span represents inbound request
    PRODUCER,      // Span represents message sent
    CONSUMER,      // Span represents message received
    INTERNAL       // Span represents internal operation
};

enum SpanStatus {
    UNSET,         // Status not set
    OK,            // Success
    ERROR          // Error occurred
};
```

**Example Trace**:
```cpp
// Root span: Client query enters cluster
Span root_span = {
    .span_id = generate_span_id(),
    .trace_id = trace_id,
    .parent_span_id = UUID::null(),
    .operation_name = "cluster_query",
    .kind = SpanKind::SERVER,
    .attributes = {
        {"db.statement", "SELECT * FROM users WHERE age > 25"},
        {"db.user", "alice"},
        {"net.peer.ip", "192.168.1.100"}
    }
};

// Child span: Query routed to shard
Span shard_span = {
    .span_id = generate_span_id(),
    .trace_id = trace_id,
    .parent_span_id = root_span.span_id,
    .operation_name = "shard_query",
    .kind = SpanKind::CLIENT,
    .attributes = {
        {"db.shard_id", "shard_005"},
        {"db.node_uuid", "node_c"}
    }
};
```

### 3.3 AuditEvent

Audit events are cryptographically linked in an immutable chain:

```cpp
struct AuditEvent {
    UUID event_uuid;                     // Event identifier (v7, time-ordered)
    uint64_t event_sequence;             // Monotonic sequence number per node

    AuditEventType event_type;           // LOGIN | QUERY | DDL | ROLE_GRANT | etc.
    AuditSeverity severity;              // INFO | WARNING | CRITICAL

    // Event identity
    UUID node_uuid;                      // Node where event occurred
    UUID user_uuid;                      // User who triggered event
    UUID session_uuid;                   // Session context

    // Event payload (canonical JSON format)
    string canonical_json;               // Canonicalized event data

    // Cryptographic chain
    bytes event_hash;                    // SHA-256(canonical_json || prev_hash)
    bytes prev_event_hash;               // Hash of previous audit event

    // Signature
    bytes signature;                     // Ed25519 signature (node private key)

    // Timestamp
    timestamp_t event_timestamp;         // When event occurred (microsecond)
    timestamp_t ingestion_timestamp;     // When event was written to audit log
};

enum AuditEventType {
    // Authentication
    LOGIN_SUCCESS,
    LOGIN_FAILURE,
    LOGOUT,

    // Authorization
    PERMISSION_GRANTED,
    PERMISSION_DENIED,
    ROLE_GRANTED,
    ROLE_REVOKED,

    // Data Access
    QUERY_EXECUTED,
    DDL_EXECUTED,
    DML_EXECUTED,

    // Security Events
    ENCRYPTION_KEY_ROTATED,
    CERTIFICATE_ISSUED,
    CERTIFICATE_REVOKED,

    // Cluster Events
    NODE_JOINED,
    NODE_LEFT,
    LEADER_ELECTED,
    SHARD_MIGRATED,

    // Compliance
    DATA_EXPORT,
    DATA_DELETION,
    POLICY_VIOLATION
};

enum AuditSeverity {
    INFO,          // Normal operation
    WARNING,       // Suspicious activity
    CRITICAL       // Security violation or compliance breach
};
```

### 3.4 ChurnMetric

Cluster topology change tracking:

```cpp
struct ChurnMetric {
    UUID cluster_uuid;
    timestamp_t observation_time;

    // Membership churn
    uint32_t node_join_count;            // Nodes joined in window
    uint32_t node_leave_count;           // Nodes left in window
    uint32_t node_failure_count;         // Nodes failed in window

    // Leadership churn
    uint32_t leader_election_count;      // Leader elections in window
    duration_t avg_leader_tenure;        // Average leadership duration

    // Shard churn
    uint32_t shard_migration_count;      // Shards moved between nodes
    uint32_t shard_rebalance_count;      // Shard rebalances triggered

    // Health indicators
    double cluster_stability_score;      // 0.0 (unstable) to 1.0 (stable)
    uint32_t network_partition_count;    // Network splits detected
};
```

### 3.5 ObservabilityPolicy

```cpp
struct ObservabilityPolicy {
    // Metrics
    bool enable_metrics;                 // Enable metrics collection (default: true)
    duration_t metrics_scrape_interval;  // Prometheus scrape interval (default: 15s)
    uint32_t metrics_retention_days;     // Metrics retention (default: 30 days)

    // Tracing
    bool enable_tracing;                 // Enable distributed tracing (default: true)
    double trace_sample_rate;            // Sample rate 0.0-1.0 (default: 0.1 = 10%)
    uint32_t trace_retention_days;       // Trace retention (default: 7 days)

    // Logs
    bool enable_logs;                    // Enable log collection (default: true)
    LogLevel min_log_level;              // Minimum log level (default: INFO)
    uint32_t log_retention_days;         // Log retention (default: 14 days)

    // Audit
    bool enable_audit;                   // Enable audit logging (default: true)
    bool audit_all_queries;              // Audit every query? (default: false)
    bool audit_ddl_only;                 // Audit DDL only (default: true)
    duration_t audit_checkpoint_interval;// Checkpoint interval (default: 1 hour)

    // Alerting
    bool enable_alerting;                // Enable alerting (default: true)
    vector<AlertRule> alert_rules;       // Alert rule definitions
};
```

---

## 4. Metrics Collection

### 4.1 Core Metrics

ScratchBird exposes metrics in four categories:

#### 4.1.1 Query Metrics

```cpp
// Counter: Total queries executed
scratchbird_queries_total{node_uuid, shard_id, query_type, status}

// Histogram: Query latency distribution
scratchbird_query_duration_seconds{node_uuid, shard_id, query_type}
  Buckets: [0.001, 0.01, 0.1, 0.5, 1.0, 5.0, 10.0, 30.0]

// Gauge: Active queries
scratchbird_active_queries{node_uuid, shard_id}

// Counter: Queries by error code
scratchbird_query_errors_total{node_uuid, shard_id, error_code}
```

#### 4.1.2 Transaction Metrics

```cpp
// Counter: Transactions started
scratchbird_transactions_started_total{node_uuid, shard_id, isolation_level}

// Counter: Transactions committed
scratchbird_transactions_committed_total{node_uuid, shard_id}

// Counter: Transactions rolled back
scratchbird_transactions_rolled_back_total{node_uuid, shard_id, reason}

// Histogram: Transaction duration
scratchbird_transaction_duration_seconds{node_uuid, shard_id}

// Gauge: Active transactions
scratchbird_active_transactions{node_uuid, shard_id}

// Gauge: MVCC snapshot age (seconds)
scratchbird_oldest_active_snapshot_age_seconds{node_uuid, shard_id}
```

#### 4.1.3 Storage Metrics

```cpp
// Gauge: Heap pages used
scratchbird_heap_pages_used{node_uuid, shard_id, tablespace_uuid}

// Gauge: TOAST pages used
scratchbird_toast_pages_used{node_uuid, shard_id, tablespace_uuid}

// Counter: Page reads
scratchbird_page_reads_total{node_uuid, shard_id, cache_hit}

// Counter: Page writes
scratchbird_page_writes_total{node_uuid, shard_id}

// Gauge: Free space map size
scratchbird_fsm_size_bytes{node_uuid, shard_id}

// Gauge: Garbage collector queue depth
scratchbird_gc_queue_depth{node_uuid, shard_id}
```

#### 4.1.4 Raft Metrics

```cpp
// Gauge: Current leader (1 if this node is leader, 0 otherwise)
scratchbird_raft_leader{node_uuid}

// Counter: Leader elections
scratchbird_raft_elections_total{node_uuid}

// Histogram: Raft log replication latency
scratchbird_raft_replication_latency_seconds{node_uuid}

// Gauge: Raft log entries
scratchbird_raft_log_entries{node_uuid}

// Counter: Raft log compactions
scratchbird_raft_compactions_total{node_uuid}

// Gauge: Cluster member count
scratchbird_cluster_members{node_uuid}
```

### 4.2 RED Metrics (Rate, Error, Duration)

Auto-generated from span data for each operation:

```cpp
// Rate: Operations per second
scratchbird_operation_rate{operation, node_uuid}

// Error: Error rate
scratchbird_operation_error_rate{operation, node_uuid}

// Duration: p50, p95, p99 latencies
scratchbird_operation_duration_p50{operation, node_uuid}
scratchbird_operation_duration_p95{operation, node_uuid}
scratchbird_operation_duration_p99{operation, node_uuid}
```

### 4.3 Metrics Collection Implementation

```cpp
class MetricsCollector {
public:
    // Register a metric
    void register_metric(const Metric& metric);

    // Increment counter
    void increment_counter(const string& name, const map<string, string>& labels, double value = 1.0);

    // Set gauge value
    void set_gauge(const string& name, const map<string, string>& labels, double value);

    // Observe histogram value
    void observe_histogram(const string& name, const map<string, string>& labels, double value);

    // Export metrics in Prometheus format
    string export_prometheus_format();

    // Export metrics in OTLP format
    bytes export_otlp_format();

private:
    map<string, Metric> metrics_;
    mutex metrics_mutex_;
};
```

**Prometheus Exposition Endpoint**:
```
GET /metrics HTTP/1.1

# HELP scratchbird_queries_total Total queries executed
# TYPE scratchbird_queries_total counter
scratchbird_queries_total{node_uuid="node_a",shard_id="shard_001",query_type="SELECT",status="success"} 123456

# HELP scratchbird_query_duration_seconds Query execution time
# TYPE scratchbird_query_duration_seconds histogram
scratchbird_query_duration_seconds_bucket{node_uuid="node_a",shard_id="shard_001",query_type="SELECT",le="0.01"} 45000
scratchbird_query_duration_seconds_bucket{node_uuid="node_a",shard_id="shard_001",query_type="SELECT",le="0.1"} 90000
...
```

---

## 5. Distributed Tracing

### 5.1 Trace Propagation

Trace context propagates across cluster operations using W3C Trace Context standard:

```cpp
struct TraceContext {
    UUID trace_id;           // 16-byte trace identifier
    UUID parent_span_id;     // 8-byte parent span
    uint8_t trace_flags;     // Sampled bit (0x01)
    string trace_state;      // Vendor-specific state
};

// Extract trace context from HTTP headers
TraceContext extract_trace_context(const HTTPRequest& req) {
    // traceparent: 00-{trace_id}-{parent_span_id}-{flags}
    string traceparent = req.get_header("traceparent");
    return parse_traceparent(traceparent);
}

// Inject trace context into RPC call
void inject_trace_context(RPCRequest& rpc, const TraceContext& ctx) {
    rpc.set_metadata("traceparent", format_traceparent(ctx));
}
```

### 5.2 Automatic Instrumentation

ScratchBird automatically creates spans for:

- **Query Execution**: Root span for client query
- **Shard Routing**: Child spans for distributed query routing
- **Raft Operations**: Spans for consensus operations
- **Storage Operations**: Spans for heap/TOAST I/O
- **Network Calls**: Spans for inter-node RPC

```cpp
// Example: Automatic query tracing
Span execute_query_with_tracing(const Query& query, const TraceContext& parent_ctx) {
    // Create span
    Span span = create_span(
        parent_ctx.trace_id,
        parent_ctx.parent_span_id,
        "execute_query",
        SpanKind::SERVER
    );

    // Add attributes
    span.set_attribute("db.statement", query.sql);
    span.set_attribute("db.user", query.user_uuid.to_string());
    span.set_attribute("db.shard_id", query.target_shard_id);

    try {
        // Execute query
        ResultSet result = execute_query_internal(query);

        // Record success
        span.set_status(SpanStatus::OK);
        span.set_attribute("db.rows_returned", result.row_count());

        return span;

    } catch (const Exception& e) {
        // Record error
        span.set_status(SpanStatus::ERROR, e.message());
        span.set_attribute("error.type", e.error_code());
        throw;

    } finally {
        // End span
        span.end();
        export_span(span);
    }
}
```

### 5.3 Distributed Query Tracing Example

```
Trace: 8a3f9b2c4d5e6f7g
├─ Span 1: cluster_query (Server, 125ms)
│  └─ Span 2: parse_query (Internal, 5ms)
│  └─ Span 3: plan_query (Internal, 15ms)
│  └─ Span 4: route_to_shards (Internal, 105ms)
│     ├─ Span 5: shard_001_query (Client, 45ms)
│     │  └─ Span 6: heap_scan (Internal, 40ms)
│     └─ Span 7: shard_002_query (Client, 52ms)
│        └─ Span 8: index_scan (Internal, 48ms)
└─ Span 9: merge_results (Internal, 8ms)
```

---

## 6. Cryptographic Audit Chain

### 6.1 Audit Chain Design

Audit events form an immutable hash chain (Merkle chain):

```
Event 1              Event 2              Event 3
┌────────────┐      ┌────────────┐      ┌────────────┐
│ UUID       │      │ UUID       │      │ UUID       │
│ Sequence: 1│      │ Sequence: 2│      │ Sequence: 3│
│ Payload    │      │ Payload    │      │ Payload    │
│ Hash: H1   │──┐   │ Hash: H2   │──┐   │ Hash: H3   │
│ Prev: null │  │   │ Prev: H1   │  │   │ Prev: H2   │
│ Sig: S1    │  │   │ Sig: S2    │  │   │ Sig: S3    │
└────────────┘  │   └────────────┘  │   └────────────┘
                └──────────────────┘
```

**Hash Calculation**:
```cpp
bytes calculate_audit_hash(const AuditEvent& event) {
    // H(event) = SHA256(canonical_json || prev_hash)
    bytes input = concat(event.canonical_json, event.prev_event_hash);
    return SHA256(input);
}
```

**Signature**:
```cpp
bytes sign_audit_event(const AuditEvent& event, const PrivateKey& node_key) {
    // Sign the event hash with node's Ed25519 private key
    return ed25519_sign(node_key, event.event_hash);
}
```

### 6.2 Audit Event Canonicalization

See **Security Spec 08.01** for full details. Brief summary:

```cpp
string canonicalize_audit_event(const AuditEvent& event) {
    // 1. Build JSON object with deterministic key ordering
    json j;
    j["event_uuid"] = event.event_uuid.to_string();
    j["event_sequence"] = event.event_sequence;
    j["event_type"] = event_type_to_string(event.event_type);
    j["node_uuid"] = event.node_uuid.to_string();
    j["user_uuid"] = event.user_uuid.to_string();
    j["timestamp"] = event.event_timestamp.to_iso8601();
    j["payload"] = event.payload;  // Payload-specific canonicalization

    // 2. Serialize with no whitespace, deterministic key order
    return j.dump(-1, ' ', false, json::error_handler_t::strict);
}
```

### 6.3 Audit Chain Verification

```cpp
bool verify_audit_chain(const vector<AuditEvent>& events) {
    bytes prev_hash = bytes{};  // First event has null prev_hash

    for (const AuditEvent& event : events) {
        // 1. Verify prev_hash matches
        if (event.prev_event_hash != prev_hash) {
            log_error("Audit chain broken at event {}: prev_hash mismatch", event.event_uuid);
            return false;
        }

        // 2. Recompute hash
        bytes computed_hash = calculate_audit_hash(event);
        if (computed_hash != event.event_hash) {
            log_error("Audit event {} hash mismatch", event.event_uuid);
            return false;
        }

        // 3. Verify signature
        PublicKey node_pubkey = get_node_public_key(event.node_uuid);
        if (!ed25519_verify(node_pubkey, event.event_hash, event.signature)) {
            log_error("Audit event {} signature invalid", event.event_uuid);
            return false;
        }

        // 4. Move to next event
        prev_hash = event.event_hash;
    }

    return true;
}
```

### 6.4 Audit Chain Checkpoints

Periodic checkpoints create Merkle roots for efficient verification:

```cpp
struct AuditCheckpoint {
    UUID checkpoint_uuid;
    uint64_t start_sequence;             // First event in checkpoint
    uint64_t end_sequence;               // Last event in checkpoint
    bytes merkle_root;                   // Merkle root of events [start, end]
    timestamp_t checkpoint_time;
    bytes checkpoint_signature;          // Signed by Raft leader
};

// Checkpoint creation (every hour)
AuditCheckpoint create_checkpoint(uint64_t start_seq, uint64_t end_seq) {
    // 1. Retrieve audit events
    vector<AuditEvent> events = get_audit_events(start_seq, end_seq);

    // 2. Build Merkle tree
    bytes merkle_root = build_merkle_tree(events);

    // 3. Sign Merkle root with leader's key
    bytes signature = sign_merkle_root(merkle_root);

    // 4. Store checkpoint
    AuditCheckpoint checkpoint = {
        .checkpoint_uuid = UUID::v7(),
        .start_sequence = start_seq,
        .end_sequence = end_seq,
        .merkle_root = merkle_root,
        .checkpoint_time = now(),
        .checkpoint_signature = signature
    };

    persist_checkpoint(checkpoint);
    return checkpoint;
}
```

---

## 7. Cluster Churn Monitoring

### 7.1 Membership Churn Metrics

Track cluster topology changes:

```cpp
void observe_node_join(const UUID& node_uuid) {
    increment_counter("scratchbird_node_join_total", {{"node_uuid", node_uuid.to_string()}});

    ChurnMetric& churn = get_current_churn_window();
    churn.node_join_count++;
    churn.cluster_stability_score = calculate_stability_score();
}

void observe_node_leave(const UUID& node_uuid, NodeLeaveReason reason) {
    increment_counter("scratchbird_node_leave_total", {
        {"node_uuid", node_uuid.to_string()},
        {"reason", leave_reason_to_string(reason)}
    });

    ChurnMetric& churn = get_current_churn_window();
    if (reason == NodeLeaveReason::FAILURE) {
        churn.node_failure_count++;
    } else {
        churn.node_leave_count++;
    }
    churn.cluster_stability_score = calculate_stability_score();
}
```

### 7.2 Leader Election Metrics

```cpp
void observe_leader_election(const UUID& new_leader_uuid, duration_t election_duration) {
    increment_counter("scratchbird_raft_elections_total", {{"node_uuid", new_leader_uuid.to_string()}});

    observe_histogram("scratchbird_raft_election_duration_seconds", {}, election_duration.seconds());

    ChurnMetric& churn = get_current_churn_window();
    churn.leader_election_count++;

    // Update average leader tenure
    if (previous_leader_uuid != UUID::null()) {
        duration_t tenure = now() - leader_start_time;
        churn.avg_leader_tenure = (churn.avg_leader_tenure + tenure) / 2.0;
    }
}
```

### 7.3 Stability Score Calculation

```cpp
double calculate_stability_score() {
    const duration_t WINDOW = duration_t::hours(1);

    ChurnMetric churn = get_churn_window(now() - WINDOW, now());

    // Score components (each 0.0 to 1.0)
    double membership_stability = 1.0 - min(1.0, churn.node_join_count + churn.node_leave_count / 10.0);
    double leadership_stability = 1.0 - min(1.0, churn.leader_election_count / 5.0);
    double shard_stability = 1.0 - min(1.0, churn.shard_migration_count / 20.0);

    // Weighted average
    return (membership_stability * 0.4) +
           (leadership_stability * 0.4) +
           (shard_stability * 0.2);
}
```

### 7.4 Churn Alerts

```cpp
void check_churn_alerts() {
    ChurnMetric churn = get_current_churn_window();

    // Alert: Excessive leader elections
    if (churn.leader_election_count > 5) {
        fire_alert(AlertSeverity::WARNING,
            "EXCESSIVE_LEADER_ELECTIONS",
            fmt::format("{} leader elections in 1 hour", churn.leader_election_count));
    }

    // Alert: Low cluster stability
    if (churn.cluster_stability_score < 0.5) {
        fire_alert(AlertSeverity::CRITICAL,
            "CLUSTER_INSTABILITY",
            fmt::format("Stability score: {:.2f} (threshold: 0.5)", churn.cluster_stability_score));
    }

    // Alert: Network partition detected
    if (churn.network_partition_count > 0) {
        fire_alert(AlertSeverity::CRITICAL,
            "NETWORK_PARTITION_DETECTED",
            "Cluster may be split");
    }
}
```

---

## 8. Peer Observation

### 8.1 Health Check Protocol

Nodes monitor each other via periodic health checks:

```cpp
struct PeerHealthCheck {
    UUID observer_uuid;                  // Node performing health check
    UUID target_uuid;                    // Node being checked
    timestamp_t check_time;

    HealthStatus status;                 // HEALTHY | DEGRADED | UNREACHABLE
    duration_t rtt;                      // Round-trip time
    string failure_reason;               // Reason if unhealthy
};

enum HealthStatus {
    HEALTHY,       // Peer responding normally
    DEGRADED,      // Peer slow or intermittent
    UNREACHABLE    // Peer not responding
};
```

**Health Check Loop**:
```cpp
void peer_observer_main_loop() {
    while (true) {
        vector<UUID> peer_uuids = get_cluster_peer_uuids();

        for (const UUID& peer_uuid : peer_uuids) {
            // Send health check RPC
            timestamp_t start = now();
            HealthCheckResponse resp = send_health_check_rpc(peer_uuid, timeout: 5s);
            duration_t rtt = now() - start;

            // Record result
            HealthStatus status = evaluate_health(resp, rtt);
            record_peer_health(peer_uuid, status, rtt);

            // Update metrics
            set_gauge("scratchbird_peer_health", {{"target", peer_uuid.to_string()}}, status == HEALTHY ? 1.0 : 0.0);
            observe_histogram("scratchbird_peer_rtt_seconds", {{"target", peer_uuid.to_string()}}, rtt.seconds());
        }

        sleep(15s);  // Health check interval
    }
}
```

### 8.2 Failure Detection

```cpp
void detect_peer_failure(const UUID& peer_uuid) {
    // Get recent health check history
    vector<PeerHealthCheck> recent_checks = get_recent_health_checks(peer_uuid, count: 5);

    // Failure condition: 3+ consecutive UNREACHABLE
    int unreachable_count = 0;
    for (const PeerHealthCheck& check : recent_checks) {
        if (check.status == HealthStatus::UNREACHABLE) {
            unreachable_count++;
        }
    }

    if (unreachable_count >= 3) {
        log_warning("Peer {} appears to have failed (3+ consecutive health check failures)", peer_uuid);

        // Notify Raft cluster
        propose_remove_node(peer_uuid, reason: "health_check_failure");

        // Fire alert
        fire_alert(AlertSeverity::CRITICAL, "PEER_FAILURE_DETECTED", peer_uuid.to_string());
    }
}
```

---

## 9. Alerting

### 9.1 Alert Rules

```cpp
struct AlertRule {
    string name;                         // Alert name (e.g., "HighQueryLatency")
    string description;                  // Human-readable description
    AlertSeverity severity;              // WARNING | CRITICAL

    string metric_query;                 // PromQL-style query
    string condition;                    // Threshold condition (e.g., "> 1.0")
    duration_t evaluation_interval;      // How often to evaluate (e.g., 1m)
    duration_t for_duration;             // Condition must hold for this long

    map<string, string> labels;          // Alert labels
    string notification_channel;         // Where to send alert (email, Slack, etc.)
};
```

**Example Alert Rules**:
```yaml
# High query latency
- name: HighQueryLatency
  description: "Query p95 latency > 1 second"
  severity: WARNING
  metric_query: "scratchbird_query_duration_p95 > 1.0"
  evaluation_interval: 1m
  for_duration: 5m
  notification_channel: slack_ops

# Excessive transaction rollbacks
- name: HighRollbackRate
  description: "Rollback rate > 10%"
  severity: WARNING
  metric_query: "rate(scratchbird_transactions_rolled_back_total[5m]) / rate(scratchbird_transactions_started_total[5m]) > 0.1"
  evaluation_interval: 1m
  for_duration: 10m
  notification_channel: email_dba

# Cluster stability degraded
- name: ClusterInstability
  description: "Cluster stability score < 0.5"
  severity: CRITICAL
  metric_query: "scratchbird_cluster_stability_score < 0.5"
  evaluation_interval: 1m
  for_duration: 5m
  notification_channel: pagerduty
```

### 9.2 Alert Evaluation

```cpp
void evaluate_alert_rules() {
    for (const AlertRule& rule : observability_policy.alert_rules) {
        // Execute metric query
        double metric_value = execute_metric_query(rule.metric_query);

        // Evaluate condition
        bool condition_met = evaluate_condition(metric_value, rule.condition);

        // Check if condition has held for required duration
        if (condition_met) {
            // Update alert state
            alert_state[rule.name].condition_start_time = alert_state[rule.name].condition_start_time.value_or(now());

            duration_t duration_held = now() - alert_state[rule.name].condition_start_time.value();

            if (duration_held >= rule.for_duration) {
                // Fire alert (if not already firing)
                if (!alert_state[rule.name].firing) {
                    fire_alert(rule.severity, rule.name, rule.description);
                    alert_state[rule.name].firing = true;
                }
            }
        } else {
            // Condition not met - reset
            if (alert_state[rule.name].firing) {
                resolve_alert(rule.name);
                alert_state[rule.name].firing = false;
            }
            alert_state[rule.name].condition_start_time = nullopt;
        }
    }
}
```

---

## 10. Operational Procedures

### 10.1 Query Metrics

```sql
-- View query latency percentiles
SELECT
    node_uuid,
    shard_id,
    query_type,
    p50_latency_ms,
    p95_latency_ms,
    p99_latency_ms
FROM sys.query_latency_metrics
WHERE observation_time > NOW() - INTERVAL '1 hour'
ORDER BY p99_latency_ms DESC;
```

### 10.2 View Distributed Trace

```sql
-- View trace by trace_id
SELECT
    span_id,
    parent_span_id,
    operation_name,
    start_time,
    duration_ms,
    status,
    attributes
FROM sys.traces
WHERE trace_id = 'trace_uuid_here'
ORDER BY start_time ASC;
```

### 10.3 Verify Audit Chain

```sql
-- Verify audit chain integrity
SELECT verify_audit_chain(
    start_sequence => 1000,
    end_sequence => 2000
);
```

### 10.4 View Cluster Churn

```sql
-- View cluster stability over time
SELECT
    observation_time,
    node_join_count,
    node_leave_count,
    leader_election_count,
    cluster_stability_score
FROM sys.churn_metrics
WHERE observation_time > NOW() - INTERVAL '24 hours'
ORDER BY observation_time ASC;
```

### 10.5 Export Audit Events

```sql
-- Export audit events to CSV for compliance
COPY (
    SELECT
        event_uuid,
        event_type,
        user_uuid,
        node_uuid,
        event_timestamp,
        canonical_json
    FROM sys.audit_events
    WHERE event_timestamp BETWEEN '2026-01-01' AND '2026-01-31'
) TO '/export/audit_events_jan_2026.csv' WITH CSV HEADER;
```

---

## 11. Security Considerations

### 11.1 Metrics Access Control

**Who can view metrics?**
- Cluster administrators
- Users with `VIEW_METRICS` privilege

**Who can export metrics?**
- Cluster administrators only

### 11.2 Trace Data Privacy

**Personally Identifiable Information (PII)**:
- SQL queries may contain PII (e.g., `WHERE email = 'alice@example.com'`)
- Span attributes should NOT include raw query parameters
- Use parameterized queries in span attributes: `SELECT * FROM users WHERE id = ?`

### 11.3 Audit Log Immutability

**Protection Mechanisms**:
1. **Cryptographic Hash Chain**: Tampering breaks chain verification
2. **Digital Signatures**: Node signatures prevent forgery
3. **Raft Replication**: Audit events replicated to quorum before ack
4. **Append-Only Storage**: Audit events cannot be deleted or modified

**Audit Event Retention**:
- Audit events are permanent (compliance requirement)
- Old events can be archived to cold storage (S3 Glacier, etc.)
- Archived events must preserve hash chain integrity

### 11.4 Observability Data Encryption

**At Rest**:
- Metrics: Encrypted in Prometheus TSDB (AES-256-GCM)
- Traces: Encrypted in Tempo backend (AES-256-GCM)
- Audit Logs: Encrypted in PostgreSQL (TDE)

**In Transit**:
- All observability data transported over mTLS
- OTLP exporters use TLS 1.3

---

## 12. Testing Procedures

### 12.1 Metrics Collection Test

```cpp
TEST(Observability, MetricsCollection) {
    auto cluster = create_test_cluster(3, 16);

    // Execute queries to generate metrics
    for (int i = 0; i < 100; i++) {
        cluster.execute_query("SELECT * FROM test_table");
    }

    // Scrape metrics from node
    string metrics = cluster.get_node(0).scrape_prometheus_metrics();

    // Verify query counter
    ASSERT_THAT(metrics, HasSubstr("scratchbird_queries_total"));
    ASSERT_THAT(metrics, HasSubstr("100"));
}
```

### 12.2 Distributed Tracing Test

```cpp
TEST(Observability, DistributedTracing) {
    auto cluster = create_test_cluster(3, 16);

    // Enable tracing with 100% sample rate
    cluster.set_trace_sample_rate(1.0);

    // Execute distributed query
    TraceContext root_ctx = create_trace_context();
    ResultSet result = cluster.execute_query_with_trace(
        "SELECT * FROM sharded_table WHERE id > 100",
        root_ctx
    );

    // Retrieve trace
    Trace trace = cluster.get_trace(root_ctx.trace_id);

    // Verify spans
    EXPECT_GE(trace.spans.size(), 3);  // Root + 2+ child spans
    EXPECT_EQ(trace.spans[0].operation_name, "cluster_query");
}
```

### 12.3 Audit Chain Verification Test

```cpp
TEST(Observability, AuditChainVerification) {
    auto cluster = create_test_cluster(3, 16);

    // Generate audit events
    cluster.login_user("alice");
    cluster.execute_query("CREATE TABLE test (id INT)");
    cluster.execute_query("INSERT INTO test VALUES (1)");
    cluster.logout_user("alice");

    // Retrieve audit events
    vector<AuditEvent> events = cluster.get_audit_events(start: 1, end: 4);

    // Verify chain integrity
    ASSERT_TRUE(verify_audit_chain(events));

    // Tamper with event
    events[2].canonical_json = "{\"tampered\": true}";

    // Verification should fail
    ASSERT_FALSE(verify_audit_chain(events));
}
```

### 12.4 Churn Metrics Test

```cpp
TEST(Observability, ChurnMetrics) {
    auto cluster = create_test_cluster(3, 16);

    // Simulate node join
    cluster.add_node("node_d");

    // Wait for metrics update
    sleep(1s);

    // Check churn metrics
    ChurnMetric churn = cluster.get_current_churn_metric();
    EXPECT_EQ(churn.node_join_count, 1);

    // Simulate leader election
    cluster.force_leader_election();

    // Wait for metrics update
    sleep(1s);

    // Check churn metrics
    churn = cluster.get_current_churn_metric();
    EXPECT_EQ(churn.leader_election_count, 1);
}
```

---

## 13. Performance Characteristics

### 13.1 Metrics Collection Overhead

| Operation | Overhead | Notes |
|-----------|----------|-------|
| Counter increment | 50-100 ns | Lock-free atomic increment |
| Gauge set | 50-100 ns | Lock-free atomic write |
| Histogram observation | 200-500 ns | Lock + bucket lookup |
| Prometheus scrape | 5-10 ms | Full metrics export (1000 metrics) |

### 13.2 Tracing Overhead

| Sample Rate | Overhead per Query | Notes |
|-------------|-------------------|-------|
| 1% | 10-50 μs | Minimal overhead |
| 10% | 50-200 μs | Recommended for production |
| 100% | 200-1000 μs | Only for debugging |

### 13.3 Audit Chain Performance

| Operation | Latency | Throughput |
|-----------|---------|------------|
| Append audit event | 1-5 ms | 10,000+ events/sec per node |
| Verify chain (1000 events) | 50-100 ms | 10,000+ events/sec |
| Checkpoint creation | 100-500 ms | Hourly background task |

---

## 14. Examples

### 14.1 Custom Metric

```cpp
// Define custom metric for cache hit rate
class CacheHitRateMetric {
public:
    void record_cache_access(bool hit) {
        if (hit) {
            metrics_collector.increment_counter("scratchbird_cache_hits_total", {});
        } else {
            metrics_collector.increment_counter("scratchbird_cache_misses_total", {});
        }
    }

    double calculate_hit_rate() {
        double hits = metrics_collector.get_counter("scratchbird_cache_hits_total");
        double misses = metrics_collector.get_counter("scratchbird_cache_misses_total");
        return hits / (hits + misses);
    }
};
```

### 14.2 Custom Trace Span

```cpp
void custom_operation_with_tracing() {
    // Extract trace context from current request
    TraceContext ctx = get_current_trace_context();

    // Create custom span
    Span span = create_span(ctx.trace_id, ctx.parent_span_id, "custom_operation");
    span.set_attribute("custom.param", "value");

    try {
        // Perform operation
        perform_custom_operation();

        span.set_status(SpanStatus::OK);
    } catch (const Exception& e) {
        span.set_status(SpanStatus::ERROR, e.message());
        throw;
    } finally {
        span.end();
        export_span(span);
    }
}
```

### 14.3 Custom Audit Event

```cpp
void log_custom_audit_event(const UUID& user_uuid, const string& action) {
    AuditEvent event;
    event.event_uuid = UUID::v7();
    event.event_sequence = get_next_audit_sequence();
    event.event_type = AuditEventType::CUSTOM_ACTION;
    event.severity = AuditSeverity::INFO;
    event.node_uuid = my_node_uuid;
    event.user_uuid = user_uuid;
    event.session_uuid = get_current_session_uuid();

    // Build canonical JSON
    json payload = {
        {"action", action},
        {"timestamp", now().to_iso8601()}
    };
    event.canonical_json = payload.dump(-1, ' ', false, json::error_handler_t::strict);

    // Calculate hash (including prev hash)
    event.prev_event_hash = get_last_audit_event_hash();
    event.event_hash = calculate_audit_hash(event);

    // Sign event
    event.signature = sign_audit_event(event, my_node_private_key);

    // Append to audit log
    append_audit_event(event);
}
```

---

## 15. Integration with External Systems

### 15.1 Prometheus Integration

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'scratchbird_cluster'
    scrape_interval: 15s
    static_configs:
      - targets:
          - 'node-a:9090'
          - 'node-b:9090'
          - 'node-c:9090'
    relabel_configs:
      - source_labels: [__address__]
        target_label: instance
```

### 15.2 Grafana Dashboards

**ScratchBird Overview Dashboard**:
- Query rate (queries/sec)
- Query latency (p50, p95, p99)
- Active transactions
- Cluster stability score
- Leader election count
- Shard distribution

**ScratchBird Storage Dashboard**:
- Heap pages used
- TOAST pages used
- Page cache hit rate
- GC queue depth

### 15.3 Jaeger Integration

```yaml
# jaeger-collector.yml
receivers:
  otlp:
    protocols:
      grpc:
        endpoint: 0.0.0.0:4317
      http:
        endpoint: 0.0.0.0:4318

exporters:
  jaeger:
    endpoint: localhost:14250

service:
  pipelines:
    traces:
      receivers: [otlp]
      exporters: [jaeger]
```

### 15.4 ELK Stack Integration

```yaml
# logstash.conf
input {
  http {
    port => 8080
    codec => json
  }
}

filter {
  if [source] == "scratchbird" {
    mutate {
      add_field => { "[@metadata][index]" => "scratchbird-logs-%{+YYYY.MM.dd}" }
    }
  }
}

output {
  elasticsearch {
    hosts => ["localhost:9200"]
    index => "%{[@metadata][index]}"
  }
}
```

---

## 16. References

### 16.1 Internal References
- **SBCLUSTER-00**: Guiding Principles
- **SBCLUSTER-01**: Cluster Configuration Epoch
- **SBCLUSTER-02**: Membership and Identity
- **Security Spec 08**: Audit Compliance
- **Security Spec 08.01**: Audit Event Canonicalization
- **Security Spec 08.02**: Audit Chain Verification Checkpoints

### 16.2 External Standards
- **OpenTelemetry**: https://opentelemetry.io/
- **Prometheus**: https://prometheus.io/
- **W3C Trace Context**: https://www.w3.org/TR/trace-context/
- **OTLP Protocol**: https://opentelemetry.io/docs/specs/otel/protocol/

### 16.3 Research and Best Practices (2025)
- [OpenTelemetry Best Practices](https://betterstack.com/community/guides/observability/opentelemetry-best-practices/)
- [Distributed Tracing with OpenTelemetry](https://medium.com/@shbhggrwl/backend-observability-in-2025-distributed-tracing-with-opentelemetry-af338a987abb)
- [Immutable Audit Log Architecture](https://www.emergentmind.com/topics/immutable-audit-log)
- [Cryptographically Signed Tamper-Proof Logs](https://www.cossacklabs.com/blog/audit-logs-security/)
- [Key Metrics for Monitoring Consul](https://www.datadoghq.com/blog/consul-metrics/)
- [Leader Election in Distributed Systems](https://www.devahmedali.click/post/leader-election-in-distributed-systems-complete-guide)

### 16.4 Research Papers
- *"Dapper, a Large-Scale Distributed Systems Tracing Infrastructure"* - Google, 2010
- *"Merkle Trees and Their Use in Cryptographic Protocols"* - Merkle, 1987
- *"The Raft Consensus Algorithm"* - Ongaro & Ousterhout, 2014

---

## 17. Revision History

| Version | Date       | Author       | Changes                                    |
|---------|------------|--------------|--------------------------------------------|
| 1.0     | 2026-01-02 | D. Calford   | Initial comprehensive specification based on OpenTelemetry 2025 best practices, cryptographic audit chains, and Raft cluster monitoring |

---

**Document Status**: DRAFT (Beta Specification Phase)
**Next Review**: Before Beta Implementation Phase
**Approval Required**: Chief Architect, Security Lead, Operations Lead

---

## Appendix A: Metric Naming Conventions

All ScratchBird metrics follow these conventions:

**Namespace**: `scratchbird_`

**Metric Types**:
- Counters: `_total` suffix (e.g., `scratchbird_queries_total`)
- Gauges: No suffix (e.g., `scratchbird_active_queries`)
- Histograms: `_seconds`, `_bytes`, etc. (e.g., `scratchbird_query_duration_seconds`)

**Units**:
- Time: `_seconds` (not milliseconds or microseconds)
- Bytes: `_bytes` (not KB or MB)
- Ratios: No unit (0.0 to 1.0)

**Labels**:
- Use `snake_case` for label names
- Keep cardinality low (< 1000 unique values per label)
- Never use UUIDs in labels (use `node_id`, `shard_id` instead of full UUIDs)

---

## Appendix B: Audit Event Type Reference

| Event Type | Severity | Description | Payload Fields |
|------------|----------|-------------|----------------|
| LOGIN_SUCCESS | INFO | User logged in | user_uuid, ip_address, auth_method |
| LOGIN_FAILURE | WARNING | Login attempt failed | username, ip_address, failure_reason |
| QUERY_EXECUTED | INFO | SQL query executed | query_hash, rows_affected, duration_ms |
| DDL_EXECUTED | WARNING | Schema change executed | ddl_type, object_name, sql_text |
| PERMISSION_DENIED | WARNING | Authorization failed | user_uuid, action, resource, policy_violated |
| ROLE_GRANTED | WARNING | Role assigned to user | grantee_uuid, role_name, grantor_uuid |
| ENCRYPTION_KEY_ROTATED | CRITICAL | Encryption key rotated | key_id, rotation_reason, previous_key_expiry |
| NODE_JOINED | INFO | Node joined cluster | node_uuid, node_role, cluster_epoch |
| LEADER_ELECTED | INFO | Raft leader elected | leader_uuid, term, election_duration_ms |
| DATA_EXPORT | CRITICAL | Data exported from cluster | export_format, row_count, destination, user_uuid |

---

## Appendix C: Observability Glossary

| Term | Definition |
|------|------------|
| **Cardinality** | Number of unique combinations of metric label values |
| **Exemplar** | Sample trace associated with a metric (e.g., slowest query in a histogram bucket) |
| **Golden Signals** | Latency, Traffic, Errors, Saturation (Google SRE) |
| **Merkle Tree** | Hash tree where each leaf is a hash of a data block, enabling efficient verification |
| **OTLP** | OpenTelemetry Protocol (for transmitting telemetry data) |
| **RED Metrics** | Rate, Errors, Duration (service health indicators) |
| **Sample Rate** | Percentage of traces collected (e.g., 0.1 = 10%) |
| **Span** | Single operation in a distributed trace |
| **Trace** | End-to-end request path across multiple services |
| **W3C Trace Context** | Standard for propagating trace context across services |
