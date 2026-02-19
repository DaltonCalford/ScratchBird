# Scalar Functions: String, JSON, And Conversion
Last modified: 2026-02-19

Back links:
- [Scalar README](README.md)
- [Functions README](../README.md)

Series navigation:
- Previous: [Math And Trig](math-and-trig.md)
- Next: [Context And Temporal](context-and-temporal.md)

Representative emitter-mapped scalar families:
- String/text: `CONCAT`, `REPLACE`, `ENDS_WITH`
- JSON: `JSON_EXTRACT`, `JSON_EXISTS`, `JSON_HAS_KEY`, `JSON_OBJECT`, `JSON_ARRAY`, `JSON_SET`, `JSON_INSERT`, `JSON_REMOVE`
- Conversion/formatting: `TO_CHAR`, `TO_DATE`, `TO_TIMESTAMP`, `LEAST`, `GREATEST`
- Array helpers: `ARRAY_POSITION`, `ARRAY_SLICE`, `ARRAY_SUBSCRIPT`

Coverage note:
- parser accepts generic function calls with validated argument list structures
- runtime closure varies by function; use audits/tests for per-function executor closure
