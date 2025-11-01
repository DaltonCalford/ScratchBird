  1. Session Start → Read /PROJECT_CONTEXT.md AND /MGA_RULES.md
  2. After Compaction → Re-read /PROJECT_CONTEXT.md AND /MGA_RULES.md
  3. Before ANY Transaction/Index Work → MUST read /MGA_RULES.md FIRST
  4. Before Major Work → Load relevant specs mentioned in Section 6
  5. Task Complete → Update relevant status in the file

CRITICAL: /MGA_RULES.md contains ABSOLUTE rules for Firebird MGA vs PostgreSQL MVCC.
If these rules are violated, the code is WRONG and must be rewritten.
NO EXCEPTIONS. NO MIXING. Pure Firebird MGA only.
