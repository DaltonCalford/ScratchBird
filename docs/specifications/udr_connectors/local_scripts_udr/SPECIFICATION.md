# Local Scripts UDR Specification

Status: Draft (Target). This specification defines the Local Scripts UDR used
for local-only script execution to produce tabular data.

## Scope
- Executes approved local scripts only (no network access).
- Scripts return rows in CSV or JSONL format.
- Intended for ETL helpers and legacy data access wrappers.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../10-UDR-System-Specification.md

## UDR Module
- Library: libscratchbird_local_scripts_udr.so
- FDW handler: script_udr_handler
- FDW validator: script_udr_validator
- Capabilities: subprocess + file_read (optional file_write)

## Server Options

| Option | Default | Description |
| --- | --- | --- |
| root_dir | required | Base directory for scripts |
| interpreter_allowlist | "" | Comma list of allowed interpreters |
| allow_write | false | Allow scripts to write to root_dir |
| max_run_ms | 30000 | Execution timeout |
| max_output_bytes | 64MB | Output cap |
| max_rows | 0 | 0=unlimited |
| env_allowlist | "" | Allowed env vars forwarded |

Example:
```sql
CREATE FOREIGN DATA WRAPPER script_udr
    HANDLER script_udr_handler
    VALIDATOR script_udr_validator;

CREATE SERVER local_scripts
    FOREIGN DATA WRAPPER script_udr
    OPTIONS (
        root_dir '/opt/scratchbird/scripts',
        interpreter_allowlist '/usr/bin/python3,/usr/bin/bash',
        max_run_ms '15000'
    );
```

## Foreign Table Options

| Option | Default | Description |
| --- | --- | --- |
| script | required | Script path relative to root_dir |
| args | "" | Static args passed to script |
| input_format | json | json or csv (params) |
| output_format | jsonl | jsonl or csv |
| header | true | CSV header in output |
| delimiter | , | CSV delimiter |

Example:
```sql
CREATE FOREIGN TABLE local_report (
    id INTEGER,
    name TEXT,
    total DECIMAL(10,2)
)
SERVER local_scripts
OPTIONS (
    script 'reporting/export_orders.py',
    args '--region=us',
    output_format 'jsonl'
);
```

## Script IO Contract
- The engine invokes the script with argv = [script, args...].
- Parameters are provided on stdin in JSON:
```json
{"sql":"select * from local_report where id = :id","params":{"id":42}}
```
- The script writes rows to stdout in JSONL:
```json
{"id":1,"name":"A","total":12.50}
{"id":2,"name":"B","total":9.99}
```
- Non-zero exit codes are mapped to UDR errors.

## Example Script (Python)
```python
import json, sys
for line in sys.stdin:
    req = json.loads(line)
    if req.get("params", {}).get("id") == 42:
        print(json.dumps({"id": 42, "name": "Widget", "total": 10.0}))
```

## Security Requirements
- No network access; deny sockets in sandbox.
- Script path must be under root_dir.
- Enforce interpreter_allowlist.
- Kill on timeout and output limit.

## Testing Checklist
- Script with JSONL output.
- Script timeout enforced.
- Network call from script blocked.
