  1. Session Start → Read /PROJECT_CONTEXT.md AND /MGA_RULES.md
  2. After Compaction → Re-read /PROJECT_CONTEXT.md AND /MGA_RULES.md
  3. Before ANY Transaction/Index Work → MUST read /MGA_RULES.md FIRST
  4. Before ANY Emulated Parser Work → MUST read /docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md
  5. Before Major Work → Load relevant specs mentioned in Section 6
  6. Task Complete → Update relevant status in the file

CRITICAL: /MGA_RULES.md contains ABSOLUTE rules for Firebird MGA vs PostgreSQL MVCC.
If these rules are violated, the code is WRONG and must be rewritten.
NO EXCEPTIONS. NO MIXING. Pure Firebird MGA only.

CRITICAL: /docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md contains ABSOLUTE rules
for emulated database parsers (Firebird, PostgreSQL, MySQL, etc.).
- Emulated parsers are COMPLETELY SEPARATE from V2 parser
- DO NOT modify V2 parser for emulation purposes
- Each emulated database has its OWN parser that generates SBLR directly
