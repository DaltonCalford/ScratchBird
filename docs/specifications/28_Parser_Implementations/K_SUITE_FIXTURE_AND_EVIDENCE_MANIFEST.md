# K-Suite Fixture and Evidence Manifest

## Purpose
Define exact fixture inputs and evidence outputs for parser checklist gates `K-001` through `K-013`.

## Scope
- Applies to all parser targets:
  - `native`
  - `firebird`
  - `postgresql`
  - `mysql`
  - `cassandra`
  - `mongodb`
  - `neo4j`
  - `redis`
  - `milvus`

## Fixture Root Contract
All K-suite fixtures must live under:

```text
tests/conformance/parser/k_suite/
  manifest.json
  cases/
  wire/
  profiles/
  seeds/catalog/
  seeds/security/
  expected/
```

Required naming rules:
1. Case fixture:
   - `cases/<gate_id>__<target>__<scenario>.yaml`
2. Wire fixture (when required by gate):
   - `wire/<gate_id>__<target>__<scenario>.bin`
3. Profile fixture:
   - `profiles/<target>__profile_v<version>.json`
4. Catalog seed:
   - `seeds/catalog/<gate_id>__<target>.catalog_seed.sql`
5. Security seed:
   - `seeds/security/<gate_id>__<target>.security_seed.sql`
6. Expected result:
   - `expected/<gate_id>__<target>__<scenario>.expected.json`

`<gate_id>` is lowercase:
- `k-001` through `k-013`

Required scenarios for every gate and target:
- `success`
- `negative`

## Evidence Bundle Contract
For each gate/target/scenario execution, write:

```text
docs/specifications/work/implementation_tracks/K-SUITE/<run_id>/<gate_id>__<target>__<scenario>/
  artifact_manifest.json
  input_snapshot.json
  observed.json
  compare.json
  raw.log
```

Failure findings must also be written to:
- `docs/specifications/work/findings/K_SUITE_<run_id>_<gate_id>__<target>__<scenario>.md`

## Deterministic Seed Contract
Each case fixture must include:
- `rng_seed_u64`
- `clock_seed_unix_ms`
- `profile_version`
- `catalog_epoch`
- `security_epoch`
- `transaction_snapshot_id`

## Expected JSON Schema
`expected/*.expected.json` must contain:
- `gate_id`
- `target`
- `scenario`
- `expected_status`
- `expected_decision`
- `expected_error_code` (nullable)
- `expected_sblr_checksum` (nullable for rejects)
- `expected_diagnostics_fields`
- `expected_source_map_policy`
- `expected_wire_policy`

## Gate-to-Fixture Mapping

| Gate | Primary Case Template | Wire Required | Required Assertions |
| --- | --- | --- | --- |
| `K-001` | `cases/k-001__<target>__<scenario>.yaml` | no | context freeze, active transaction enforcement |
| `K-002` | `cases/k-002__<target>__<scenario>.yaml` | yes | malformed ingress rejection determinism |
| `K-003` | `cases/k-003__<target>__<scenario>.yaml` | no | stable source-span map |
| `K-004` | `cases/k-004__<target>__<scenario>.yaml` | no | capability precedence and missing-row reject |
| `K-005` | `cases/k-005__<target>__<scenario>.yaml` | no | canonicalization with no dialect leakage |
| `K-006` | `cases/k-006__<target>__<scenario>.yaml` | no | `NOT_DISCOVERABLE` mapping policy |
| `K-007` | `cases/k-007__<target>__<scenario>.yaml` | no | deterministic `parameter_signature` |
| `K-008` | `cases/k-008__<target>__<scenario>.yaml` | no | normalization evidence present in SBLR |
| `K-009` | `cases/k-009__<target>__<scenario>.yaml` | no | parser preflight rejects invalid SBLR |
| `K-010` | `cases/k-010__<target>__<scenario>.yaml` | no | full execute envelope completeness |
| `K-011` | `cases/k-011__<target>__<scenario>.yaml` | no | deterministic error map and correlation id |
| `K-012` | `cases/k-012__<target>__<scenario>.yaml` | no | stale handle rerun path determinism |
| `K-013` | `cases/k-013__<target>__<scenario>.yaml` | yes | dialect egress rendering policy |

## Minimum Case Count Contract
Required K-suite minimum:
- `13 gates * 9 targets * 2 scenarios = 234 cases`

Any missing gate/target/scenario triplet is a hard gate failure.

## Required Compare Fields
`compare.json` must include:
- `gate_id`
- `target`
- `scenario`
- `pass`
- `failed_assertions[]`
- `expected_checksum`
- `observed_checksum`
- `expected_error_code`
- `observed_error_code`
- `expected_decision`
- `observed_decision`

## Execution Command Contract
Canonical test runner invocation:

```bash
ctest --test-dir build --output-on-failure -R '^(K-|PARSER-K-)'
```

Replay invocation for one gate on one target:

```bash
ctest --test-dir build --output-on-failure -R '^K-006__postgresql__negative$'
```

## Gate Integration Rule
`K-*` status is `PASS` only when:
1. All required fixtures exist for every target and both scenarios.
2. Observed decision and error mapping match expected artifacts.
3. Determinism checksums match repeated runs for the same seeds.
4. Evidence bundle files exist and checksum-verify.
5. Failure findings are present for any failed case.


## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.
