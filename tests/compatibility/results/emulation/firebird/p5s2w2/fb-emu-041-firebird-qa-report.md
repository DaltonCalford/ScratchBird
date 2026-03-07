Last updated: 2026-03-06

# FB-EMU-041 Firebird-QA Smoke Report

- Mode: `execute`
- Overall result: `pass`
- Command timeout: `7200s`

## Command Results

### Command 1
- `cwd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/repos/firebird-qa`
- `cmd`: `python3 -m venv /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv`
- `exit_code`: `0`
- `timed_out`: `false`

```text
<no output>
```

### Command 2
- `cwd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/repos/firebird-qa`
- `cmd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/bin/python -m pip install -U pip setuptools wheel`
- `exit_code`: `0`
- `timed_out`: `false`

```text
Requirement already satisfied: pip in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (26.0.1)
Requirement already satisfied: setuptools in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (82.0.0)
Requirement already satisfied: wheel in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (0.46.3)
Requirement already satisfied: packaging>=24.0 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from wheel) (26.0)
```

### Command 3
- `cwd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/repos/firebird-qa`
- `cmd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/bin/python -m pip install -e .`
- `exit_code`: `0`
- `timed_out`: `false`

```text
Obtaining file:///home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/repos/firebird-qa
  Installing build dependencies: started
  Installing build dependencies: finished with status 'done'
  Checking if build backend supports build_editable: started
  Checking if build backend supports build_editable: finished with status 'done'
  Getting requirements to build editable: started
  Getting requirements to build editable: finished with status 'done'
  Installing backend dependencies: started
  Installing backend dependencies: finished with status 'done'
  Preparing editable metadata (pyproject.toml): started
  Preparing editable metadata (pyproject.toml): finished with status 'done'
Requirement already satisfied: firebird-base~=2.0 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from firebird-qa==0.21.0) (2.0.2)
Requirement already satisfied: firebird-driver~=2.0 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from firebird-qa==0.21.0) (2.0.2)
Requirement already satisfied: psutil~=5.9 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from firebird-qa==0.21.0) (5.9.8)
Requirement already satisfied: pytest>=7.4 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from firebird-qa==0.21.0) (9.0.2)
Requirement already satisfied: protobuf~=5.29 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from firebird-base~=2.0->firebird-qa==0.21.0) (5.29.6)
Requirement already satisfied: python-dateutil~=2.8 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from firebird-driver~=2.0->firebird-qa==0.21.0) (2.9.0.post0)
Requirement already satisfied: six>=1.5 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from python-dateutil~=2.8->firebird-driver~=2.0->firebird-qa==0.21.0) (1.17.0)
Requirement already satisfied: iniconfig>=1.0.1 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from pytest>=7.4->firebird-qa==0.21.0) (2.3.0)
Requirement already satisfied: packaging>=22 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from pytest>=7.4->firebird-qa==0.21.0) (26.0)
Requirement already satisfied: pluggy<2,>=1.5 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from pytest>=7.4->firebird-qa==0.21.0) (1.6.0)
Requirement already satisfied: pygments>=2.7.2 in /home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/lib/python3.12/site-packages (from pytest>=7.4->firebird-qa==0.21.0) (2.19.2)
Building wheels for collected packages: firebird-qa
  Building editable for firebird-qa (pyproject.toml): started
  Building editable for firebird-qa (pyproject.toml): finished with status 'done'
  Created wheel for firebird-qa: filename=firebird_qa-0.21.0-py3-none-any.whl size=5496 sha256=144aa8f879862f92bd2b759a0f585d6ec2669ecd8b9703251247445caea566f8
  Stored in directory: <outside-tree-path>
Successfully built firebird-qa
Installing collected packages: firebird-qa
  Attempting uninstall: firebird-qa
    Found existing installation: firebird-qa 0.21.0
    Uninstalling firebird-qa-0.21.0:
      Successfully uninstalled firebird-qa-0.21.0
Successfully installed firebird-qa-0.21.0
```

### Command 4
- `cwd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/repos/firebird-qa`
- `cmd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/firebird/runtime/firebird-qa-venv/bin/python -c 'import firebird.qa.plugin; print('"'"'firebird_qa_plugin_import_ok'"'"')'`
- `exit_code`: `0`
- `timed_out`: `false`

```text
firebird_qa_plugin_import_ok
```

## Notes
- Smoke run validates harness installability and plugin import via Python 3 virtualenv.
- Full firebird-qa pytest collection/execution requires valid server credentials in firebird-driver.conf.
- Full Firebird compatibility closure remains tracked outside this bootstrap gate.
