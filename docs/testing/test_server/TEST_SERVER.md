# ScratchBird Test Server

Quick reference for the ScratchBird Test Server used for driver development, GUI testing, and security validation.

## Quick Start

```bash
# Setup (one-time)
./scripts/test-server-user.sh setup

# Start server
./scripts/test-server-user.sh start

# Check status
./scripts/test-server-user.sh status

# View logs
./scripts/test-server-user.sh logs

# Stop server
./scripts/test-server-user.sh stop
```

## Connection Information

| Protocol | Host | Port | Auth Mode |
|----------|------|------|-----------|
| Native | 127.0.0.1 | 3092 | Bootstrap / SCRAM-SHA-256 |
| PostgreSQL | 127.0.0.1 | 5432 | Bootstrap / SCRAM-SHA-256 |
| Firebird | 127.0.0.1 | 3050 | Bootstrap / SCRAM-SHA-256 |

## Connection Strings

**Bootstrap Mode (any user/pass):**
```
scratchbird://anyuser:anypass@127.0.0.1:3092/testdb
postgresql://anyuser:anypass@127.0.0.1:5432/testdb
firebird://anyuser:anypass@127.0.0.1:3050/testdb
```

## Documentation

| Document | Description |
|----------|-------------|
| **[Full Specification](docs/specifications/testing/test_server/README.md)** | Complete test server documentation |
| **[Operations Guide](docs/specifications/testing/test_server/OPERATIONS.md)** | Startup, shutdown, maintenance |
| **[Security Testing](docs/specifications/testing/test_server/SECURITY_TESTING.md)** | Security compliance testing |

## File Locations

```
~/.scratchbird/testdb/
├── testdb.sdb              # Database file
├── test-server.conf        # Server configuration
├── scratchbird_hba.conf    # HBA rules
└── logs/
    └── server.stdout       # Server logs
```

## Testing Checklist

- [ ] Connect with bootstrap credentials
- [ ] Create real users via SQL
- [ ] Test SCRAM-SHA-256 authentication
- [ ] Verify HBA rule enforcement
- [ ] Test rate limiting
- [ ] Validate TLS connections (if enabled)

---

**See Also:**
- [Test Server Specification](docs/specifications/testing/test_server/README.md)
- [Testing Specifications Index](docs/specifications/testing/README.md)
