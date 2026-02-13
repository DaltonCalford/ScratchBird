# Database Tools Specifications

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**[← Back to Specifications Index](../README.md)**

This directory contains specifications for database tool integrations.

## Overview

This directory is reserved for specifications related to database management tools, IDE integrations, and developer tooling.

## Current Status

This directory now includes CLI specifications for network-capable clients and tooling.
Additional tool integration specifications are located in:

- **[SB_ISQL_CLI_SPECIFICATION.md](SB_ISQL_CLI_SPECIFICATION.md)** - sb_isql network CLI (native + emulated clients)
- **[SB_BACKUP_CLI_SPECIFICATION.md](SB_BACKUP_CLI_SPECIFICATION.md)** - sb_backup CLI behavior
- **[SB_VERIFY_CLI_SPECIFICATION.md](SB_VERIFY_CLI_SPECIFICATION.md)** - sb_verify CLI behavior
- **[SB_SECURITY_CLI_SPECIFICATION.md](SB_SECURITY_CLI_SPECIFICATION.md)** - sb_security CLI behavior
- **[SB_TOOLING_NETWORK_SPEC.md](SB_TOOLING_NETWORK_SPEC.md)** - Network support for sb_backup/sb_verify/sb_security/sb_charset_loader (loader currently deprecated)
- **[SB_BUILD_AND_TEST_CLI_SPEC.md](SB_BUILD_AND_TEST_CLI_SPEC.md)** - sb_build / sb_test contract (CI)
- **[Beta Requirements - Tools](../beta_requirements/tools/)** - Tool compatibility specifications for Beta release
  - DBeaver
  - pgAdmin
  - MySQL Workbench
  - DataGrip
  - Excel
  - Power BI
  - Tableau
  - And more...

## Related Specifications

- [Beta Requirements - Tools](../beta_requirements/tools/) - Tool compatibility requirements
- [Admin](../admin/) - ScratchBird admin CLI
- [Operations](../operations/) - Monitoring and observability

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
