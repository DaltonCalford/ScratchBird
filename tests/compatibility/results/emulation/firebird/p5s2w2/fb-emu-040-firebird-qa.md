Last updated: 2026-03-24

# FB-EMU-040 Firebird-QA Gate Integration

- Gate mode: `execute`

## Prerequisites
- `tests/compatibility/firebird/repos/fbt-repository`: `present`
- `firebird-qa harness clone`: `present`
- `python3 runtime`: `present`

## Command templates
```bash
cd tests/compatibility/firebird/repos/firebird-qa
python3 -m venv ../../runtime/firebird-qa-venv
../../runtime/firebird-qa-venv/bin/python -m pip install -U pip setuptools wheel
../../runtime/firebird-qa-venv/bin/python -m pip install -e .
../../runtime/firebird-qa-venv/bin/python -c "import firebird.qa.plugin; print('firebird_qa_plugin_import_ok')"
```

## Notes
- Dry-run initializes gate contract and prerequisites only.
- firebird-qa snapshot is required inside ScratchBird/tests/compatibility.
- firebird-qa harness clone is present in this workspace snapshot.
- Evidence file path matches tracker row FB-EMU-040.
