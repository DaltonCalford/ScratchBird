# Type Family: JSON, Vector, And Search
Last modified: 2026-02-19

Back links:
- [Type Families README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Temporal](temporal.md)
- Next: [Network, Geo, And Range](network-geo-range.md)

## Parser-Accepted Names
- JSON/search/vector: `JSON`, `JSONB`, `JSONPATH`, `TSVECTOR`, `TSQUERY`, `VECTOR`.

## Operator Coverage
- JSON extract operators closed: `->`, `->>`, `#>`, `#>>`.
- JSON existence operators parse distinct forms (`?`, `?|`, `?&`) but `?|` and `?&` remain collapsed to generic exists opcode in emitter.
- Array/collection operators closed with structural semantics: `@>`, `<@`, `&&`.

## Function Coverage Highlights
- JSON helper opcodes include extraction/object/array/set/remove families.
- Vector/query extension surfaces are parsed with feature gates; runtime closure for advanced NoSQL/vector bridges remains partial.
