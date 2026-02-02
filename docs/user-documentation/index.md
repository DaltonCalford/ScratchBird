# ScratchBird Database Engine - User Documentation

**Version:** 0.9.0-beta0
**Status:** Beta Preview
**Last Updated:** December 2025

---

## Welcome to ScratchBird

ScratchBird is a **Universal Database Engine** built from scratch using Multi-Generational Architecture (MGA). It provides a unique capability: connect using the protocol of your choice (PostgreSQL, MySQL, Firebird, or Native) while all queries compile to the same internal bytecode (SBLR).

### Key Features

- **Multi-Protocol Support**: Connect using `psql`, `mysql`, FlameRobin, or native clients
- **MGA Concurrency**: Readers never block writers, writers never block readers
- **11 Index Types**: B-Tree, Hash, GiST, GIN, BRIN, Bitmap, R-Tree, SP-GiST, Bloom, Partial, Expression
- **86 Data Types**: Full SQL standard plus JSON, UUID, Network types, and more
- **Enterprise Security**: SCRAM-SHA-256/512, LDAP, Kerberos, OAuth 2.0, SAML 2.0, MFA

---

## Documentation Sections

### Installation

Get ScratchBird running on your system.

| Guide | Description |
|-------|-------------|
| [System Requirements](installation/system-requirements.md) | Hardware and software prerequisites |
| [Linux (DEB)](installation/linux-deb.md) | Debian/Ubuntu installation |
| [Linux (RPM)](installation/linux-rpm.md) | RHEL/Fedora installation |
| [Linux (Tarball)](installation/linux-tarball.md) | Generic Linux installation |
| [Windows (Installer)](installation/windows-installer.md) | Windows NSIS installer |
| [Windows (Portable)](installation/windows-portable.md) | Windows ZIP portable |
| [Docker](installation/docker.md) | Container deployment |
| [Building from Source](installation/building-from-source.md) | Compile from source code |

### Getting Started

Your first steps with ScratchBird.

| Guide | Description |
|-------|-------------|
| [Overview](getting-started/index.md) | Getting started introduction |
| [First Database](getting-started/first-database.md) | Create your first database |
| [First Connection](getting-started/first-connection.md) | Connect with various clients |
| [Basic SQL](getting-started/basic-sql.md) | Essential SQL operations |

**Tutorials:**
- [Web Application Backend](getting-started/tutorials/web-app-backend.md)
- [Data Warehouse Setup](getting-started/tutorials/data-warehouse.md)
- [Migrating from PostgreSQL](getting-started/tutorials/migration-from-postgres.md)

### Configuration

Configure ScratchBird for your environment.

| Guide | Description |
|-------|-------------|
| [Overview](configuration/index.md) | Configuration introduction |
| [sb_server.conf Reference](configuration/sb_server.conf.md) | Main server configuration |
| [Host-Based Authentication](configuration/hba.conf.md) | Connection security rules |
| [SSL/TLS Setup](configuration/ssl-setup.md) | Secure connections |
| [Environment Variables](configuration/environment-vars.md) | Runtime configuration |

### Administration

Manage and maintain your ScratchBird installation.

| Guide | Description |
|-------|-------------|
| [Overview](admin/index.md) | Administration introduction |
| [User Management](admin/user-management.md) | Users, roles, and permissions |
| [Backup and Restore](admin/backup-restore.md) | Data protection |
| [Monitoring](admin/monitoring.md) | Metrics and logging |
| [Security Best Practices](admin/security.md) | Hardening your installation |
| [Performance Tuning](admin/performance-tuning.md) | Optimization guide |
| [Troubleshooting](admin/troubleshooting.md) | Common issues and solutions |

### SQL Language Guide

Complete SQL reference for ScratchBird.

| Section | Description |
|---------|-------------|
| [Overview](language-guide/index.md) | SQL language introduction |
| [DDL Reference](language-guide/ddl/index.md) | Data Definition Language |
| [DML Reference](language-guide/dml/index.md) | Data Manipulation Language |
| [Procedural SQL](language-guide/psql/index.md) | Stored procedures and functions |
| [Built-in Functions](language-guide/functions/index.md) | Function reference |
| [Data Types](language-guide/data-types/index.md) | Type reference |

### Command-Line Tools

Reference for ScratchBird command-line utilities.

| Tool | Description |
|------|-------------|
| [Overview](tools/index.md) | Tools introduction |
| [sb_server](tools/sb-server.md) | Database server daemon |
| sb_isql | Interactive SQL shell (see ScratchBird-driver docs) |
| [sb_admin](tools/sb-admin.md) | Administration CLI |
| [sb_verify](tools/sb-verify.md) | Database verification |
| [sb_backup](tools/sb-backup.md) | Backup and restore |
| [sb_security](tools/sb-security.md) | Security management |

### Connectivity

Connect to ScratchBird from various clients and applications.

| Guide | Description |
|-------|-------------|
| [Overview](connectivity/index.md) | Connection options |
| [PostgreSQL Clients](connectivity/postgresql-clients.md) | psql, pgAdmin, DBeaver |
| [MySQL Clients](connectivity/mysql-clients.md) | mysql, MySQL Workbench |
| [Firebird Clients](connectivity/firebird-clients.md) | FlameRobin, IBExpert |
| ODBC Driver | ODBC connectivity (see ScratchBird-driver docs) |
| JDBC Driver | Java connectivity (see ScratchBird-driver docs) |
| [Native Client](connectivity/native-client.md) | ScratchBird client library |

### Reference

| Document | Description |
|----------|-------------|
| [FAQ](faq/index.md) | Frequently asked questions |
| [Glossary](glossary.md) | Terms and definitions |

---

## Protocol Ports

ScratchBird listens on multiple ports for different protocols:

| Protocol | Default Port | Client Examples |
|----------|-------------|-----------------|
| ScratchBird Native | 3092 | libscratchbird_client |
| PostgreSQL | 5432 | psql, pgAdmin, DBeaver |
| MySQL | 3306 | mysql, MySQL Workbench |
| Firebird | 3050 | FlameRobin, IBExpert |

---

## Quick Links

- **Report Issues:** [GitHub Issues](https://github.com/DaltonCalford/ScratchBird/issues)
- **Source Code:** [GitHub Repository](https://github.com/DaltonCalford/ScratchBird)
- **License:** IDPL 1.0 (Firebird-derived)

---

## Beta Warning

**This is beta software.** Do not use for production data. Data formats may change between beta versions. See the [FAQ](faq/index.md) for more information about the beta program.
