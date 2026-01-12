  1. Session Start → Read /PROJECT_CONTEXT.md AND /docs/ARCHITECTURAL_LAYERS.md AND /MGA_RULES.md AND /IMPLEMENTATION_STANDARDS.md
  2. After Compaction → Re-read /PROJECT_CONTEXT.md AND /docs/ARCHITECTURAL_LAYERS.md AND /MGA_RULES.md AND /IMPLEMENTATION_STANDARDS.md
  3. Before ANY Transaction/Index Work → MUST read /MGA_RULES.md FIRST
  4. Before ANY Parser/API/Wire Protocol Work → MUST read /docs/ARCHITECTURAL_LAYERS.md FIRST
  5. Before ANY Emulated Parser Work → MUST read /docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md
  6. Before ANY Implementation Work → MUST read /IMPLEMENTATION_STANDARDS.md and run foundation audit
  7. Before Major Work → Load relevant specs mentioned in Section 6
  8. Before Marking ANY Task Complete → MUST verify against /COMPLETION_VERIFICATION_CHECKLIST.md
  9. Task Complete → Update relevant status in the file ONLY after providing all required evidence

CRITICAL: /docs/ARCHITECTURAL_LAYERS.md defines WHERE work belongs in the architecture.
If you're confused about where to implement something, READ THIS FIRST.
- REQUEST: Client → Wire decode → Parser (SQL→SBLR) → SBLR → Server Core
- RESPONSE: Server Core → SBLR → Parser (native→dialect format) → Wire encode → Client
- Parser Layer is BIDIRECTIONAL: handles both SQL→SBLR AND result formatting
- ALL dialect-specific APIs are surfaced in the Parser Layer
- Parsers map dialect APIs to native SBLR bytecode (request path)
- Parsers convert native results to dialect-specific format (response path)
- Server Core is 100% dialect-agnostic (only understands SBLR)
- Wire Protocol Layer only handles encoding/decoding (NO parsing, NO result formatting)
- If you need an API surfaced OR results formatted, it goes in the Parser Layer

CRITICAL: /MGA_RULES.md contains ABSOLUTE rules for Firebird MGA vs PostgreSQL MVCC.
If these rules are violated, the code is WRONG and must be rewritten.
NO EXCEPTIONS. NO MIXING. Pure Firebird MGA only.

CRITICAL: /docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md contains ABSOLUTE rules
for emulated database parsers (Firebird, PostgreSQL, MySQL, etc.).
- Emulated parsers are COMPLETELY SEPARATE from V2 parser
- DO NOT modify V2 parser for emulation purposes
- Each emulated database has its OWN parser that generates SBLR directly

CRITICAL: /IMPLEMENTATION_STANDARDS.md contains ABSOLUTE requirements for all implementation work.
If these standards are violated, the work is WRONG and must be redone.
NO EXCEPTIONS. NO SHORTCUTS. Evidence required for everything.
- MUST verify catalog infrastructure exists BEFORE implementing features
- MUST include restart/persistence tests for ALL features
- MUST include negative/error tests for ALL features
- MUST test all code paths (not just executor)
- MUST provide evidence before marking anything complete
- Work that bypasses these standards is as wrong as violating MGA_RULES.md
