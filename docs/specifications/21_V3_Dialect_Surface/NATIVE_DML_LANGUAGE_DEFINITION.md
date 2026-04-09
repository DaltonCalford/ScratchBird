# Native DML Language Definition

## Current code-backed truth
- Real parser entry points exist for `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `MERGE`, `COPY`, `WITH`, prepared-statement surfaces, and utility-adjacent DML entry paths.
- Real emit or lower paths exist for select, insert, update, delete, merge, and copy statements.
- The native inet suite proves listener-path parser coverage for a real statement corpus.

## Proven anchors
- `include/scratchbird/parser/parser_v3.h`
- `include/scratchbird/parser/v3_emitter.h`
- `include/scratchbird/sblr/ast_sblr_lowerer.h`
- `tests/conformance/v3_native_inet`

## Boundary
- This file no longer claims that every historical DML expansion or compatibility alias is fully closed.
- Advanced compatibility forms, positioned mutation, generated-key behavior, and statement-list sequencing remain partial until re-audited against current parser and listener tests.
- Treat DML grammar and lowering authority as real; treat full parity matrices as not yet closed.
