# Outstanding Work Tracker (Master)

Status: In Progress
Last Updated: 2026-02-02

This tracker is the **single source of truth** for remaining work. All other
planning docs can be archived after this is in place.

## A) Git Config Key Normalization (PLAN_GIT_CONFIG_KEY_NORMALIZATION.md)

### A1. YAML Parser Updates (src/git/GitConfigParser.cpp)
- [ ] Parse canonical keys: `repo_type`, `repo_url`, `repo_path`, `repo_mode`,
      `repo_branch`, `integration_mode`, `sign_commits`, `commit_template`.
- [ ] Accept legacy aliases (`url`, `branch`, `path`, `mode`, `type`) with
      canonical precedence when both are present.
- [ ] Parse `auto_pull` consistently with auto_commit/auto_push.
- [ ] Update `validate()` to require `repo_url` (allow legacy alias).
- [ ] Update `toYAML()` to emit canonical `repo_*` keys.

### A2. sb_config.ini Support
- [ ] Add INI parsing path for `[git.*]` sections.
- [ ] Map INI sections to the same config structures as YAML.
- [ ] Apply canonical precedence + legacy alias handling for INI inputs.

### A3. Tests + Diagnostics
- [ ] Unit tests for canonical keys (YAML).
- [ ] Unit tests for legacy aliases + precedence (YAML).
- [ ] Unit tests for INI parsing (repository/schema/migrations/envs).
- [ ] Add a config lint/diagnostic message for deprecated keys.

### A4. Docs Sync (post-implementation)
- [ ] Confirm user docs reflect updated parser behavior.
- [ ] Update config examples to use canonical keys.

## B) SBLR Type Opcode Remediation Tests (SBLR_TYPE_OPCODE_REMEDIATION_PLAN.md)

### B1. SBLR Unit Tests
- [ ] Add bytecode round-trip tests for all SBLR type markers.
- [ ] Add typed literal parsing tests for new literal opcodes.

### B2. DDL/DML Coverage
- [ ] Minimal CREATE TABLE + INSERT/SELECT coverage for each new type.

## Exit Criteria

- All checklist items are complete.
- Tests for A3 and B1/B2 pass.
