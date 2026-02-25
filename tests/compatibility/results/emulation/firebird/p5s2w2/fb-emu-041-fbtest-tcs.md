Last updated: 2026-02-24

# FB-EMU-041 Legacy fbtest/TCS Integration Report

- Mode: `execute`
- Overall result: `fail`
- Command timeout: `1200s`
- `sb_fb_isql` resolution: `/home/dcalford/CliWork/ScratchBird-driver/build/tracks/alpha/drivers/cli/sb_isql`

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
- `cmd`: `scripts/run_firebird_ctest.sh`
- `exit_code`: `2`
- `timed_out`: `false`

```text
sb_fb_isql unavailable; found only generic client at /home/dcalford/CliWork/ScratchBird-driver/build/tracks/alpha/drivers/cli/sb_isql. Generic sb_isql is rejected for Firebird wire-protocol parity.
```

## Notes
- This report captures legacy vector integration status, not final parity closure.
- Full fbtest/TCS execution closure remains tracked in FB-EMU-041/042.
