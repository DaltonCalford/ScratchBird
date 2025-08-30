## Configuration Options

This document enumerates configurable options discovered in the codebase, grouped by component. Each section has anchors and Implementation References back to the source files.

- See also: config examples in `docs/config/examples.md`.

### Engine (EngineConfig) {#engineconfig}

Implementation References:
- `include/scratchbird/engine/config.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| engine.allowed_page_sizes | std::vector<uint32_t> | (empty) | Global | `include/scratchbird/engine/config.h` |
| engine.default_page_size | uint32_t | 4096 | Global | `include/scratchbird/engine/config.h` |
| engine.prealloc_mb | uint32_t | 0 | Global | `include/scratchbird/engine/config.h` |
| engine.direct_io | bool | false | Global | `include/scratchbird/engine/config.h` |
| engine.checksum_policy | enum ChecksumPolicy | Crc32c | Global | `include/scratchbird/engine/config.h` |
| engine.fsync_policy | enum FsyncPolicy | Always | Global | `include/scratchbird/engine/config.h` |
| engine.prefetch_on_alloc | bool | false | Global | `include/scratchbird/engine/config.h` |
| engine.prefetch_horizon_pages | uint32_t | 128 | Global | `include/scratchbird/engine/config.h` |
| engine.bootstrap_execute | bool | false | Global | `include/scratchbird/engine/config.h` |
| engine.tablespaces_enabled | bool | true | Global | `include/scratchbird/engine/config.h` |
| engine.enable_partition_pruning | bool | true | Global | `include/scratchbird/engine/config.h` |
| engine.enable_partition_wise_ops | bool | false | Global | `include/scratchbird/engine/config.h` |
| engine.enable_materialized_views | bool | true | Global | `include/scratchbird/engine/config.h` |
| engine.enable_mv_incremental | bool | false | Global | `include/scratchbird/engine/config.h` |
| engine.enable_mv_concurrent_refresh | bool | false | Global | `include/scratchbird/engine/config.h` |
| engine.enable_query_rewrite | bool | false | Global | `include/scratchbird/engine/config.h` |
| engine.enable_global_indexes | bool | false | Global | `include/scratchbird/engine/config.h` |

### Performance (PerformanceConfiguration) {#performance}

Implementation References:
- `include/scratchbird/engine/performance_config.h`
- `src/engine/performance_config.cpp`

Component bundles:
- tcp_config: socket_receive_buffer_size (65536), socket_send_buffer_size (65536)
- connection_pool_config: max_connections (100), initial_pool_size (10)
- network_buffer_config: buffer_size (65536), auto_tuning_enabled (true)
- buffer_pool_config: buffer_count (1024), buffer_size (4096)

Global settings:

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| performance.enable_performance_monitoring | bool | true | Global | `include/scratchbird/engine/performance_config.h` |
| performance.metrics_collection_interval | std::chrono::seconds | 10 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.metrics_retention_period | std::chrono::seconds | 3600 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.enable_performance_alerts | bool | true | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_check_interval | std::chrono::seconds | 30 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.max_alerts_per_type | size_t | 10 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.enable_auto_tuning | bool | true | Global | `include/scratchbird/engine/performance_config.h` |
| performance.auto_tuning_interval | std::chrono::minutes | 15 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.auto_tuning_aggressiveness | double | 0.5 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.enable_performance_logging | bool | true | Global | `include/scratchbird/engine/performance_config.h` |
| performance.performance_log_level | std::string | "INFO" | Global | `include/scratchbird/engine/performance_config.h` |
| performance.max_log_entries | size_t | 10000 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.allow_runtime_config_changes | bool | true | Global | `include/scratchbird/engine/performance_config.h` |
| performance.protected_settings | std::vector<std::string> | (empty) | Global | `include/scratchbird/engine/performance_config.h` |

Thresholds (performance.alert_thresholds):

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| performance.alert_thresholds.cpu_usage_warning_percent | double | 80.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.cpu_usage_critical_percent | double | 95.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.memory_usage_warning_percent | double | 80.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.memory_usage_critical_percent | double | 90.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.buffer_hit_ratio_warning | double | 70.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.buffer_hit_ratio_critical | double | 50.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.avg_query_time_warning_ms | double | 1000.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.avg_query_time_critical_ms | double | 5000.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.slow_query_threshold_ms | double | 10000.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.connection_count_warning | uint64_t | 80 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.connection_count_critical | uint64_t | 95 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.disk_space_warning_percent | double | 80.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.disk_space_critical_percent | double | 90.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.error_rate_warning | double | 10.0 | Global | `include/scratchbird/engine/performance_config.h` |
| performance.alert_thresholds.error_rate_critical | double | 50.0 | Global | `include/scratchbird/engine/performance_config.h` |

### Buffer Pool (BufferPoolConfig) {#bufferpoolconfig}

Implementation References:
- `include/scratchbird/engine/buffer_pool.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| buffer_pool.num_buffers | size_t | 1024 | Global | `include/scratchbird/engine/buffer_pool.h` |
| buffer_pool.buffer_size | size_t | 8192 | Global | `include/scratchbird/engine/buffer_pool.h` |
| buffer_pool.dirty_page_threshold | double | 0.7 | Global | `include/scratchbird/engine/buffer_pool.h` |
| buffer_pool.background_write_interval | std::chrono::milliseconds | 100 | Global | `include/scratchbird/engine/buffer_pool.h` |
| buffer_pool.enable_statistics | bool | true | Global | `include/scratchbird/engine/buffer_pool.h` |
| buffer_pool.enable_background_writer | bool | false | Global | `include/scratchbird/engine/buffer_pool.h` |
| buffer_pool.stats_report_interval | std::chrono::seconds | 60 | Global | `include/scratchbird/engine/buffer_pool.h` |
| buffer_pool.use_huge_pages | bool | false | Global | `include/scratchbird/engine/buffer_pool.h` |
| buffer_pool.enable_prefetch | bool | true | Global | `include/scratchbird/engine/buffer_pool.h` |

### Background Writer {#backgroundwriter}

Implementation References:
- `include/scratchbird/engine/background_writer.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| bgwriter.write_interval | std::chrono::milliseconds | 100 | Global | `include/scratchbird/engine/background_writer.h` |
| bgwriter.max_buffers_per_cycle | uint32_t | 100 | Global | `include/scratchbird/engine/background_writer.h` |
| bgwriter.dirty_buffer_threshold | uint32_t | 10 | Global | `include/scratchbird/engine/background_writer.h` |
| bgwriter.max_io_rate | uint64_t | 0 | Global | `include/scratchbird/engine/background_writer.h` |
| bgwriter.enabled | bool | true | Global | `include/scratchbird/engine/background_writer.h` |
| bgwriter.use_sync_writes | bool | false | Global | `include/scratchbird/engine/background_writer.h` |

### Connection Pool (ConnectionPoolConfig) {#connectionpoolconfig}

Implementation References:
- `include/scratchbird/engine/connection_pool.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| pool.min_pool_size | size_t | 5 | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.max_pool_size | size_t | 100 | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.initial_pool_size | size_t | 10 | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.connection_timeout | std::chrono::seconds | 30 | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.idle_timeout | std::chrono::seconds | 300 | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.health_check_interval | std::chrono::seconds | 60 | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.enable_health_monitoring | bool | true | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.max_connection_failures | size_t | 3 | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.connection_queue_size | size_t | 200 | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.use_process_pool | bool | true | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.worker_executable | std::string | "scratchbird_worker" | Global | `include/scratchbird/engine/connection_pool.h` |
| pool.shared_memory_name | std::string | "/scratchbird_pool" | Global | `include/scratchbird/engine/connection_pool.h` |

### Network Buffer (NetworkBufferConfig) {#networkbuffer}

Implementation References:
- `include/scratchbird/engine/network_buffer.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| netbuf.default_recv_buffer_size | size_t | 65536 | Per-connection | `include/scratchbird/engine/network_buffer.h` |
| netbuf.default_send_buffer_size | size_t | 65536 | Per-connection | `include/scratchbird/engine/network_buffer.h` |
| netbuf.min_buffer_size | size_t | 4096 | Per-connection | `include/scratchbird/engine/network_buffer.h` |
| netbuf.max_buffer_size | size_t | 16777216 | Per-connection | `include/scratchbird/engine/network_buffer.h` |
| netbuf.enable_auto_tuning | bool | true | Per-connection | `include/scratchbird/engine/network_buffer.h` |
| netbuf.tuning_interval | std::chrono::seconds | 30 | Per-connection | `include/scratchbird/engine/network_buffer.h` |
| netbuf.utilization_threshold | double | 0.8 | Per-connection | `include/scratchbird/engine/network_buffer.h` |
| netbuf.growth_factor | double | 1.5 | Per-connection | `include/scratchbird/engine/network_buffer.h` |
| netbuf.shrink_factor | double | 0.75 | Per-connection | `include/scratchbird/engine/network_buffer.h` |
| netbuf.enable_monitoring | bool | true | Global | `include/scratchbird/engine/network_buffer.h` |
| netbuf.stats_collection_interval | std::chrono::seconds | 10 | Global | `include/scratchbird/engine/network_buffer.h` |
| netbuf.overflow_alert_threshold | size_t | 5 | Global | `include/scratchbird/engine/network_buffer.h` |
| netbuf.underutilization_threshold | double | 0.2 | Global | `include/scratchbird/engine/network_buffer.h` |

### TCP Optimization (TCPOptimizer::NetworkConfig) {#tcpoptimizer}

Implementation References:
- `include/scratchbird/engine/tcp_optimizer.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| tcp.tcp_nodelay | bool | true | Per-socket | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.tcp_keepalive_idle | std::chrono::seconds | 600 | Per-socket | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.tcp_keepalive_interval | std::chrono::seconds | 30 | Per-socket | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.tcp_keepalive_count | int | 3 | Per-socket | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.tcp_user_timeout | std::chrono::seconds | 0 | Per-socket | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.socket_recv_buffer | size_t | 262144 | Per-socket | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.socket_send_buffer | size_t | 262144 | Per-socket | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.reuse_port | bool | false | Listener | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.enable_fast_open | bool | false | Listener | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.enable_defer_accept | bool | false | Listener | `include/scratchbird/engine/tcp_optimizer.h` |
| tcp.listen_backlog | int | 128 | Listener | `include/scratchbird/engine/tcp_optimizer.h` |

### Network Server (NetworkServerConfig) {#networkserver}

Implementation References:
- `include/scratchbird/engine/network_server.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| server.bind_address | std::string | "127.0.0.1" | Server | `include/scratchbird/engine/network_server.h` |
| server.port | uint16_t | 3050 | Server | `include/scratchbird/engine/network_server.h` |
| server.protocol | std::string | "inet" | Server | `include/scratchbird/engine/network_server.h` |
| server.max_connections | uint32_t | 1000 | Server | `include/scratchbird/engine/network_server.h` |
| server.connection_timeout_seconds | uint32_t | 300 | Server | `include/scratchbird/engine/network_server.h` |
| server.keepalive_interval_seconds | uint32_t | 60 | Server | `include/scratchbird/engine/network_server.h` |
| server.ipv6_enabled | bool | true | Server | `include/scratchbird/engine/network_server.h` |
| server.tcp_nodelay | bool | true | Server | `include/scratchbird/engine/network_server.h` |
| server.listen_backlog | uint32_t | 128 | Server | `include/scratchbird/engine/network_server.h` |
| server.worker_threads | uint32_t | 0 | Server | `include/scratchbird/engine/network_server.h` |
| server.log_level | std::string | "INFO" | Server | `include/scratchbird/engine/network_server.h` |

### Wire Compression (CompressionConfig) {#wirecompression}

Implementation References:
- `include/scratchbird/engine/wire_compression.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| compression.algorithm | enum CompressionAlgorithm | ZLIB | Per-connection | `include/scratchbird/engine/wire_compression.h` |
| compression.level | enum CompressionLevel | BALANCED | Per-connection | `include/scratchbird/engine/wire_compression.h` |
| compression.adaptive_level | bool | true | Per-connection | `include/scratchbird/engine/wire_compression.h` |
| compression.min_compress_size | uint32_t | 64 | Per-connection | `include/scratchbird/engine/wire_compression.h` |
| compression.cpu_threshold | double | 0.8 | Global | `include/scratchbird/engine/wire_compression.h` |
| compression.max_compress_time_ms | double | 5.0 | Global | `include/scratchbird/engine/wire_compression.h` |
| compression.target_compression_ratio | double | 0.7 | Global | `include/scratchbird/engine/wire_compression.h` |
| compression.buffer_size | uint32_t | 65536 | Per-connection | `include/scratchbird/engine/wire_compression.h` |
| compression.max_message_size | uint32_t | 16777216 | Per-connection | `include/scratchbird/engine/wire_compression.h` |
| compression.enable_statistics | bool | true | Global | `include/scratchbird/engine/wire_compression.h` |
| compression.stats_window_size | uint32_t | 1000 | Global | `include/scratchbird/engine/wire_compression.h` |

### WAL (WalConfig) {#wal}

Implementation References:
- `include/scratchbird/engine/wal.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| wal.wal_dir | std::string | (required) | Global | `include/scratchbird/engine/wal.h` |
| wal.segment_size_mb | uint32_t | 64 | Global | `include/scratchbird/engine/wal.h` |
| wal.buffer_size_kb | uint32_t | 1024 | Global | `include/scratchbird/engine/wal.h` |
| wal.fsync_enabled | bool | true | Global | `include/scratchbird/engine/wal.h` |
| wal.checkpoint_interval_s | uint32_t | 300 | Global | `include/scratchbird/engine/wal.h` |
| wal.max_wal_segments | uint32_t | 10 | Global | `include/scratchbird/engine/wal.h` |

### Connection Security (ConnectionSecurityConfig) {#connectionsecurity}

Implementation References:
- `include/scratchbird/engine/connection_security.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| security.encryption_policy | enum EncryptionPolicy | Preferred | Server | `include/scratchbird/engine/connection_security.h` |
| security.force_tls_for_auth | bool | true | Server | `include/scratchbird/engine/connection_security.h` |
| security.require_perfect_forward_secrecy | bool | true | Server | `include/scratchbird/engine/connection_security.h` |
| security.min_security_level | enum SecurityLevel | Standard | Server | `include/scratchbird/engine/connection_security.h` |
| security.allow_weak_ciphers | bool | false | Server | `include/scratchbird/engine/connection_security.h` |
| security.allow_compression | bool | false | Server | `include/scratchbird/engine/connection_security.h` |
| security.validate_client_certificates | bool | true | Server | `include/scratchbird/engine/connection_security.h` |
| security.require_mutual_auth | bool | false | Server | `include/scratchbird/engine/connection_security.h` |
| security.max_connection_age | std::chrono::minutes | 480 | Server | `include/scratchbird/engine/connection_security.h` |
| security.allowed_protocols | std::vector<std::string> | [TLSv1.2, TLSv1.3] | Server | `include/scratchbird/engine/connection_security.h` |
| security.blocked_cipher_suites | std::vector<std::string> | (empty) | Server | `include/scratchbird/engine/connection_security.h` |
| security.required_cipher_suites | std::vector<std::string> | (empty) | Server | `include/scratchbird/engine/connection_security.h` |
| security.allowed_ip_ranges | std::vector<std::string> | (empty) | Server | `include/scratchbird/engine/connection_security.h` |
| security.blocked_ip_ranges | std::vector<std::string> | (empty) | Server | `include/scratchbird/engine/connection_security.h` |
| security.enable_geo_blocking | bool | false | Server | `include/scratchbird/engine/connection_security.h` |
| security.allowed_countries | std::vector<std::string> | (empty) | Server | `include/scratchbird/engine/connection_security.h` |
| security.blocked_countries | std::vector<std::string> | (empty) | Server | `include/scratchbird/engine/connection_security.h` |
| security.rate_limiting.max_connections_per_ip | uint32_t | 10 | Server | `include/scratchbird/engine/connection_security.h` |
| security.rate_limiting.max_attempts_per_minute | uint32_t | 60 | Server | `include/scratchbird/engine/connection_security.h` |
| security.rate_limiting.max_auth_failures | uint32_t | 5 | Server | `include/scratchbird/engine/connection_security.h` |
| security.rate_limiting.ip_block_duration | std::chrono::minutes | 30 | Server | `include/scratchbird/engine/connection_security.h` |
| security.rate_limiting.enable_progressive_delays | bool | true | Server | `include/scratchbird/engine/connection_security.h` |
| security.security_headers | std::map<std::string,std::string> | (empty) | Server | `include/scratchbird/engine/connection_security.h` |
| security.enable_security_audit | bool | true | Server | `include/scratchbird/engine/connection_security.h` |
| security.audit_log_path | std::string | "/var/log/scratchbird/security.log" | Server | `include/scratchbird/engine/connection_security.h` |

### TLS (TLSConfiguration) {#tls}

Implementation References:
- `include/scratchbird/engine/tls_server.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| tls.min_version | enum TLSVersion | TLS_1_2 | Server | `include/scratchbird/engine/tls_server.h` |
| tls.max_version | enum TLSVersion | TLS_1_3 | Server | `include/scratchbird/engine/tls_server.h` |
| tls.certificate_file | std::string | (empty) | Server | `include/scratchbird/engine/tls_server.h` |
| tls.private_key_file | std::string | (empty) | Server | `include/scratchbird/engine/tls_server.h` |
| tls.ca_certificate_file | std::string | (empty) | Server | `include/scratchbird/engine/tls_server.h` |
| tls.crl_file | std::string | (empty) | Server | `include/scratchbird/engine/tls_server.h` |
| tls.client_verification | enum TLSVerificationMode | None | Server | `include/scratchbird/engine/tls_server.h` |
| tls.require_client_certificate | bool | false | Server | `include/scratchbird/engine/tls_server.h` |
| tls.allowed_cipher_suites | std::vector<std::string> | (empty) | Server | `include/scratchbird/engine/tls_server.h` |
| tls.allowed_curves | std::vector<std::string> | (empty) | Server | `include/scratchbird/engine/tls_server.h` |
| tls.disable_compression | bool | true | Server | `include/scratchbird/engine/tls_server.h` |
| tls.enable_session_tickets | bool | false | Server | `include/scratchbird/engine/tls_server.h` |
| tls.require_perfect_forward_secrecy | bool | true | Server | `include/scratchbird/engine/tls_server.h` |
| tls.enable_ocsp_stapling | bool | false | Server | `include/scratchbird/engine/tls_server.h` |
| tls.ocsp_responder_url | std::string | (empty) | Server | `include/scratchbird/engine/tls_server.h` |
| tls.session_timeout | std::chrono::minutes | 60 | Server | `include/scratchbird/engine/tls_server.h` |
| tls.enable_session_resumption | bool | true | Server | `include/scratchbird/engine/tls_server.h` |
| tls.enforce_server_cipher_order | bool | true | Server | `include/scratchbird/engine/tls_server.h` |
| tls.disable_renegotiation | bool | true | Server | `include/scratchbird/engine/tls_server.h` |

### Provider/Y-Valve (ProviderConfig, FailoverConfig) {#provider}

Implementation References:
- `include/scratchbird/engine/database_provider.h`
- `include/scratchbird/engine/provider_dispatch.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| provider.provider_name | std::string | (empty) | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.provider_version | std::string | "1.0.0" | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.provider_type | enum ProviderType | (none) | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.capabilities.* | struct | varies | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.connection_pool_size | uint32_t | 100 | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.max_concurrent_transactions | uint32_t | 1000 | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.statement_cache_size | uint32_t | 1000 | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.timeout_ms | uint32_t | 30000 | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.max_memory_mb | uint64_t | 1024 | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.max_open_databases | uint32_t | 50 | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.max_prepared_statements | uint32_t | 10000 | Provider | `include/scratchbird/engine/database_provider.h` |
| provider.custom_options | std::map<std::string,std::string> | (empty) | Provider | `include/scratchbird/engine/database_provider.h` |
| failover.enabled | bool | false | Dispatcher | `include/scratchbird/engine/provider_dispatch.h` |
| failover.max_retries | uint32_t | 3 | Dispatcher | `include/scratchbird/engine/provider_dispatch.h` |
| failover.retry_delay_ms | uint32_t | 1000 | Dispatcher | `include/scratchbird/engine/provider_dispatch.h` |
| failover.health_check_interval_ms | uint32_t | 30000 | Dispatcher | `include/scratchbird/engine/provider_dispatch.h` |
| failover.auto_recovery | bool | true | Dispatcher | `include/scratchbird/engine/provider_dispatch.h` |

### Statement/Plan Caches {#caches}

Implementation References:
- `include/scratchbird/engine/prepared_statement_cache.h`
- `include/scratchbird/engine/plan_cache.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| psc.max_statements | uint32_t | 500 | Global | `include/scratchbird/engine/prepared_statement_cache.h` |
| psc.max_memory_bytes | uint64_t | 33554432 | Global | `include/scratchbird/engine/prepared_statement_cache.h` |
| psc.entry_ttl_seconds | uint32_t | 1800 | Global | `include/scratchbird/engine/prepared_statement_cache.h` |
| psc.eviction_threshold | double | 0.75 | Global | `include/scratchbird/engine/prepared_statement_cache.h` |
| psc.enabled | bool | true | Global | `include/scratchbird/engine/prepared_statement_cache.h` |
| psc.enable_metadata_cache | bool | true | Global | `include/scratchbird/engine/prepared_statement_cache.h` |
| psc.cache_threshold | uint32_t | 2 | Global | `include/scratchbird/engine/prepared_statement_cache.h` |
| psc.cleanup_interval_seconds | uint32_t | 180 | Global | `include/scratchbird/engine/prepared_statement_cache.h` |
| psc.enable_statistics | bool | true | Global | `include/scratchbird/engine/prepared_statement_cache.h` |
| plan.max_plans | uint32_t | 1000 | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.max_memory_bytes | uint64_t | 67108864 | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.entry_ttl_seconds | uint32_t | 3600 | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.eviction_threshold | double | 0.8 | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.enabled | bool | true | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.enable_generic_plans | bool | true | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.enable_specific_plans | bool | true | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.specific_plan_threshold | uint32_t | 5 | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.enable_cache_warming | bool | true | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.cleanup_interval_seconds | uint32_t | 300 | Global | `include/scratchbird/engine/plan_cache.h` |
| plan.enable_statistics | bool | true | Global | `include/scratchbird/engine/plan_cache.h` |

### Partitioned Lock Manager (PartitionedLockManagerConfig) {#plm}

Implementation References:
- `include/scratchbird/engine/partitioned_lock_manager.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| plm.partition_count | uint32_t | 16 | Global | `include/scratchbird/engine/partitioned_lock_manager.h` |
| plm.max_locks_per_partition | uint32_t | 10000 | Global | `include/scratchbird/engine/partitioned_lock_manager.h` |
| plm.deadlock_detection_enabled | bool | true | Global | `include/scratchbird/engine/partitioned_lock_manager.h` |
| plm.deadlock_check_interval_ms | uint32_t | 1000 | Global | `include/scratchbird/engine/partitioned_lock_manager.h` |
| plm.max_wait_time_ms | uint32_t | 30000 | Global | `include/scratchbird/engine/partitioned_lock_manager.h` |
| plm.collect_statistics | bool | true | Global | `include/scratchbird/engine/partitioned_lock_manager.h` |
| plm.cross_partition_deadlock_detection | bool | true | Global | `include/scratchbird/engine/partitioned_lock_manager.h` |
| plm.initial_table_size | uint32_t | 1024 | Global | `include/scratchbird/engine/partitioned_lock_manager.h` |
| plm.load_factor_threshold | double | 0.75 | Global | `include/scratchbird/engine/partitioned_lock_manager.h` |

### Batch Operations (BatchConfig) {#batch}

Implementation References:
- `include/scratchbird/engine/batch_operations.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| batch.max_rows_per_batch | uint32_t | 1000 | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.max_batch_memory_mb | uint32_t | 64 | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.batch_timeout | std::chrono::milliseconds | 100 | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.execution_mode | enum BatchExecutionMode | ADAPTIVE | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.min_batch_size | uint32_t | 10 | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.enable_parallel_execution | bool | true | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.continue_on_error | bool | false | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.max_retry_attempts | uint32_t | 3 | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.enable_statistics | bool | true | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.stats_window_size | uint32_t | 1000 | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.enable_network_batching | bool | true | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.max_network_batch_size | uint32_t | 8192 | Global | `include/scratchbird/engine/batch_operations.h` |
| batch.network_batch_timeout | std::chrono::milliseconds | 50 | Global | `include/scratchbird/engine/batch_operations.h` |

### Index Family (IndexFamilyConfig) {#indexfamily}

Implementation References:
- `include/scratchbird/engine/index_family.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| index.method | enum IndexMethod | BTree | Per-index | `include/scratchbird/engine/index_family.h` |
| index.hash.initial_buckets | uint32_t | 1024 | Per-index | `include/scratchbird/engine/index_family.h` |
| index.hash.load_factor | double | 0.75 | Per-index | `include/scratchbird/engine/index_family.h` |
| index.hash.extensible_hashing | bool | true | Per-index | `include/scratchbird/engine/index_family.h` |
| index.bitmap.compression_threshold | uint32_t | 1000 | Per-index | `include/scratchbird/engine/index_family.h` |
| index.bitmap.use_rle_compression | bool | true | Per-index | `include/scratchbird/engine/index_family.h` |
| index.bitmap.use_wah_compression | bool | false | Per-index | `include/scratchbird/engine/index_family.h` |
| index.gin.posting_list_threshold | uint32_t | 100 | Per-index | `include/scratchbird/engine/index_family.h` |
| index.gin.compress_posting_lists | bool | true | Per-index | `include/scratchbird/engine/index_family.h` |
| index.gin.tokenizer | std::string | "simple" | Per-index | `include/scratchbird/engine/index_family.h` |
| index.rtree.max_entries_per_node | uint32_t | 50 | Per-index | `include/scratchbird/engine/index_family.h` |
| index.rtree.min_entries_per_node | uint32_t | 20 | Per-index | `include/scratchbird/engine/index_family.h` |
| index.rtree.split_strategy | double | 0.4 | Per-index | `include/scratchbird/engine/index_family.h` |

### Listener, Logging, Tracing {#misc}

Implementation References:
- `include/scratchbird/listener.h`
- `include/scratchbird/telemetry/logging.h`
- `include/scratchbird/trace/trace.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| listener.bind_address | std::string | (empty) | Server | `include/scratchbird/listener.h` |
| listener.port | uint16_t | 0 | Server | `include/scratchbird/listener.h` |
| logging.level | enum LogLevel | Info | Global | `include/scratchbird/telemetry/logging.h` |
| logging.json_format | bool | false | Global | `include/scratchbird/telemetry/logging.h` |
| trace.profile.name | std::string | (empty) | Global | `include/scratchbird/trace/trace.h` |
| trace.profile.sample_ratio | double | 0.0 | Global | `include/scratchbird/trace/trace.h` |
| trace.profile.buffer_capacity | size_t | 16384 | Global | `include/scratchbird/trace/trace.h` |

### Database Links (DatabaseLinkConfig) {#dblinks}

Implementation References:
- `include/scratchbird/engine/database_link.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| dblink.link_name | std::string | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.target_host | std::string | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.target_port | uint16_t | 0 | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.target_database | std::string | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.fdw_name | std::string | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.username | std::string | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.password | std::string | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.use_ssl | bool | false | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.ssl_cert_path | std::string | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.ssl_key_path | std::string | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.ssl_ca_path | std::string | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |
| dblink.options.* | std::unordered_map<std::string,std::string> | (empty) | Per-link | `include/scratchbird/engine/database_link.h` |

### Remote Provider Network (Remote NetworkConfig) {#remote-network}

Implementation References:
- `include/scratchbird/engine/remote_provider.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| remote.hostname | std::string | "localhost" | Remote | `include/scratchbird/engine/remote_provider.h` |
| remote.port | uint16_t | 3050 | Remote | `include/scratchbird/engine/remote_provider.h` |
| remote.connect_timeout_ms | uint32_t | 30000 | Remote | `include/scratchbird/engine/remote_provider.h` |
| remote.read_timeout_ms | uint32_t | 60000 | Remote | `include/scratchbird/engine/remote_provider.h` |
| remote.write_timeout_ms | uint32_t | 30000 | Remote | `include/scratchbird/engine/remote_provider.h` |
| remote.keepalive_interval_ms | uint32_t | 60000 | Remote | `include/scratchbird/engine/remote_provider.h` |
| remote.enable_compression | bool | false | Remote | `include/scratchbird/engine/remote_provider.h` |
| remote.enable_encryption | bool | false | Remote | `include/scratchbird/engine/remote_provider.h` |
| remote.max_packet_size | uint32_t | 32768 | Remote | `include/scratchbird/engine/remote_provider.h` |
| remote.buffer_size | uint32_t | 8192 | Remote | `include/scratchbird/engine/remote_provider.h` |

### Authentication: Trusted/LDAP and Two-Factor {#auth}

Implementation References:
- `include/scratchbird/engine/trusted_auth.h`
- `include/scratchbird/engine/two_factor_auth.h`

LDAPAuthenticator::LDAPConfig:

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| ldap.server_url | std::string | (empty) | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.base_dn | std::string | (empty) | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.bind_dn | std::string | (empty) | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.bind_password | std::string | (empty) | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.user_search_base | std::string | (empty) | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.user_search_filter | std::string | "(uid={username})" | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.group_search_base | std::string | (empty) | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.group_search_filter | std::string | "(member={user_dn})" | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.use_tls | bool | true | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.verify_certificate | bool | true | Auth | `include/scratchbird/engine/trusted_auth.h` |
| ldap.timeout_seconds | uint32_t | 30 | Auth | `include/scratchbird/engine/trusted_auth.h` |

Two-Factor configs:

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| totp.time_step | uint32_t | 30 | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| totp.digits | uint32_t | 6 | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| totp.algorithm | std::string | "SHA1" | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| totp.issuer | std::string | "ScratchBird" | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| message.template_text | std::string | "Your verification code is: {code}" | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| message.code_length | uint32_t | 6 | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| message.expiry | std::chrono::minutes | 5 | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| message.max_attempts | uint32_t | 3 | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| hardware.challenge_type | std::string | "webauthn" | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| hardware.timeout | std::chrono::minutes | 2 | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| backup.code_count | uint32_t | 10 | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| backup.code_length | uint32_t | 8 | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| backup.single_use | bool | true | Auth | `include/scratchbird/engine/two_factor_auth.h` |
| backup.expiry | std::chrono::hours | 8760 | Auth | `include/scratchbird/engine/two_factor_auth.h` |

### Storage {#storage}

Implementation References:
- `include/scratchbird/engine/storage.h`

| Name | Type | Default | Scope | Source |
| --- | --- | --- | --- | --- |
| storage.path | std::string | (empty) | Global | `include/scratchbird/engine/storage.h` |

