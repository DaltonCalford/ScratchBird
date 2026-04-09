# Reserved Words and Keywords

## Current code-backed truth
- The gatekeeper lexer model in `lexer_v3.h` is the primary authority for globally reserved V3 keywords.
- Many historical SQL words are intentionally contextual and are resolved by the parser, not reserved by the lexer.

## Boundary
- This file is authoritative for the V3 lexer model.
- It is not a donor-exact or universally reserved keyword registry for every compatibility dialect.
- Any broader keyword inventory must remain bounded to actual lexer and parser proof.
