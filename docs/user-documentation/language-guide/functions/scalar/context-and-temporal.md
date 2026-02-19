# Scalar Functions: Context And Temporal
Last modified: 2026-02-19

Back links:
- [Scalar README](README.md)
- [Functions README](../README.md)

Series navigation:
- Previous: [String, JSON, And Conversion](string-json-conversion.md)

Context/time function forms:
- `NOW`
- `CURRENT_TIMESTAMP`
- `CURRENT_DATE`
- `CURRENT_TIME`
- `CURRENT_USER`
- `SESSION_USER`
- `CURRENT_ROLE`
- `CURRENT_CONNECTION`
- `CURRENT_SESSION`
- `CURRENT_TRANSACTION`

Important semantic distinction:
- `CURRENT_TIMESTAMP`: transaction start timestamp semantics when a transaction exists
- `NOW`: current wall-clock timestamp at evaluation time
