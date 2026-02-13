# Monitoring and Observability

## Overview

This document describes the comprehensive monitoring, observability, and alerting strategy for the distributed database system. It covers metrics collection, logging, tracing, dashboards, and alerting rules across all tiers.

## Monitoring Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Data Sources                                            │
│  ├─ TX Engines (Prometheus exporters)                   │
│  ├─ Ingestion Engines (Prometheus exporters)            │
│  ├─ OLAP Shards (Prometheus exporters)                  │
│  ├─ Kafka Brokers (JMX exporters)                       │
│  ├─ Clock Masters (Prometheus exporters)                │
│  └─ Application Logs (JSON structured)                  │
└───────────────────────┬─────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        │               │               │
┌───────▼────────┐ ┌────▼──────┐ ┌─────▼────────┐
│  Prometheus    │ │ Loki      │ │ Jaeger       │
│  (Metrics)     │ │ (Logs)    │ │ (Traces)     │
└───────┬────────┘ └────┬──────┘ └─────┬────────┘
        │               │               │
        └───────────────┼───────────────┘
                        │
                ┌───────▼────────┐
                │  Grafana       │
                │  (Dashboards)  │
                └───────┬────────┘
                        │
                ┌───────▼────────┐
                │  AlertManager  │
                │  (Alerting)    │
                └────────────────┘
```

## Metrics Collection

### Prometheus Exporters

Each component exposes metrics via HTTP endpoint:

```c
// metrics.h - Metrics exporter interface

struct MetricsExporter {
    uint16_t port;                    // HTTP port for /metrics
    pthread_t server_thread;
    
    // Metrics registry
    MetricRegistry* registry;
};

// Start metrics exporter
int start_metrics_exporter(MetricsExporter* exporter, uint16_t port) {
    exporter->port = port;
    exporter->registry = create_metric_registry();
    
    // Start HTTP server on port
    pthread_create(&exporter->server_thread, NULL, 
                   metrics_server_thread, exporter);
    
    return 0;
}

// HTTP handler for /metrics endpoint
void handle_metrics_request(MetricsExporter* exporter, HTTPResponse* response) {
    // Generate Prometheus format
    char* metrics_text = serialize_metrics_prometheus(exporter->registry);
    
    http_response_set_body(response, metrics_text);
    http_response_set_header(response, "Content-Type", "text/plain");
}
```

### Key Metrics by Component

#### Transaction Engine Metrics

```c
// TX Engine metrics

// Throughput
COUNTER(tx_engine_transactions_total, "Total transactions processed");
COUNTER(tx_engine_transactions_committed, "Successfully committed transactions");
COUNTER(tx_engine_transactions_rolled_back, "Rolled back transactions");

// Latency
HISTOGRAM(tx_engine_transaction_duration_seconds, 
          "Transaction execution time",
          {0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0});

HISTOGRAM(tx_engine_query_duration_seconds,
          "Query execution time",
          {0.0001, 0.001, 0.01, 0.1, 1.0});

// Cache
GAUGE(tx_engine_cache_size_bytes, "Current cache size");
COUNTER(tx_engine_cache_hits_total, "Cache hits");
COUNTER(tx_engine_cache_misses_total, "Cache misses");

// Connections
GAUGE(tx_engine_connections_active, "Active connections");
GAUGE(tx_engine_connections_idle, "Idle connections");

// Replication
COUNTER(tx_engine_replication_events_sent, "Events sent to replication");
GAUGE(tx_engine_replication_queue_depth, "Replication queue depth");
GAUGE(tx_engine_replication_lag_seconds, "Replication lag");

// Resources
GAUGE(tx_engine_memory_used_bytes, "Memory usage");
GAUGE(tx_engine_cpu_usage_percent, "CPU usage");

// Clock sync
GAUGE(tx_engine_clock_uncertainty_nanoseconds, "Clock uncertainty");
GAUGE(tx_engine_clock_offset_nanoseconds, "Clock offset from master");
COUNTER(tx_engine_clock_heartbeats_missed_total, "Missed heartbeats");
```

#### Ingestion Engine Metrics

```c
// Ingestion Engine metrics

// Throughput
COUNTER(ingestion_events_received_total, "Total change events received");
COUNTER(ingestion_events_processed_total, "Events successfully processed");
COUNTER(ingestion_events_failed_total, "Events that failed processing");

// Latency
HISTOGRAM(ingestion_event_processing_duration_seconds,
          "Event processing time",
          {0.001, 0.01, 0.1, 1.0, 10.0});

// Queue depths
GAUGE(ingestion_merge_queue_depth, "Merge queue depth");
GAUGE(ingestion_olap_push_queue_depth, "OLAP push queue depth");

// Conflicts
COUNTER(ingestion_conflicts_detected_total, "Conflicts detected");
COUNTER(ingestion_conflicts_resolved_total, "Conflicts resolved");
GAUGE(ingestion_conflict_rate, "Conflict rate (%)");

// Deduplication
COUNTER(ingestion_duplicates_skipped_total, "Duplicate events skipped");
GAUGE(ingestion_dedup_rate, "Deduplication rate (%)");

// Validation
COUNTER(ingestion_validation_failures_total, "Validation failures");

// OLAP push
COUNTER(ingestion_olap_batches_pushed_total, "Batches pushed to OLAP");
HISTOGRAM(ingestion_olap_push_duration_seconds, "OLAP push duration");
GAUGE(ingestion_olap_push_lag_seconds, "OLAP push lag");

// Per-source metrics (by server_id)
COUNTER(ingestion_events_by_source, "Events received per TX engine", 
        labels=["server_id"]);
GAUGE(ingestion_lag_by_source_seconds, "Replication lag per TX engine",
      labels=["server_id"]);
```

#### OLAP Shard Metrics

```c
// OLAP Shard metrics

// Query performance
COUNTER(olap_queries_total, "Total queries executed");
HISTOGRAM(olap_query_duration_seconds, "Query execution time",
          {0.1, 0.5, 1.0, 5.0, 10.0, 30.0, 60.0});

// Storage
GAUGE(olap_total_rows, "Total rows stored");
GAUGE(olap_total_bytes_uncompressed, "Total uncompressed bytes");
GAUGE(olap_total_bytes_compressed, "Total compressed bytes");
GAUGE(olap_compression_ratio, "Compression ratio");

// Partitions
GAUGE(olap_partitions_total, "Total partitions");
GAUGE(olap_partitions_active, "Active partitions");
GAUGE(olap_partitions_cold, "Cold partitions");

// I/O
COUNTER(olap_bytes_read_total, "Bytes read from storage");
COUNTER(olap_bytes_written_total, "Bytes written to storage");

// Compaction
GAUGE(olap_compactions_running, "Running compactions");
COUNTER(olap_compactions_completed_total, "Completed compactions");
HISTOGRAM(olap_compaction_duration_seconds, "Compaction duration");
COUNTER(olap_bytes_compacted_total, "Bytes compacted");

// Cache
GAUGE(olap_cache_size_bytes, "Query result cache size");
COUNTER(olap_cache_hits_total, "Cache hits");
COUNTER(olap_cache_misses_total, "Cache misses");

// Query optimization
COUNTER(olap_partitions_pruned_total, "Partitions pruned by query optimizer");
COUNTER(olap_columns_pruned_total, "Columns pruned by query optimizer");
```

#### Clock Master Metrics

```c
// Clock Master metrics

GAUGE(clock_master_current_timestamp_nanoseconds, "Current timestamp");
GAUGE(clock_master_uncertainty_nanoseconds, "Current uncertainty bound");
GAUGE(clock_master_jitter_nanoseconds, "Clock jitter");
GAUGE(clock_master_drift_ppm, "Clock drift (parts per million)");

COUNTER(clock_master_heartbeats_sent_total, "Heartbeats sent");
GAUGE(clock_master_clients_connected, "Connected clients");

// Per-source metrics (GPS, NTP, PTP)
GAUGE(clock_master_source_offset_nanoseconds, "Offset from time source",
      labels=["source"]);
GAUGE(clock_master_source_quality, "Source quality indicator",
      labels=["source"]);
```

## Prometheus Configuration

```yaml
# prometheus.yml

global:
  scrape_interval: 15s
  evaluation_interval: 15s
  external_labels:
    cluster: 'production-cluster'
    environment: 'production'

# Alertmanager configuration
alerting:
  alertmanagers:
    - static_configs:
        - targets:
            - alertmanager:9093

# Load alerting rules
rule_files:
  - '/etc/prometheus/rules/*.yml'

# Scrape configurations
scrape_configs:
  # TX Engines
  - job_name: 'tx-engines'
    ec2_sd_configs:
      - region: us-east-1
        port: 9090
        filters:
          - name: tag:Tier
            values: [transaction]
    relabel_configs:
      - source_labels: [__meta_ec2_instance_id]
        target_label: instance_id
      - source_labels: [__meta_ec2_private_ip]
        target_label: instance
        replacement: '${1}:9090'
  
  # Ingestion Engines
  - job_name: 'ingestion-engines'
    ec2_sd_configs:
      - region: us-east-1
        port: 9091
        filters:
          - name: tag:Tier
            values: [ingestion]
  
  # OLAP Shards
  - job_name: 'olap-shards'
    ec2_sd_configs:
      - region: us-east-1
        port: 9092
        filters:
          - name: tag:Tier
            values: [olap]
  
  # Clock Masters
  - job_name: 'clock-masters'
    static_configs:
      - targets:
          - '10.0.1.10:9093'
          - '10.0.1.11:9093'
  
  # Kafka Brokers (JMX exporter)
  - job_name: 'kafka'
    static_configs:
      - targets:
          - 'kafka1:7071'
          - 'kafka2:7071'
          - 'kafka3:7071'
```

## Alerting Rules

```yaml
# /etc/prometheus/rules/database-alerts.yml

groups:
- name: transaction_engines
  interval: 30s
  rules:
  # High transaction latency
  - alert: HighTransactionLatency
    expr: histogram_quantile(0.99, tx_engine_transaction_duration_seconds) > 1.0
    for: 5m
    labels:
      severity: warning
      component: tx-engine
    annotations:
      summary: "High transaction latency on {{ $labels.instance }}"
      description: "P99 transaction latency is {{ $value }}s (threshold: 1s)"
  
  # Low cache hit rate
  - alert: LowCacheHitRate
    expr: |
      rate(tx_engine_cache_hits_total[5m]) / 
      (rate(tx_engine_cache_hits_total[5m]) + rate(tx_engine_cache_misses_total[5m])) < 0.7
    for: 10m
    labels:
      severity: warning
      component: tx-engine
    annotations:
      summary: "Low cache hit rate on {{ $labels.instance }}"
      description: "Cache hit rate is {{ $value | humanizePercentage }} (threshold: 70%)"
  
  # High replication lag
  - alert: HighReplicationLag
    expr: tx_engine_replication_lag_seconds > 5
    for: 3m
    labels:
      severity: critical
      component: tx-engine
    annotations:
      summary: "High replication lag on {{ $labels.instance }}"
      description: "Replication lag is {{ $value }}s (threshold: 5s)"
  
  # Clock sync issues
  - alert: ClockSyncDegraded
    expr: tx_engine_clock_uncertainty_nanoseconds > 100000000  # 100ms
    for: 5m
    labels:
      severity: warning
      component: clock-sync
    annotations:
      summary: "Clock uncertainty too high on {{ $labels.instance }}"
      description: "Clock uncertainty is {{ $value | humanize }}ns (threshold: 100ms)"
  
  - alert: ClockSyncLost
    expr: tx_engine_clock_heartbeats_missed_total > 10
    for: 1m
    labels:
      severity: critical
      component: clock-sync
    annotations:
      summary: "Clock sync lost on {{ $labels.instance }}"
      description: "Missed {{ $value }} heartbeats"

- name: ingestion_engines
  interval: 30s
  rules:
  # High conflict rate
  - alert: HighConflictRate
    expr: ingestion_conflict_rate > 5.0
    for: 10m
    labels:
      severity: warning
      component: ingestion
    annotations:
      summary: "High conflict rate on {{ $labels.instance }}"
      description: "Conflict rate is {{ $value }}% (threshold: 5%)"
  
  # OLAP push lag
  - alert: OLAPPushLagHigh
    expr: ingestion_olap_push_lag_seconds > 600  # 10 minutes
    for: 5m
    labels:
      severity: warning
      component: ingestion
    annotations:
      summary: "OLAP push lag high on {{ $labels.instance }}"
      description: "Push lag is {{ $value }}s (threshold: 600s)"
  
  # Queue buildup
  - alert: MergeQueueBackup
    expr: ingestion_merge_queue_depth > 100000
    for: 5m
    labels:
      severity: warning
      component: ingestion
    annotations:
      summary: "Merge queue backing up on {{ $labels.instance }}"
      description: "Queue depth is {{ $value }} events (threshold: 100k)"

- name: olap_shards
  interval: 30s
  rules:
  # Slow queries
  - alert: SlowOLAPQueries
    expr: histogram_quantile(0.99, olap_query_duration_seconds) > 60
    for: 10m
    labels:
      severity: warning
      component: olap
    annotations:
      summary: "Slow OLAP queries on {{ $labels.instance }}"
      description: "P99 query time is {{ $value }}s (threshold: 60s)"
  
  # Storage approaching capacity
  - alert: StorageHighUtilization
    expr: olap_total_bytes_compressed / node_filesystem_size_bytes > 0.85
    for: 1h
    labels:
      severity: warning
      component: olap
    annotations:
      summary: "Storage utilization high on {{ $labels.instance }}"
      description: "Storage is {{ $value | humanizePercentage }} full (threshold: 85%)"
  
  # Compaction lag
  - alert: CompactionLag
    expr: olap_compactions_running == 0 and 
          time() - olap_last_compaction_timestamp > 7200  # 2 hours
    for: 30m
    labels:
      severity: warning
      component: olap
    annotations:
      summary: "No compaction running on {{ $labels.instance }}"
      description: "Last compaction was {{ $value | humanizeDuration }} ago"

- name: cluster_health
  interval: 60s
  rules:
  # Cluster-wide replication lag
  - alert: ClusterReplicationLagHigh
    expr: max(ingestion_lag_by_source_seconds) > 10
    for: 5m
    labels:
      severity: critical
      component: cluster
    annotations:
      summary: "Cluster replication lag is high"
      description: "Maximum lag across all sources is {{ $value }}s"
  
  # Too few TX engines
  - alert: InsufficientTXEngines
    expr: count(up{job="tx-engines"} == 1) < 2
    for: 5m
    labels:
      severity: critical
      component: cluster
    annotations:
      summary: "Insufficient TX engines running"
      description: "Only {{ $value }} TX engines are healthy (minimum: 2)"
```

## Grafana Dashboards

### Main Cluster Dashboard

```json
{
  "dashboard": {
    "title": "Database Cluster Overview",
    "panels": [
      {
        "title": "Cluster Health",
        "type": "stat",
        "targets": [
          {
            "expr": "count(up{job=~\"tx-engines|ingestion-engines|olap-shards\"} == 1)"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "thresholds": {
              "steps": [
                {"value": 0, "color": "red"},
                {"value": 8, "color": "yellow"},
                {"value": 10, "color": "green"}
              ]
            }
          }
        }
      },
      {
        "title": "Transaction Throughput",
        "type": "graph",
        "targets": [
          {
            "expr": "sum(rate(tx_engine_transactions_committed[5m]))",
            "legendFormat": "Commits/s"
          },
          {
            "expr": "sum(rate(tx_engine_transactions_rolled_back[5m]))",
            "legendFormat": "Rollbacks/s"
          }
        ]
      },
      {
        "title": "Transaction Latency (P99)",
        "type": "graph",
        "targets": [
          {
            "expr": "histogram_quantile(0.99, sum(rate(tx_engine_transaction_duration_seconds_bucket[5m])) by (le))",
            "legendFormat": "P99 Latency"
          }
        ]
      },
      {
        "title": "Cache Hit Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "sum(rate(tx_engine_cache_hits_total[5m])) / (sum(rate(tx_engine_cache_hits_total[5m])) + sum(rate(tx_engine_cache_misses_total[5m])))",
            "legendFormat": "Hit Rate"
          }
        ]
      },
      {
        "title": "Replication Lag",
        "type": "graph",
        "targets": [
          {
            "expr": "max(tx_engine_replication_lag_seconds)",
            "legendFormat": "Max Lag"
          },
          {
            "expr": "avg(tx_engine_replication_lag_seconds)",
            "legendFormat": "Avg Lag"
          }
        ]
      },
      {
        "title": "Clock Uncertainty",
        "type": "graph",
        "targets": [
          {
            "expr": "max(tx_engine_clock_uncertainty_nanoseconds) / 1000000",
            "legendFormat": "Max Uncertainty (ms)"
          }
        ]
      },
      {
        "title": "OLAP Query Performance",
        "type": "graph",
        "targets": [
          {
            "expr": "sum(rate(olap_queries_total[5m]))",
            "legendFormat": "Queries/s"
          }
        ]
      },
      {
        "title": "Storage Compression",
        "type": "stat",
        "targets": [
          {
            "expr": "sum(olap_total_bytes_uncompressed) / sum(olap_total_bytes_compressed)"
          }
        ],
        "fieldConfig": {
          "defaults": {
            "unit": "none",
            "decimals": 2
          }
        }
      }
    ]
  }
}
```

### TX Engine Detail Dashboard

```json
{
  "dashboard": {
    "title": "TX Engine Details",
    "templating": {
      "list": [
        {
          "name": "instance",
          "type": "query",
          "query": "label_values(tx_engine_transactions_total, instance)"
        }
      ]
    },
    "panels": [
      {
        "title": "Transaction Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "rate(tx_engine_transactions_committed{instance=\"$instance\"}[5m])"
          }
        ]
      },
      {
        "title": "Active Connections",
        "type": "graph",
        "targets": [
          {
            "expr": "tx_engine_connections_active{instance=\"$instance\"}"
          }
        ]
      },
      {
        "title": "Memory Usage",
        "type": "graph",
        "targets": [
          {
            "expr": "tx_engine_memory_used_bytes{instance=\"$instance\"} / 1024 / 1024 / 1024",
            "legendFormat": "Memory (GB)"
          }
        ]
      },
      {
        "title": "Cache Statistics",
        "type": "table",
        "targets": [
          {
            "expr": "tx_engine_cache_size_bytes{instance=\"$instance\"}"
          },
          {
            "expr": "rate(tx_engine_cache_hits_total{instance=\"$instance\"}[5m])"
          },
          {
            "expr": "rate(tx_engine_cache_misses_total{instance=\"$instance\"}[5m])"
          }
        ]
      }
    ]
  }
}
```

## Structured Logging

### Log Format

Use JSON structured logging for easy parsing and analysis:

```c
// logging.h

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_CRITICAL
};

void log_structured(LogLevel level, const char* component, 
                   const char* event, const char* format, ...) {
    json_t* log_entry = json_object();
    
    // Standard fields
    json_object_set_new(log_entry, "timestamp", 
                        json_string(get_iso8601_timestamp()));
    json_object_set_new(log_entry, "level", 
                        json_string(log_level_string(level)));
    json_object_set_new(log_entry, "component", 
                        json_string(component));
    json_object_set_new(log_entry, "event", 
                        json_string(event));
    json_object_set_new(log_entry, "host", 
                        json_string(get_hostname()));
    json_object_set_new(log_entry, "server_id", 
                        json_integer(get_server_id()));
    
    // Variable arguments for additional fields
    va_list args;
    va_start(args, format);
    add_custom_fields(log_entry, format, args);
    va_end(args);
    
    // Output JSON
    char* json_str = json_dumps(log_entry, JSON_COMPACT);
    fprintf(stderr, "%s\n", json_str);
    
    free(json_str);
    json_decref(log_entry);
}

// Example usage
log_structured(LOG_INFO, "tx-engine", "transaction_committed",
               "transaction_id=%lu,duration_ms=%lu,rows_affected=%d",
               txn->id, duration_ms, rows_affected);
```

### Example Log Output

```json
{
  "timestamp": "2024-01-15T10:30:45.123Z",
  "level": "INFO",
  "component": "tx-engine",
  "event": "transaction_committed",
  "host": "tx-engine-1.internal",
  "server_id": 1,
  "transaction_id": 12345678,
  "duration_ms": 15,
  "rows_affected": 3
}

{
  "timestamp": "2024-01-15T10:30:46.456Z",
  "level": "WARN",
  "component": "ingestion",
  "event": "conflict_detected",
  "host": "ingestion-1.internal",
  "server_id": 0,
  "table_id": 42,
  "row_key": "user:5",
  "conflicting_uuids": ["018c8e48-1000-...", "018c8e48-2000-..."],
  "resolution_strategy": "last_write_wins"
}

{
  "timestamp": "2024-01-15T10:30:47.789Z",
  "level": "ERROR",
  "component": "olap",
  "event": "query_failed",
  "host": "olap-shard-1.internal",
  "query_id": "q-98765",
  "error": "Partition not found",
  "partition_key": "2024-01-01",
  "duration_ms": 250
}
```

### Loki Configuration

```yaml
# loki-config.yaml

auth_enabled: false

server:
  http_listen_port: 3100

ingester:
  lifecycler:
    ring:
      kvstore:
        store: inmemory
      replication_factor: 1
  chunk_idle_period: 5m
  chunk_retain_period: 30s

schema_config:
  configs:
    - from: 2024-01-01
      store: boltdb
      object_store: filesystem
      schema: v11
      index:
        prefix: index_
        period: 168h

storage_config:
  boltdb:
    directory: /loki/index
  filesystem:
    directory: /loki/chunks

limits_config:
  enforce_metric_name: false
  reject_old_samples: true
  reject_old_samples_max_age: 168h

chunk_store_config:
  max_look_back_period: 0s

table_manager:
  retention_deletes_enabled: true
  retention_period: 2160h  # 90 days
```

### Promtail Configuration

```yaml
# promtail-config.yaml

server:
  http_listen_port: 9080
  grpc_listen_port: 0

positions:
  filename: /tmp/positions.yaml

clients:
  - url: http://loki:3100/loki/api/v1/push

scrape_configs:
  # TX Engine logs
  - job_name: tx-engines
    static_configs:
      - targets:
          - localhost
        labels:
          job: tx-engines
          __path__: /var/log/db/tx-engine/*.log
    pipeline_stages:
      - json:
          expressions:
            timestamp: timestamp
            level: level
            component: component
            event: event
      - labels:
          level:
          component:
          event:
      - timestamp:
          source: timestamp
          format: RFC3339Nano
  
  # Ingestion logs
  - job_name: ingestion-engines
    static_configs:
      - targets:
          - localhost
        labels:
          job: ingestion-engines
          __path__: /var/log/db/ingestion/*.log
    pipeline_stages:
      - json:
          expressions:
            timestamp: timestamp
            level: level
            component: component
            event: event
      - labels:
          level:
          component:
          event:
```

## Distributed Tracing

### OpenTelemetry Integration

```c
// tracing.h - OpenTelemetry integration

#include <opentelemetry/trace/provider.h>

// Initialize tracing
void init_tracing(const char* service_name, const char* jaeger_endpoint) {
    auto exporter = jaeger_exporter::JaegerExporter::Create(
        jaeger_exporter::JaegerExporterOptions{
            .endpoint = jaeger_endpoint,
            .service_name = service_name
        }
    );
    
    auto processor = std::make_unique<SimpleSpanProcessor>(std::move(exporter));
    auto provider = std::make_shared<TracerProvider>(std::move(processor));
    
    trace::Provider::SetTracerProvider(provider);
}

// Create a traced transaction
Span* start_transaction_span(const char* sql_text) {
    auto tracer = trace::Provider::GetTracerProvider()->GetTracer("tx-engine");
    
    auto span = tracer->StartSpan("transaction",
        {
            {"db.system", "custom"},
            {"db.operation", "transaction"},
            {"db.statement", sql_text}
        }
    );
    
    return span;
}

// Example: Traced transaction execution
void execute_transaction_traced(Transaction* txn) {
    // Start span
    auto span = start_transaction_span(txn->sql_text);
    
    // Add transaction ID
    span->SetAttribute("transaction_id", txn->id);
    span->SetAttribute("server_id", get_server_id());
    
    // Execute
    int result = execute_transaction(txn);
    
    // Record result
    if (result == 0) {
        span->SetAttribute("transaction.committed", true);
        span->SetStatus(StatusCode::kOk);
    } else {
        span->SetAttribute("transaction.committed", false);
        span->SetAttribute("error.message", txn->error_message);
        span->SetStatus(StatusCode::kError);
    }
    
    // End span
    span->End();
}
```

### Trace Propagation

Propagate trace context across services:

```c
// Extract trace context from incoming request
TraceContext extract_trace_context(HTTPRequest* request) {
    // W3C Trace Context format
    const char* traceparent = http_request_get_header(request, "traceparent");
    const char* tracestate = http_request_get_header(request, "tracestate");
    
    return parse_trace_context(traceparent, tracestate);
}

// Inject trace context into outgoing request
void inject_trace_context(HTTPRequest* request, TraceContext* ctx) {
    char traceparent[256];
    snprintf(traceparent, sizeof(traceparent),
             "00-%s-%s-%02x",
             ctx->trace_id,
             ctx->span_id,
             ctx->trace_flags);
    
    http_request_set_header(request, "traceparent", traceparent);
    
    if (ctx->trace_state) {
        http_request_set_header(request, "tracestate", ctx->trace_state);
    }
}
```

## Performance Profiling

### Continuous Profiling

```yaml
# pyroscope-config.yaml

# Continuous profiling with Pyroscope
server:
  http_listen_port: 4040

targets:
  - job_name: tx-engines
    targets:
      - tx-engine-1:6060
      - tx-engine-2:6060
    labels:
      component: tx-engine
  
  - job_name: ingestion-engines
    targets:
      - ingestion-1:6060
      - ingestion-2:6060
    labels:
      component: ingestion
  
  - job_name: olap-shards
    targets:
      - olap-1:6060
      - olap-2:6060
    labels:
      component: olap
```

### CPU Profiling Integration

```c
// profiling.h

#ifdef ENABLE_PROFILING
#include <gperftools/profiler.h>

// Start CPU profiling
void start_cpu_profiling(const char* filename) {
    ProfilerStart(filename);
}

// Stop CPU profiling
void stop_cpu_profiling() {
    ProfilerStop();
}

// HTTP endpoint for on-demand profiling
void handle_profile_request(HTTPRequest* request, HTTPResponse* response) {
    int duration = http_request_get_param_int(request, "duration", 30);
    
    char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/profile-%ld.prof", time(NULL));
    
    ProfilerStart(filename);
    sleep(duration);
    ProfilerStop();
    
    // Return profile file
    http_response_send_file(response, filename);
}
#endif
```

## Operational Runbooks

### Runbook: High Replication Lag

```markdown
# Runbook: High Replication Lag

## Symptoms
- Alert: HighReplicationLag
- TX engine replication lag > 5 seconds
- OLAP data freshness degraded

## Investigation Steps

1. Check Kafka broker health:
   ```bash
   kafka-broker-api-versions.sh --bootstrap-server kafka1:9092
   ```

2. Check consumer lag:
   ```bash
   kafka-consumer-groups.sh --bootstrap-server kafka1:9092 \
     --describe --group ingestion-group-1
   ```

3. Check ingestion engine health:
   ```bash
   curl http://ingestion-1:9091/health
   ```

4. Check ingestion queue depth:
   ```bash
   curl http://ingestion-1:9091/metrics | grep merge_queue_depth
   ```

## Resolution Steps

### If Kafka is slow:
1. Check broker disk I/O: `iostat -x 1`
2. Check network bandwidth: `iftop`
3. Consider adding brokers or increasing resources

### If Ingestion is slow:
1. Scale up ingestion engines:
   ```bash
   ./scripts/add-ingestion-engine.sh
   ```

2. Increase ingestion parallelism:
   ```bash
   # Edit /etc/db/ingestion.yaml
   processing:
     worker_threads: 32  # Increase from 16
   ```

3. Restart ingestion service:
   ```bash
   systemctl restart ingestion-engine
   ```

### If TX engines are overwhelming:
1. Reduce replication batch size
2. Implement backpressure
3. Add TX engine instances to distribute load

## Prevention
- Set up auto-scaling for ingestion engines
- Monitor queue depths proactively
- Implement flow control in TX engines
```

### Runbook: Clock Sync Lost

```markdown
# Runbook: Clock Sync Lost

## Symptoms
- Alert: ClockSyncLost or ClockSyncDegraded
- Clock uncertainty > 100ms
- Missed heartbeats > 10

## Investigation Steps

1. Check clock master health:
   ```bash
   curl http://clock-master:9093/health
   systemctl status clock-master
   ```

2. Check network connectivity:
   ```bash
   ping -c 5 clock-master
   traceroute clock-master
   ```

3. Check chrony status on TX engines:
   ```bash
   chronyc sources
   chronyc tracking
   ```

4. Check for NTP source issues:
   ```bash
   # AWS Time Sync
   chronyc sources | grep 169.254.169.123
   
   # GCP
   chronyc sources | grep metadata.google
   ```

## Resolution Steps

### If clock master is down:
1. Failover to backup:
   ```bash
   ./scripts/failover-clock-master.sh
   ```

2. Investigate primary failure:
   ```bash
   journalctl -u clock-master -n 1000
   ```

### If network issue:
1. Check firewall rules
2. Check network connectivity
3. Restart networking if needed

### If NTP source issue:
1. Check internet connectivity
2. Add alternative NTP sources:
   ```bash
   # Edit /etc/chrony/chrony.conf
   server 0.pool.ntp.org iburst
   server 1.pool.ntp.org iburst
   ```

3. Restart chrony:
   ```bash
   systemctl restart chrony
   ```

### If TX engines desync:
1. Temporarily pause writes to affected engines
2. Wait for resync
3. Resume operations once uncertainty < 50ms

## Prevention
- Ensure clock master HA configuration
- Monitor clock master uptime
- Set up automated failover
- Use redundant NTP sources
```

## Dashboard Examples

### SQL Query Performance

```sql
-- Query performance by table (Prometheus query)
topk(10,
  sum by (table_name) (
    rate(olap_query_duration_seconds_sum[5m])
  ) / 
  sum by (table_name) (
    rate(olap_query_duration_seconds_count[5m])
  )
)

-- Cache efficiency by TX engine
sum by (instance) (rate(tx_engine_cache_hits_total[5m])) /
(
  sum by (instance) (rate(tx_engine_cache_hits_total[5m])) +
  sum by (instance) (rate(tx_engine_cache_misses_total[5m]))
)

-- Replication lag distribution
histogram_quantile(0.50, 
  sum(rate(tx_engine_replication_lag_seconds_bucket[5m])) by (le)
) as p50,
histogram_quantile(0.95,
  sum(rate(tx_engine_replication_lag_seconds_bucket[5m])) by (le)
) as p95,
histogram_quantile(0.99,
  sum(rate(tx_engine_replication_lag_seconds_bucket[5m])) by (le)
) as p99
```

This comprehensive monitoring and observability setup provides complete visibility into the distributed database system, enabling proactive issue detection and rapid troubleshooting.
