Last updated: 2026-03-24

# FB-EMU-041 Legacy fbtest/TCS Integration Report

- Mode: `execute`
- Overall result: `pass`
- Command timeout: `7200s`
- `sb_fb_isql` resolution: `<outside-tree-path>`

## Command Results

### Command 1
- `cwd`: `/home/dcalford/CliWork/ScratchBird`
- `cmd`: `python3 -c 'from pathlib import Path; root=Path('"'"'tests/compatibility/firebird/repos/fbt-repository/tests'"'"'); gtcs=root/'"'"'functional/gtcs'"'"'; total=sum(1 for _ in root.rglob('"'"'*.fbt'"'"')); gtcs_total=sum(1 for _ in gtcs.rglob('"'"'*.fbt'"'"')); print(f'"'"'fbt_total={total}'"'"'); print(f'"'"'gtcs_total={gtcs_total}'"'"')'`
- `exit_code`: `0`
- `timed_out`: `false`

```text
fbt_total=2283
gtcs_total=86
```

### Command 2
- `cwd`: `/home/dcalford/CliWork/ScratchBird`
- `cmd`: `python3 /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/scripts/convert_fbt_to_sql.py /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/repos/fbt-repository/tests/functional/gtcs/dsql-domain-01.fbt /home/dcalford/CliWork/ScratchBird/tests/compatibility/results/emulation/firebird/p5s2w2/fbtest_tcs_smoke/converted`
- `exit_code`: `0`
- `timed_out`: `false`

```text
✓ Converted: dsql-domain-01.fbt → dsql-domain-01.sql
```

### Command 3
- `cwd`: `/home/dcalford/CliWork/ScratchBird`
- `cmd`: `bash -lc 'SCRATCHBIRD_FB_ISQL=<outside-tree-path> /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/scripts/run_firebird_ctest.sh'`
- `exit_code`: `0`
- `timed_out`: `false`

```text
Firebird compatibility tests passed. Logs: /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/results/ctest/20260324_014000
```

## Notes
- This report captures legacy vector integration status, not final parity closure.
- Full fbtest/TCS execution closure remains tracked in FB-EMU-041/042.
