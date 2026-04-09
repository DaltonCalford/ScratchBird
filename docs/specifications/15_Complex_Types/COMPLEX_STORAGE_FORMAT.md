# Complex Storage Format

Status: current_authority

## Current authoritative storage surface

The current authoritative complex-value storage surface is:
- TypedValue serializePlainValue
- TypedValue deserializePlainValue

## Complex payload capability matrix

| Family | Current authority | Main boundary |
| --- | --- | --- |
| ARRAY | supported internal payload with dimension header, null bitmap, and nested element payloads | donor-engine parity is not claimed |
| LIST | supported internal payload through the same array-like carrier path | donor-engine parity is not claimed |
| MAP | bounded internal payload through the same array-like carrier path | separate durable key/value framing is not yet closed |
| COMPOSITE | supported internal payload with SBC1 metadata, typed fields, and optional field names | donor-engine parity is not claimed |
| ROW | supported internal payload through the composite path | donor-engine parity is not claimed |
| VARIANT | supported internal payload through composite-style framing with exactly-one-value enforcement and optional tag metadata | not a universal tagged-union contract |
| JSONB | bounded binary-backed internal payload | PostgreSQL jsonb compatibility is not claimed |
| BSON | bounded binary-backed internal payload | external BSON compatibility is not claimed |
| TAGGED_UNION | bounded binary-backed internal payload with fail-closed validation | broader section-wide tagged-union semantics remain bounded |
| DICT_ENCODED | bounded binary-backed internal payload with fail-closed validation | broader dictionary semantics remain bounded |

## TOAST eligibility boundary

TypeSystem currently treats these complex families as TOAST-eligible:
- ARRAY
- LIST
- MAP
- COMPOSITE
- ROW
- VARIANT
- TAGGED_UNION
- DICT_ENCODED
- COMPLETION_FIELD
- PREFIX_SEARCH_FIELD
- FLAT_OBJECT

## Explicit non-guarantees

This section does not currently prove:
- PostgreSQL wire or binary compatibility for arrays or composites
- exact standalone durable payload contracts for SET, ENUM, or every domain-wrapped or emulated complex family
- the older exact EWKB, JSONB, BSON, vector, map, list, or set storage claims from historical prose
