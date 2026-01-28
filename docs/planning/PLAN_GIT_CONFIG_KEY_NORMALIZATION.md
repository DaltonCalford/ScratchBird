# Plan: Git Config Key Normalization

## Objective

Normalize Git integration configuration keys across `.scratchbird.yml` and
`sb_config.ini`, ensure legacy aliases continue to work, and enforce canonical
key precedence. This aligns runtime parsing with the Git integration spec.

## Constraints

- Documentation updates are complete in
  `ScratchBird/docs/specifications/core/GIT_METADATA_INTEGRATION_SPECIFICATION.md`.

## Checklist

### Phase 0 - Spec Alignment (Docs)

- [x] Define canonical `repository.repo_*` keys in the Git integration spec.
- [x] Document legacy key aliases and precedence rules.
- [x] Record current parser gaps in the spec (GitConfigParser audit).
- [x] Add user-facing upgrade note in the configuration guide.

### Phase 1 - YAML Parser Updates (src/git/GitConfigParser.cpp)

- [ ] Add canonical key parsing: `repo_type`, `repo_url`, `repo_path`,
  `repo_mode`, `repo_branch`, `integration_mode`, `sign_commits`,
  `commit_template`.
- [ ] Accept legacy aliases (`url`, `branch`, `path`, `mode`, `type`) with
  canonical precedence when both are present.
- [ ] Parse `auto_pull` and expose it consistently with existing auto_commit/push.
- [ ] Update `validate()` to require `repo_url` (accept legacy alias).
- [ ] Update `toYAML()` output to emit canonical `repo_*` keys.

### Phase 2 - sb_config.ini Support

- [ ] Define a minimal INI parser path for `[git.*]` sections.
- [ ] Map INI sections to the same config structures used by YAML.
- [ ] Apply canonical precedence and legacy alias handling for INI inputs.

### Phase 3 - Tests and Diagnostics

- [ ] Add unit tests for canonical keys (YAML).
- [ ] Add unit tests for legacy aliases (YAML) and precedence resolution.
- [ ] Add unit tests for INI parsing across repository/schema/migrations/envs.
- [ ] Add a config lint/diagnostic message for deprecated keys.

### Phase 4 - Docs Sync (Post-Implementation)

- [ ] Confirm user docs reflect the updated parser behavior.
- [ ] Update any config examples that still use legacy keys.
