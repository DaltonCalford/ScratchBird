# Issues

- task_id: `ENG-MYSQL-003`
- gate: `ENG-MYSQL-GATE-03`
- blocker_class: `toolchain/environment`
- observed_behavior: `Compare runner aborted immediately because only generic sb_isql is available; generic client is native-protocol only and cannot provide MySQL wire-protocol parity`
- expected_behavior: `row should complete and close gate with required evidence`
- evidence_path: `/home/dcalford/CliWork/ScratchBird/artifacts/baseline/mysql/p6s1w3/eng-mysql-003-compare`
- proposed_options:
  1. Provide `sb_my_isql` via `SCRATCHBIRD_MY_ISQL` (or build FDW CLI wrappers in ScratchBird-driver).
  2. Keep generic `sb_isql` blocked for this lane to prevent false parity signals.
  3. Rerun compare gate after wrapper availability is restored.
