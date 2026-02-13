# SB Build/Test CLI Specification (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: define a deterministic CLI contract for building and testing the
ScratchBird project. This is required for automated validation and CI.

## Command: `sb_build`

Usage:
```
sb_build [--clean] [--config=debug|release] [--jobs=N] [--target=all|engine|drivers]
```

Behavior:
- `--clean` removes previous build artifacts.
- `--config` selects build type (default: `debug`).
- `--jobs` sets parallelism (default: hardware concurrency).
- `--target` selects build scope.

Exit Codes:
- `0` success
- `2` build failed
- `3` invalid arguments

## Command: `sb_test`

Usage:
```
sb_test [--suite=unit|integration|all] [--dialect=scratchbird|postgres|mysql|firebird|all] [--filter=PATTERN]
```

Behavior:
- `--suite` selects test suite(s) (default: `all`).
- `--dialect` restricts dialect tests (default: `all`).
- `--filter` runs only tests matching pattern.

Exit Codes:
- `0` success
- `4` tests failed
- `5` invalid arguments

## Output Contract

- All commands MUST emit JSON on stdout when `SB_CI=1`:
  `{ "status": "ok|fail", "code": <exit_code>, "summary": "...", "details": [...] }`
- Human-readable output is allowed when `SB_CI` is not set.
