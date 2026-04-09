# IMP-01 Implementation Checklist

## Ticket
- ID: `IMP-01`
- Section: `01_Configuration_Subsystem`
- Gate Contract: `docs/specifications/01_Configuration_Subsystem/TEST_CONTRACT.md`

## Inputs
- `docs/specifications/01_Configuration_Subsystem/SPEC_OUTLINE.md`
- `docs/specifications/01_Configuration_Subsystem/CONFIG_CATALOG_AND_BOOTSTRAP.md`
- `docs/specifications/01_Configuration_Subsystem/CONFIG_SQL_SURFACE.md`
- `docs/specifications/01_Configuration_Subsystem/CONFIG_DEFAULTS.md`
- `docs/specifications/01_Configuration_Subsystem/CONFIG_MODELS_WORKGROUP_AND_CLUSTER.md`
- `docs/specifications/01_Configuration_Subsystem/TEST_CONTRACT.md`

## Ordered Tasks
1. Implement config-source precedence:
- binary defaults -> bootstrap file -> catalog values after mount -> session overrides.
2. Implement strict parser/validator for bootstrap config file:
- enforce required keys.
- enforce duplicate-key rejection.
- enforce unknown-key policy by namespace/profile.
3. Implement catalog-backed configuration authority:
- `sys.config.key`
- `sys.config.value`
- `sys.config.change_log`
4. Implement SQL control surface:
- `SHOW CONFIG`
- `SET CONFIG`
- `RESET CONFIG`
- `CONFIG HISTORY`
- `RELOAD CONFIG`
- `SHOW RESOURCE BUNDLES`
- `VALIDATE RESOURCE BUNDLE`
- `ACTIVATE RESOURCE BUNDLE`
5. Implement reload/restart semantics:
- apply live-reload keys on reload boundary.
- gate restart-required keys.
6. Implement scope and security controls:
- scope semantics for instance/database/schema/user/session.
- role checks for config admin and sensitive-key visibility.
7. Implement cluster/workgroup propagation semantics:
- workgroup isolation.
- cluster propagation idempotency `(key_id, scope_uuid, change_id)`.
- deterministic retry/backoff policy.
8. Implement required tests from test contract:
- parsing/validation tests.
- precedence/override tests.
- reload safety tests.
- resource bundle validation/activation tests.
9. Record evidence artifacts and gate result.

## Exit Criteria
- All required tests pass.
- `GATE_RESULT.json` result is `pass`.
- Traceability rows map implemented code paths and tests to section requirements.
