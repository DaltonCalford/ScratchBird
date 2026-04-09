# Beta 2 Delimited Text File Read Write And Overwrite UDR Model

## Purpose

This document defines the UDR package family used to treat delimited text
files as governed external data sources for bounded read, append-write, and
overwrite workflows.

The package covers `CSV` and `TSV` as the required Beta 2 minimum formats.

## Owning package

- `sb_pkg_textfile_udr`

## Scope

This package owns:

- delimited file open and descriptor validation
- schema declaration and bounded schema inference
- rowset read
- append write
- overwrite and atomic replace
- error-row capture and diagnostics
- file capability metadata and policy enforcement

## Explicit non-goals

This package does not claim:

- arbitrary filesystem browsing without path policy
- unrestricted local file mutation
- true in-place row update/delete semantics for flat files
- spreadsheet, columnar, or binary file formats
- remote object-store access as a baseline requirement

## Mandatory file formats

Beta 2 must support:

- `CSV`
- `TSV`

The format model must expose:

- delimiter
- quote character
- escape character
- newline mode
- header row present/absent
- encoding
- null token policy
- trim policy

## Required routine families

At minimum the package shall provide:

- `sb_textfile.describe(...)`
- `sb_textfile.read_rows(...)`
- `sb_textfile.read_table(...)`
- `sb_textfile.write_rows(...)`
- `sb_textfile.append_rows(...)`
- `sb_textfile.overwrite_rows(...)`
- `sb_textfile.validate_schema(...)`

## Example contract

```sql
select *
from sb_textfile.read_table(
    path => '/var/lib/scratchbird/import/customers.csv',
    format => 'csv',
    header => true,
    schema_json => json_object(
        'customer_id', 'BIGINT',
        'name', 'STRING',
        'created_at', 'TIMESTAMP'
    )
);

call sb_textfile.overwrite_rows(
    path => '/var/lib/scratchbird/export/orders.tsv',
    format => 'tsv',
    source_query => 'select * from analytics.orders_export'
);
```

## Access-policy rules

1. Every file path must resolve under an approved import/export root.
2. Read and write roots may be different and separately policy-controlled.
3. The package must reject paths outside approved roots before file access.
4. Secrets, credentials, and arbitrary host files must not be reachable
   through this package.

## Read rules

1. Reads may be schema-declared or schema-inferred.
2. Schema inference is bounded by explicit sample-row and byte ceilings.
3. Header-based column names must be normalized deterministically.
4. Type inference must never silently widen into lossy text defaults when an
   explicit schema is provided.
5. Malformed rows may be:
   - rejected fail-closed
   - diverted into an error-row sink
   - accepted only when explicit permissive mode is selected

## Write rules

1. Append write appends new logical rows to an existing file while preserving
   the current format contract.
2. Overwrite must use a temporary-file plus atomic replace contract; partial
   overwrite is forbidden.
3. Header rows must be emitted deterministically according to the declared
   schema/order.
4. Output encoding, newline, delimiter, quote, escape, and null token policy
   must be explicit.
5. Write failures must not leave partially promoted target files behind.

## CRUD boundary

Delimited text files are not random-update stores. Therefore:

- `read` is required
- `create/append` is required
- full-file `overwrite` is required
- in-place `update` is not a Beta 2 requirement
- in-place `delete` is not a Beta 2 requirement

Any logical update/delete workflow over a text file must be expressed as:

1. read source rows
2. apply transformation/filter
3. overwrite target atomically

## Type mapping

The package shall support deterministic mapping between delimited text and
admitted ScratchBird scalar and complex types for:

- integer families
- decimal families
- floating families
- boolean
- date/time/timestamp families
- string/text
- uuid
- json as textual payload

Unsupported target types must fail closed with a structured mapping error.

## Observability and metrics

Every operation shall emit:

- bytes read/written
- rows read/written
- error-row count
- schema inference sample size when used
- elapsed time
- source/target path hash
- policy version

## Error vocabulary

The package shall at minimum surface:

- `TEXTFILE_PATH_NOT_ALLOWED`
- `TEXTFILE_FORMAT_UNSUPPORTED`
- `TEXTFILE_SCHEMA_REQUIRED`
- `TEXTFILE_SCHEMA_MISMATCH`
- `TEXTFILE_PARSE_ERROR`
- `TEXTFILE_ENCODING_INVALID`
- `TEXTFILE_OVERWRITE_ATOMICITY_FAILED`
- `TEXTFILE_WRITE_PARTIAL_FORBIDDEN`

## Cross-section dependencies

- section `13` for text-to-type coercion
- section `14` and section `15` for admitted datatype mapping
- section `20` for diagnostics
- section `23` for table-producing execution semantics
- section `30` for client/tool-facing import-export surfaces where applicable
- section `33` for bounded memory and spill policy
- section `39` for bulk path coordination
