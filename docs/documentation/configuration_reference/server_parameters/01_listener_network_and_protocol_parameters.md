<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# Listener and Network Parameters

[Server Parameters README](../README.md) | [Configuration Reference README](../../README.md)

## Coverage and Evidence Status

Status: Complete

## Network Parameters

### listen_addresses

Specifies TCP/IP address(es) to listen on.

| Attribute | Value |
|-----------|-------|
| Type | String |
| Default | `localhost` |
| Scope | Server |

```ini
listen_addresses = 'localhost'           # Local only
listen_addresses = '*'                   # All interfaces
listen_addresses = '192.168.1.1,::1'     # Specific addresses
```

### port

Server TCP port.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `3092` (native SB) |
| Range | 1-65535 |

```ini
port = 3092
```

### max_connections

Maximum concurrent connections.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `100` |
| Range | 1-10000 |

```ini
max_connections = 200
```

## Protocol Parameters

### native_protocol.port

Native SBWP protocol port.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `3092` |

```ini
native_protocol.port = 3092
```

### emulation.postgresql.port

PostgreSQL emulation port.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `5432` |

```ini
emulation.postgresql.port = 5432
```

### emulation.mysql.port

MySQL emulation port.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `3306` |

```ini
emulation.mysql.port = 3306
```

### emulation.firebird.port

Firebird emulation port.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `3050` |

```ini
emulation.firebird.port = 3050
```

## SSL/TLS Parameters

### ssl

Enable SSL connections.

| Attribute | Value |
|-----------|-------|
| Type | Boolean |
| Default | `on` |

```ini
ssl = on
```

### ssl_cert_file

Server SSL certificate.

| Attribute | Value |
|-----------|-------|
| Type | String |
| Default | `server.crt` |

```ini
ssl_cert_file = '/etc/scratchbird/server.crt'
```

### ssl_key_file

Server SSL private key.

| Attribute | Value |
|-----------|-------|
| Type | String |
| Default | `server.key` |

```ini
ssl_key_file = '/etc/scratchbird/server.key'
```

### ssl_ca_file

CA certificate for client verification.

| Attribute | Value |
|-----------|-------|
| Type | String |
| Default | (none) |

```ini
ssl_ca_file = '/etc/scratchbird/ca.crt'
```

## Connection Parameters

### tcp_keepalives_idle

TCP keepalive idle time.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `0` (system default) |
| Unit | Seconds |

```ini
tcp_keepalives_idle = 600
```

### tcp_keepalives_interval

TCP keepalive interval.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `0` |

```ini
tcp_keepalives_interval = 30
```

### tcp_keepalives_count

TCP keepalive probes.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `0` |

```ini
tcp_keepalives_count = 3
```

## Timeout Parameters

### statement_timeout

Maximum statement execution time.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `0` (disabled) |
| Unit | Milliseconds |

```ini
statement_timeout = 30000  # 30 seconds
```

### lock_timeout

Maximum wait for locks.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `0` |
| Unit | Milliseconds |

```ini
lock_timeout = 10000  # 10 seconds
```

### idle_in_transaction_session_timeout

Disconnect idle transactions.

| Attribute | Value |
|-----------|-------|
| Type | Integer |
| Default | `0` |
| Unit | Milliseconds |

```ini
idle_in_transaction_session_timeout = 60000  # 1 minute
```

## Examples

### Basic Network Configuration

```ini
# Listen on all interfaces, port 3092
listen_addresses = '*'
port = 3092
max_connections = 200
```

### Secure Configuration

```ini
# SSL required
ssl = on
ssl_cert_file = '/secure/server.crt'
ssl_key_file = '/secure/server.key'
ssl_ca_file = '/secure/ca.crt'
ssl_crl_file = '/secure/ca.crl'

# Protocols
native_protocol.port = 3092
emulation.postgresql.port = 5432

# Timeouts
statement_timeout = 60000
idle_in_transaction_session_timeout = 300000
```

## See Also

- [Security and Transport Parameters](../security_and_transport_configuration/README.md)
- [Session Parameters](../session_parameters/README.md)
