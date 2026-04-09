# Engine Error Emission and Registry Binding Model

## Purpose
Define how compiler, executor, storage, IPC server, and runtime support code
emit canonical registry-bound errors instead of inline human-readable strings.

## Runtime Objects

### `EngineErrorDescriptor`
Compile-time generated descriptor:

- `error_ref_uuid`
- `stable_symbol`
- `status`
- `default_sqlstate`
- `severity`
- `detail_schema_id`

### `EngineErrorRecord`
Runtime emitted object:

- descriptor fields
- populated detail vector
- cause chain
- `correlation_uuid`
- source file, line, and function for operator-only diagnostics

## `ErrorContext` Compatibility Contract
`ErrorContext` remains the transitional carrier but its client-facing text role
is removed for cataloged errors.

Required fields after cutover:

- `code`
- `sqlstate`
- `sqlstate_text`
- `vnext_code` as legacy secondary
- `message` only for uncataloged internal fallback or non-client operator trace
- `error_ref_uuid`
- `stable_symbol`
- `detail_schema_id`
- `detail_items`
- `cause`

## Emission Rules

### Core Rule
Every cataloged failure path shall:

1. bind one generated descriptor
2. populate typed detail slots only
3. attach cause chain if a child error exists
4. emit the record without composing client prose

### Forbidden Rule
Cataloged failures shall not:

- call `ctx->set(status, literal_text, ...)`
- construct `ExecutionResult(literal_text)` for client-facing failures
- throw `ExecutionStatusException(status, sqlstate, literal_text)` for
  cataloged engine failures
- embed donor-native error text in engine layers

## Executor Raise Rules

### Engine-Origin Failures
Storage, catalog, optimizer, security, and execution failures shall emit one
registry-bound record.

### User-Defined Raise
If a stored program or SBLR instruction raises a shared custom error, the
executor shall bind the UUID of that custom error definition and carry only its
parameter slots.

### Non-Cataloged Panic
Unexpected exceptions shall collapse to the reserved internal error row and
carry a text digest plus operator trace fields only.

## Emission Flow

### Storage Path
1. storage routine detects semantic failure
2. routine selects generated descriptor
3. routine populates typed slots
4. routine returns `EngineErrorRecord`
5. upper layer attaches correlation UUID and any cause frame
6. IPC server serializes the canonical payload

### Compiler or Verifier Path
1. compiler or verifier selects error descriptor
2. source span and symbol slots are populated as typed detail items
3. parser or IPC boundary receives canonical payload
4. parser renders client text from its render pack

### Auth or AuthZ Path
1. security layer selects canonical identity such as invalid password,
   insufficient privilege, or expired credential
2. identity-bearing slots are populated with redaction flags
3. parser family emits donor-visible auth code and text

## Generated API Example

```cpp
namespace scratchbird::core::errors {
    inline constexpr EngineErrorDescriptor kCatalogUndefinedTable{
        UUID::fromString("6c9f8e0e-d3fd-5d67-b399-d5b0b18cc4cf"),
        "SBERR_CATALOG_UNDEFINED_TABLE",
        Status::UNDEFINED_TABLE,
        "42P01",
        Severity::ERROR,
        17,
    };
}

auto CatalogManager::requireTable(UUID schema_uuid,
                                  std::string_view table_name,
                                  ErrorContext* ctx) -> Status
{
    if (!exists(schema_uuid, table_name)) {
        ErrorBuilder b(errors::kCatalogUndefinedTable);
        b.setIdentifier(0, renderSchemaPath(schema_uuid));
        b.setIdentifier(1, table_name);
        return b.finish(ctx);
    }
    return Status::OK;
}
```

## IPC Send Rule
The engine-side IPC sender shall serialize `EngineErrorRecord` directly into the
versioned error payload from section `22`. It shall not render client text.

## Logging Rule
Structured logging and audit events shall record:

- `correlation_uuid`
- `error_ref_uuid`
- `stable_symbol`
- `status`
- `sqlstate`
- populated detail slots after redaction policy
- cause-chain UUIDs
- operator trace fields

Human-readable text in logs shall be produced by the shared render-pack library
from the canonical payload, not by engine-side literal composition.

## Static Text Eradication Gate
The build shall fail if cataloged engine code introduces new client-visible
literal text through:

- `ctx->set(` with a string literal
- `SET_ERROR_CONTEXT(` with a string literal
- `ExecutionResult(` with a string literal in a cataloged failure path
- `sendError(` with literal client text in engine-side IPC handlers

Allowlisted exceptions:

- test fixtures that explicitly validate the migration gate
- startup bootstrap fatal logs emitted before parser libraries are available
- the reserved uncataloged internal failure path

## Migration of Existing Macros
`SET_ERROR_CONTEXT` and `SET_ERROR_CONTEXT_VNEXT` shall be replaced by registry
macros.

Required replacement family:

- `SET_ERROR_REF(ctx, descriptor)`
- `SET_ERROR_REF_1(ctx, descriptor, slot0)`
- `SET_ERROR_REF_2(ctx, descriptor, slot0, slot1)`
- `SET_ERROR_REF_CAUSE(ctx, descriptor, cause_record)`

## Refusal Rules
- executor layers shall not synthesize donor-native codes
- engine layers shall not use parser render packs directly on the main data path
- registry-bound errors shall not lose `error_ref_uuid` during wrapping or
  rethrow
- cause frames shall not overwrite parent identity
