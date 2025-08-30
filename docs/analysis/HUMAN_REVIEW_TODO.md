[ScratchBird Analysis Documentation](index.md)

### Human review TODOs

Ownership: Senior developer(s) familiar with transactions, optimizer, server.

- Validate transaction visibility semantics and conflict detection
  - Files: `include/scratchbird/engine/txn.h`, `src/engine/txn.cpp`, `src/engine/serializable_isolation.cpp`
  - REQs: REQ-TXN-MGA-*

- Confirm deferrability and FK cascade edge cases
  - Files: `trigger_engine.*`, `alter_table_manager.*`, `catalog_manager.*`
  - REQs: REQ-INTEGRITY-TRIG-*, REQ-INTEGRITY-*

- Review optimizer cost constants and plan cache invalidation
  - Files: `planner.cpp`, `statistics.cpp`, `plan_cache.*`, `prepared_statement_cache.*`
  - REQs: REQ-OPT-STAT-*

- Validate server protocol compatibility and TLS/auth flows
  - Files: `network_server.*`, `protocol_handler.*`, `firebird_protocol*.{h,cpp}`, `authentication.*`, `tls_server.*`
  - REQs: REQ-SERVER-YVALVE-*, REQ-AUTH-*, REQ-TLS

- Assess WAL/recovery guarantees against spec claims
  - Files: `wal*.{h,cpp}`, `wal_manager.*`
  - REQs: (to be added for recovery phases)

- PSQL semantics: exception propagation, security context, package lifecycle
  - Files: `psql_executor.*`, `psql_dev_tools.*`
  - REQs: REQ-PSQL-RUNTIME-*

## Related
- [Documentation style guide](_style.md)
- [ScratchBird Analysis Documentation](index.md)
