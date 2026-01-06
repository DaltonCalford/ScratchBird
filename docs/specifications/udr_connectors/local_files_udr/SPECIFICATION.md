# Local Files UDR Specification

Status: Draft (Target). This specification defines the Local Files UDR used to
access CSV and JSON data from local disk only.

## Scope
- Local filesystem access only (no network paths).
- Supported formats: CSV, JSON (array), JSON Lines (jsonl).
- Optional write support if explicitly enabled (default read-only).

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../10-UDR-System-Specification.md
- ../../09_DDL_FOREIGN_DATA.md

## UDR Module
- Library: libscratchbird_local_files_udr.so
- FDW handler: file_udr_handler
- FDW validator: file_udr_validator
- Capabilities: file_read (optional file_write)

## Server Options
Use CREATE SERVER with file-specific options:

| Option | Default | Description |
| --- | --- | --- |
| root_dir | required | Base directory for all file access |
| path_allowlist | "**" | Glob allowlist relative to root_dir |
| path_denylist | "" | Glob denylist relative to root_dir |
| allow_write | false | Enable INSERT/UPDATE/DELETE to files |
| max_rows | 0 | 0=unlimited, hard cap per scan |
| max_bytes | 0 | 0=unlimited, hard cap per scan |
| on_error | fail | fail/skip/log for row parse errors |

Example:
```sql
CREATE FOREIGN DATA WRAPPER file_udr
    HANDLER file_udr_handler
    VALIDATOR file_udr_validator;

CREATE SERVER local_files
    FOREIGN DATA WRAPPER file_udr
    OPTIONS (
        root_dir '/var/scratchbird/imports',
        path_allowlist 'data/**',
        path_denylist 'data/private/**',
        allow_write 'false',
        max_rows '500000'
    );
```

## Foreign Table Options

| Option | Default | Description |
| --- | --- | --- |
| file_path | required | Relative path under root_dir |
| format | csv | csv/json/jsonl |
| encoding | utf-8 | Character encoding |
| header | true | CSV header row present |
| delimiter | , | CSV delimiter |
| quote | " | CSV quote char |
| escape | " | CSV escape char |
| null_string | "" | CSV null representation |
| trim | true | Trim whitespace |
| date_format | yyyy-mm-dd | Date parse format |
| time_format | hh:mm:ss | Time parse format |
| timestamp_format | iso8601 | Timestamp parse format |
| json_path | "$" | JSON path to array (json only) |
| infer_schema | false | Infer schema if no columns provided |
| sample_rows | 1000 | Rows to infer schema from |

Example:
```sql
CREATE FOREIGN TABLE sales_csv (
    id INTEGER,
    sku TEXT,
    qty INTEGER,
    price DECIMAL(10,2),
    sold_at TIMESTAMP
)
SERVER local_files
OPTIONS (
    file_path 'data/sales_2024.csv',
    format 'csv',
    header 'true',
    delimiter ',',
    timestamp_format 'yyyy-mm-dd hh:mm:ss'
);
```

## Query Behavior
- SELECT uses streaming parsing with backpressure.
- WHERE and LIMIT are applied locally after parsing.
- INSERT/UPDATE/DELETE are allowed only if allow_write=true and format supports
  safe writes (csv append or jsonl append; json array requires rewrite).

## Pass-through and DDL
- Pass-through SQL is not applicable; file UDR does not accept remote SQL.
- DDL is limited to CREATE/ALTER/DROP FOREIGN TABLE objects.

## Example Usage
```sql
SELECT sku, SUM(qty) FROM sales_csv GROUP BY sku;
```

## Example UDR Entry Points (C++)
```cpp
class FileUdrModule : public IPluginModule { /* ... */ };
class FileExternalTable : public IExternalProcedure { /* ... */ };
```

## Security Requirements
- Reject absolute paths and path traversal.
- Resolve symlinks and ensure final path is under root_dir.
- Enforce allowlist/denylist patterns.
- No network access in sandbox.

## Testing Checklist
- CSV with/without header, custom delimiter, quoted fields.
- JSON array and jsonl parsing.
- Path traversal attempts are blocked.
- allow_write false prevents DML.
