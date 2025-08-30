## Configuration Examples

These examples demonstrate minimal and extended configurations derived from defaults in the codebase. Adjust paths and values for your environment.

### Minimal Engine and WAL

```ini
# engine.conf
default_page_size = 4096
fsync_policy = always
checksum_policy = crc32c
tablespaces_enabled = true

[wal]
wal_dir = /var/lib/scratchbird/wal
segment_size_mb = 64
buffer_size_kb = 1024
checkpoint_interval_s = 300
max_wal_segments = 10
```

### Network Server with TLS and Security

```ini
[server]
bind_address = 0.0.0.0
port = 3050
max_connections = 1000
keepalive_interval_seconds = 60
tcp_nodelay = true

[tls]
certificate_file = /etc/scratchbird/cert.pem
private_key_file = /etc/scratchbird/key.pem
client_verification = Required
min_version = TLS_1_2
max_version = TLS_1_3

[security]
encryption_policy = Required
require_perfect_forward_secrecy = true
validate_client_certificates = true
allowed_protocols = TLSv1.2,TLSv1.3
rate_limiting.max_connections_per_ip = 20
rate_limiting.max_attempts_per_minute = 120
rate_limiting.max_auth_failures = 5
rate_limiting.ip_block_duration = 30m
```

### Performance Manager

```ini
[performance]
enable_performance_monitoring = true
metrics_collection_interval = 10s
enable_performance_alerts = true
alert_check_interval = 30s
enable_auto_tuning = true
auto_tuning_interval = 15m
auto_tuning_aggressiveness = 0.5

[performance.alert_thresholds]
cpu_usage_warning_percent = 80.0
cpu_usage_critical_percent = 95.0
buffer_hit_ratio_warning = 70.0
buffer_hit_ratio_critical = 50.0
avg_query_time_warning_ms = 1000.0
avg_query_time_critical_ms = 5000.0
```

### Caches

```ini
[prepared_statement_cache]
max_statements = 500
entry_ttl_seconds = 1800
eviction_threshold = 0.75
enabled = true

[plan_cache]
max_plans = 1000
entry_ttl_seconds = 3600
eviction_threshold = 0.8
enabled = true
```

### Database Link

```ini
[dblink.my_pg]
target_host = db.example.com
target_port = 5432
target_database = app
fdw_name = postgresql_fdw
username = app_user
password = ${PG_PASSWORD}
use_ssl = true
ssl_ca_path = /etc/ssl/certs/ca-bundle.crt
```

### Remote Provider Network

```ini
[remote]
hostname = db.example.com
port = 3050
connect_timeout_ms = 30000
read_timeout_ms = 60000
enable_compression = true
max_packet_size = 32768
```

### Two-Factor (TOTP + Message)

```ini
[auth.totp]
digits = 6
time_step = 30
issuer = ScratchBird

[auth.message]
template_text = Your verification code is: {code}
code_length = 6
expiry = 5m
```

