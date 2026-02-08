# Administration Specifications

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**[← Back to Specifications Index](../README.md)**

This directory contains administration tool specifications for ScratchBird database management.

## Overview

ScratchBird provides comprehensive administration tools including a command-line interface (CLI) for database management, monitoring, and operations.

## Specifications in this Directory

- **[SB_ADMIN_CLI_SPECIFICATION.md](SB_ADMIN_CLI_SPECIFICATION.md)** (608 lines) - ScratchBird Admin CLI specification
- **[SB_SERVER_NETWORK_CLI_SPECIFICATION.md](SB_SERVER_NETWORK_CLI_SPECIFICATION.md)** - sb_server network CLI options

## Key Features

### sb-admin CLI

The `sb-admin` CLI provides administration commands:

- **Database Management** - Create, drop, backup, restore databases
- **User Management** - Create, modify, delete users and roles
- **Monitoring** - Query statistics, performance metrics
- **Configuration** - View and modify server configuration
- **Maintenance** - Vacuum, analyze, reindex operations

### Example Commands

```bash
# Database operations
sb-admin create-database mydb
sb-admin backup-database mydb -o backup.sbdb
sb-admin restore-database mydb -i backup.sbdb

# User management
sb-admin create-user alice --role admin
sb-admin grant-privileges alice --database mydb --privileges ALL

# Monitoring
sb-admin show-stats --database mydb
sb-admin show-connections

# Maintenance
sb-admin sweep --database mydb --analyze    # native sweep/GC (VACUUM alias)
sb-admin reindex --database mydb --table users
```

## Related Specifications

- [Deployment](../deployment/) - System deployment and systemd integration
- [Operations](../operations/) - Operational monitoring and metrics
- [Security](../Security Design Specification/) - Authentication and authorization
- [Catalog](../catalog/) - System catalog queries

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
