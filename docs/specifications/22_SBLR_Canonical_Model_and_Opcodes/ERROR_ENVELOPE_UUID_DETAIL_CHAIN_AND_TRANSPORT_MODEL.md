# Error Envelope UUID Detail Chain and Transport Model

## Purpose
Define the canonical Beta 2 error payload carried by SBLR runtime surfaces,
engine-to-parser IPC, and parser-side diagnostics mapping.

## Canonical Envelope
Every engine-origin or SBLR-origin error sent across the parser boundary shall
use this envelope:

- `correlation_uuid`
- `error_ref_uuid`
- `stable_symbol`
- `status`
- `sqlstate`
- `severity`
- `source_layer`
- `detail_schema_id`
- `detail_items`
- `cause_chain`
- `legacy_vnext_code` or null
- `diagnostic_flags`

The canonical envelope shall not contain client-ready prose.

## Field Definitions

### `correlation_uuid`
- Stable per request.
- Used for audit, tracing, and client-side correlation.

### `error_ref_uuid`
- Primary semantic identity.
- Must resolve to one registry row from section `20`.

### `stable_symbol`
- Convenience label for diagnostics and parser table lookup.
- Must match the registry row for `error_ref_uuid`.

### `status`
- Runtime status class from `core::Status`.

### `sqlstate`
- Default or overridden SQLSTATE.
- Must be exactly five characters when present.

### `severity`
- One of `DEBUG`, `INFO`, `NOTICE`, `WARNING`, `ERROR`, `FATAL`, `PANIC`.

### `source_layer`
- One of:
  - `PARSER_DECODE`
  - `PARSER_PARSE`
  - `PARSER_BIND`
  - `SBLR_VERIFY`
  - `ENGINE_VALIDATION`
  - `ENGINE_EXECUTION`
  - `ENGINE_AUTH`
  - `ENGINE_AUTHZ`
  - `IPC_TRANSPORT`

## Detail Item Encoding
Detail items are carried as ordered slot values, not named text fragments.

```cpp
enum class ErrorDetailType : uint8_t {
    STRING = 1,
    UUID = 2,
    INT64 = 3,
    UINT64 = 4,
    BOOL = 5,
    DECIMAL128 = 6,
    TIMESTAMP = 7,
    IDENTIFIER = 8,
    ENUM_SYMBOL = 9,
    BYTES_HEX = 10,
};

struct ErrorDetailItem {
    uint16_t slot_id;
    ErrorDetailType type;
    uint16_t flags;
    uint32_t value_length;
    // Value bytes follow.
};
```

Rules:

- `slot_id` is relative to the bound detail schema, not globally named text.
- slots may be absent only if the detail schema marks them nullable
- repeated slots are forbidden
- value order in transport shall be ascending by `slot_id`

## Cause Chain Encoding
The cause chain is recursive and carries the same identity model without
re-rendered prose.

```cpp
struct ErrorCauseFrame {
    UUID error_ref_uuid;
    uint32_t status;
    char sqlstate[6];
    uint16_t detail_schema_id;
    uint16_t detail_count;
    // Detail items follow.
};
```

Rules:

- cause depth is limited to `8`
- parsers may choose whether to surface cause-chain prose to clients
- operator tooling shall preserve full cause chain in audit or support output

## IPC Contract Upgrade
`IPCMessageType::ERROR_RESPONSE` remains the message type. The fixed-size
payload is replaced by a versioned payload body.

### Required Payload Shape

```cpp
struct IPCErrorPayloadV2 {
    uint16_t payload_version;      // must be 2
    uint16_t flags;
    UUID error_ref_uuid;
    uint32_t status;
    char sqlstate[6];
    uint8_t severity;
    uint8_t source_layer;
    uint16_t detail_schema_id;
    uint16_t detail_count;
    uint16_t cause_count;
    uint32_t detail_bytes;
    uint32_t cause_bytes;
    uint32_t stable_symbol_length;
    uint32_t legacy_vnext_length;
    // stable_symbol bytes
    // legacy_vnext bytes
    // detail bytes
    // cause bytes
};
```

Rules:

- `payload_version=1` fixed text payload is deprecated and shall not be emitted
  by upgraded engines
- all parser agents admitted after this project shall require version `2`
- any parser that receives an unknown `payload_version` shall fail closed

## SBLR `RAISE` Payload
SBLR raise surfaces shall emit one of two canonical forms:

### Cataloged Engine Error Raise
- carries `error_ref_uuid`
- carries typed detail items
- may override `sqlstate` only when the registry row allows override

### Shared Custom Error Raise
- carries the UUID of the shared custom-error definition
- carries typed parameter values for that custom definition
- parser-side rendering uses the custom definition render template, not raw text

Inline text-only raise payloads are not admitted in Beta 2 execution.

## Native ScratchBird Result Contract
The native parser shall convert the canonical envelope into client-visible text
by:

1. looking up `error_ref_uuid` in the native render pack
2. validating the received detail schema against the render row
3. formatting message, detail, and hint from typed slots
4. returning native ScratchBird result fields

## Donor Parser Result Contract
Each donor parser shall convert the canonical envelope into donor-native output
by:

1. looking up `error_ref_uuid` in the donor map pack
2. applying any slot remap or slot suppression policy
3. selecting donor codes and donor field layout
4. rendering donor-native text from the donor template
5. falling back to the donor generic internal error row if no mapping exists

## Sample Encode Path

```cpp
IPCErrorPayloadWriter writer;
writer.begin(correlation_uuid,
             descriptor.error_ref_uuid,
             descriptor.status,
             descriptor.default_sqlstate,
             Severity::ERROR,
             SourceLayer::ENGINE_EXECUTION,
             descriptor.detail_schema_id);
writer.putSlot(0, ErrorDetailType::IDENTIFIER, "sales.orders");
writer.putSlot(1, ErrorDetailType::IDENTIFIER, "legacy_code");
writer.finish(message.payload);
```

## Sample Decode Path

```cpp
DecodedError err = IPCErrorPayloadReader::decode(message.payload);
const RenderRow& row = donor_map.lookup(err.error_ref_uuid);
DonorErrorFrame out = renderDonorFrame(row, err.detail_items, err.cause_chain);
sendToClient(out);
```

## Verifier Rules
- unknown `error_ref_uuid` fails verification unless it is the reserved
  uncataloged internal row
- mismatched `stable_symbol` and UUID fails verification
- repeated or out-of-order slots fail verification
- invalid slot type for the bound schema fails verification
- invalid cause depth fails verification
- `sqlstate` values of non-length `5` fail verification

## Refusal Rules
- a parser shall not use legacy text in transport as the mapping key
- a parser shall not accept an error payload without UUID identity
- SBLR shall not carry free-form client prose for cataloged engine errors
