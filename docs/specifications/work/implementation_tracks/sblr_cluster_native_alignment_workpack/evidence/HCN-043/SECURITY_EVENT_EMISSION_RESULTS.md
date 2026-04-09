# Security Event Emission Results

Validated by `StructuredEventStreamTest.EmitsDeterministicEventIdsAndEpochContext`:
- emitted events include required epoch context values.
- emitted IDs are deterministic (`evt-1`, `evt-2`, ...).
- JSON-lines payload includes canonical security/event metadata fields.

Validated by `StructuredEventStreamTest.RejectsMissingEpochOrInvalidPayload`:
- invalid structured events are rejected before emission.
