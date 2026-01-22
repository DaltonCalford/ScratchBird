# Frequently Asked Questions

Common questions about ScratchBird.

[Back to Documentation Index](../index.md)

---

## General

### What is ScratchBird?

ScratchBird is a modern, multi-protocol database engine that supports PostgreSQL, MySQL, and Firebird wire protocols simultaneously. It allows you to use existing tools and drivers from any of these ecosystems.

### What makes ScratchBird different?

- **Multi-protocol support**: Connect with PostgreSQL, MySQL, or Firebird clients
- **Single engine**: All protocols access the same database
- **Modern C++**: Built with C++17 for performance and safety
- **Embedded mode**: Run within your application without a server

### Is ScratchBird production-ready?

ScratchBird 0.9.0 is a beta release. It's suitable for development and testing, but should be evaluated thoroughly before production use. We recommend extensive testing with your specific workload.

### What license is ScratchBird under?

ScratchBird is licensed under the Initial Developer's Public License (IDPL) 1.0, similar to the Firebird database.

---

## Installation

### What are the system requirements?

- **OS**: Linux (Debian 10+, Ubuntu 20.04+, RHEL 8+), Windows 10+
- **CPU**: x86-64 architecture
- **RAM**: 512 MB minimum, 4 GB+ recommended
- **Disk**: 100 MB for installation, plus data storage

See [System Requirements](../installation/system-requirements.md) for details.

### Can I run ScratchBird on ARM?

ARM support is planned but not yet available in this release.

### How do I install on Docker?

```bash
docker run -d -p 5432:5432 -e SCRATCHBIRD_ADMIN_PASSWORD=secret scratchbird:latest
```

See [Docker Installation](../installation/docker.md) for full details.

### How do I upgrade ScratchBird?

1. Stop the server
2. Backup your data
3. Install the new version
4. Start the server

Package managers handle most upgrades automatically.

---

## Connectivity

### Which port should I use?

| Use Case | Port |
|----------|------|
| PostgreSQL tools/drivers | 5432 |
| MySQL tools/drivers | 3306 |
| Firebird tools/drivers | 3050 |
| ScratchBird native | 3092 |

### Can I use psql/mysql/isql?

Yes! ScratchBird accepts connections from:
- `psql` (PostgreSQL client)
- `mysql` (MySQL client)
- Firebird `isql`
- `sb_isql` (native)

### Do existing applications work without changes?

Most applications work by just changing the connection string. The protocol compatibility handles the rest.

### Why use 127.0.0.1 instead of localhost with MySQL?

The MySQL client uses Unix socket for "localhost" by default. Use `127.0.0.1` to force TCP connection to ScratchBird.

---

## SQL Compatibility

### Which SQL dialect does ScratchBird use?

ScratchBird uses standard SQL with PostgreSQL-style extensions. When connected via MySQL or Firebird protocols, it also accepts their specific syntax.

### Are stored procedures supported?

Yes, ScratchBird supports stored procedures and functions using PL/pgSQL-compatible syntax.

### Are triggers supported?

Yes, triggers are fully supported.

### What about extensions?

PostgreSQL extensions are not directly supported. Many common extension features are built-in:
- `uuid-ossp` → Use `gen_random_uuid()`
- `pg_trgm` → Use LIKE with indexes
- JSON functions are native

---

## Performance

### How does performance compare to PostgreSQL?

Performance is competitive with PostgreSQL for most workloads. Specific benchmarks depend on your use case.

### How much RAM should I allocate?

General guidelines:
- `buffer_pool_size`: 25% of RAM, up to 8 GB
- Leave room for OS cache and work_mem

### Why are queries slow?

Common causes:
1. Missing indexes - Use `EXPLAIN ANALYZE`
2. Outdated statistics - Run `ANALYZE`
3. Too little memory - Increase `work_mem`

See [Performance Tuning](../admin/performance-tuning.md).

---

## Administration

### How do I create a backup?

```bash
sb_backup create mydb /backup/mydb.sbdb
```

Or use `pg_dump` via PostgreSQL protocol.

### How do I restore from backup?

```bash
sb_backup restore /backup/mydb.sbdb mydb_restored
```

### How do I add a user?

```sql
CREATE USER newuser WITH PASSWORD 'secure_password';
GRANT CONNECT ON DATABASE mydb TO newuser;
```

### How do I reset the admin password?

```bash
sb_security password admin --new-password
```

---

## Troubleshooting

### Server won't start

1. Check logs: `journalctl -u scratchbird`
2. Validate config: `sb_server --check`
3. Check port conflicts: `ss -tlnp | grep 5432`

### "Connection refused"

- Is the server running?
- Is the port correct?
- Is the firewall blocking?

### "Authentication failed"

- Verify username/password
- Check hba.conf rules
- Verify user exists

### Where are the log files?

Default: `/var/log/scratchbird/sb_server.log`

Or via systemd: `journalctl -u scratchbird`

---

## Features

### Does ScratchBird support replication?

Basic replication is planned for a future release.

### Does ScratchBird support partitioning?

Table partitioning is supported.

### What index types are available?

- BTREE (default)
- HASH
- GIN (full-text, JSON, arrays)
- GIST (spatial)
- BRIN (large tables)
- And more (11 types total)

### Is full-text search supported?

Yes, using GIN indexes and text search functions.

### Is JSON supported?

Yes, JSON and JSONB types with full query support.

---

## Migration

### How do I migrate from PostgreSQL?

```bash
pg_dump -h old-server -U admin mydb > dump.sql
psql -h scratchbird-server -U admin -d mydb < dump.sql
```

See [Migration from PostgreSQL](../getting-started/tutorials/migration-from-postgres.md).

### How do I migrate from MySQL?

1. Export with mysqldump
2. Convert schema (usually minimal changes)
3. Import via MySQL protocol

### How do I migrate from Firebird?

1. Export with gbak or isql
2. Review schema compatibility
3. Import via Firebird protocol

---

## Security

### Is SSL/TLS supported?

Yes. Enable in configuration:

```ini
[ssl]
enabled = true
cert_file = /path/to/server.crt
key_file = /path/to/server.key
```

### What authentication methods are supported?

- SCRAM-SHA-256 (recommended)
- SCRAM-SHA-512
- MD5 (legacy)
- Trust (development only)
- Certificate

### How do I secure my installation?

1. Use strong passwords
2. Enable SSL/TLS
3. Configure hba.conf restrictively
4. Use firewall rules
5. Keep software updated

See [Security Guide](../admin/security.md).

---

## Getting Help

### Where do I report bugs?

[GitHub Issues](https://github.com/daltoncs/scratchbird/issues)

### Where can I get support?

- This documentation
- GitHub Issues
- Community forums (coming soon)

### How do I contribute?

See the CONTRIBUTING.md file in the repository.

---

## See Also

- [Glossary](../glossary.md)
- [Troubleshooting](../admin/troubleshooting.md)
- [Getting Started](../getting-started/index.md)
