# Object Naming and Quoting

## Current code-backed truth
- The lexer and schema-path layers are real.
- The gatekeeper-keyword model is designed to maximize identifier flexibility and minimize globally reserved words.
- Quoted and unquoted identifier handling belongs to the parser and lexer boundary, not to executor semantics.

## Proven anchors
- `lexer_v3.h`
- `schema_path_v3.h`
- `parser_v3.h`

## Boundary
- The broad cross-dialect quoting matrix is still partial.
- Treat this file as a bounded parser naming-contract document until later proof closes exact donor-compatibility details.
