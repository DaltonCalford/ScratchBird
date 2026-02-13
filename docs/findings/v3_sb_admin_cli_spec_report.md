# SB_ADMIN_CLI_SPECIFICATION.md - Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/admin/SB_ADMIN_CLI_SPECIFICATION.md`

Status notes:
- The document explicitly states it is **Non-Authoritative** and points to `AUTHORITATIVE_SPEC_INVENTORY.md`.
- This spec describes a standalone CLI tool (`sb_admin`). No implementation code for `sb_admin` appears to live in this repo (no `src/` or `include/` hits, and no CMake target definition found here).
- The project README indicates CLI tools like `sb_admin` are in the ScratchBird-driver repo, suggesting this spec may be implemented externally.

Verification:
- No code-level verification performed in this repo; `sb_admin` implementation not located here.

Command groups and features listed (for later verification in the CLI tool repo):
- Global options (host/port/user/password/database/config/json/timeout/version/help, etc.).
- Server commands: status/start/stop/restart/reload/info/config/connections/kill/terminate.
- Database commands: list/create/drop/info/size/sweep/analyze/check.
- Cluster commands: status/init/join/leave/nodes/promote/demote/failover/rebalance/sync-status.
- User commands: list/create/drop/alter/password/roles/grant/revoke.
- Backup commands: create/list/info/restore/verify/delete/schedule/export (+ options for compression/parallel/manifest/etc.).
- Restore commands: full/pitr/table/status/cancel.
- Diagnostics: health, diag (slow-queries/locks/bloat/cache/io/wait-events/activity/explain), logs (tail/search/errors/stats).
- Monitoring: Nagios checks, Prometheus metrics, SNMP support.
- Maintenance: sweep/GC (VACUUM alias), sweep status, reindex, maintenance mode.
- Security: audit/ssl/keys/firewall.
- Config file format, exit codes, environment variables.
