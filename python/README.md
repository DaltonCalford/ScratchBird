# ScratchBird Python Driver

ScratchBird DB-API 2.0 driver using the ScratchBird native wire protocol.

## Development

```bash
python -m pip install -e .
```

## Testing

Unit tests:

```bash
python -m pip install -e ".[test]"
pytest
```

Integration tests (requires a running server and a DSN):

```bash
export SCRATCHBIRD_TEST_DSN="scratchbird://user:pass@localhost:3092/mydb"
pytest python/tests/test_integration.py
```

## Packaging

Build a wheel/sdist:

```bash
python -m pip install build
python -m build
```

## Publish

```bash
python -m pip install twine
twine upload dist/*
```
