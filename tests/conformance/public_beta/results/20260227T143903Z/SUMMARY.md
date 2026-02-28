# Required Public Beta Gate

- Generated (UTC): 2026-02-27T14:42:23Z
- Result directory: /home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z

## Category Summary

| Category | Passed Steps | Failed Steps |
|---|---:|---:|
| `wire_protocol` | 5 | 2 |
| `transaction_semantics` | 5 | 0 |
| `security_enforcement` | 8 | 0 |
| `end_to_end_sql` | 2 | 0 |
| `modal_nosql` | 9 | 0 |
| `cluster_infra` | 10 | 0 |

## Step Results

| Category | Step | Result | Log |
|---|---|---|---|
| `wire_protocol` | `compat_postgresql` | `FAIL` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/compat_postgresql.log` |
| `wire_protocol` | `compat_mysql` | `FAIL` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/compat_mysql.log` |
| `wire_protocol` | `compat_firebird` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/compat_firebird.log` |
| `wire_protocol` | `pg_frame_conformance` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/pg_frame_conformance.log` |
| `wire_protocol` | `mysql_frame_conformance` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/mysql_frame_conformance.log` |
| `wire_protocol` | `firebird_frame_conformance` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/firebird_frame_conformance.log` |
| `wire_protocol` | `generic_protocol_frame_conformance` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/generic_protocol_frame_conformance.log` |
| `end_to_end_sql` | `compat_scratchbird_native` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/compat_scratchbird_native.log` |
| `end_to_end_sql` | `v3_native_inet_suite` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/v3_native_inet_suite.log` |
| `transaction_semantics` | `transaction_truth_matrix` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/transaction_truth_matrix.log` |
| `transaction_semantics` | `transaction_truth_native` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/transaction_truth_native.log` |
| `transaction_semantics` | `mga_basic_update` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/mga_basic_update.log` |
| `transaction_semantics` | `mga_mvcc_visibility` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/mga_mvcc_visibility.log` |
| `transaction_semantics` | `storage_tx_backversion` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/storage_tx_backversion.log` |
| `security_enforcement` | `security_parity_matrix` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/security_parity_matrix.log` |
| `security_enforcement` | `security_phase3_column` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/security_phase3_column.log` |
| `security_enforcement` | `security_phase3_rls_dml` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/security_phase3_rls_dml.log` |
| `security_enforcement` | `domain_security` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/domain_security.log` |
| `security_enforcement` | `domain_encryption` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/domain_encryption.log` |
| `security_enforcement` | `domain_e2e` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/domain_e2e.log` |
| `security_enforcement` | `audit_compliance_trail` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/audit_compliance_trail.log` |
| `security_enforcement` | `auth_policy_protocol_parity` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/auth_policy_protocol_parity.log` |
| `modal_nosql` | `parser_search_dsl_surface` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/parser_search_dsl_surface.log` |
| `modal_nosql` | `parser_search_dsl_negative` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/parser_search_dsl_negative.log` |
| `modal_nosql` | `parser_vector_surface` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/parser_vector_surface.log` |
| `modal_nosql` | `parser_vector_negative` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/parser_vector_negative.log` |
| `modal_nosql` | `parser_redis_surface` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/parser_redis_surface.log` |
| `modal_nosql` | `parser_redis_negative` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/parser_redis_negative.log` |
| `modal_nosql` | `nosql_emitter_mapping` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/nosql_emitter_mapping.log` |
| `modal_nosql` | `nosql_emitter_alias_reject` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/nosql_emitter_alias_reject.log` |
| `modal_nosql` | `nosql_virtual_catalog_contract` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/nosql_virtual_catalog_contract.log` |
| `cluster_infra` | `cluster_fencing_term` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_fencing_term.log` |
| `cluster_infra` | `cluster_fencing_epoch` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_fencing_epoch.log` |
| `cluster_infra` | `cluster_identity_default` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_identity_default.log` |
| `cluster_infra` | `cluster_identity_persist` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_identity_persist.log` |
| `cluster_infra` | `cluster_replication_pipeline` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_replication_pipeline.log` |
| `cluster_infra` | `cluster_replication_conflict_contracts` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_replication_conflict_contracts.log` |
| `cluster_infra` | `cluster_observability_rows` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_observability_rows.log` |
| `cluster_infra` | `cluster_parser_control_surface` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_parser_control_surface.log` |
| `cluster_infra` | `cluster_parser_replication_surface` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_parser_replication_surface.log` |
| `cluster_infra` | `cluster_parser_replication_negative` | `PASS` | `/home/dcalford/CliWork/ScratchBird/tests/conformance/public_beta/results/20260227T143903Z/logs/cluster_parser_replication_negative.log` |
