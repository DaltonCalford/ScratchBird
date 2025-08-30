### Reserved Words and Keywords

What it is
- The set of words recognized as keywords by the lexer (either generated table, or the fallback set in this tree).

Why it matters
- Keywords influence parsing. Knowing them helps you decide when to quote identifiers and avoid ambiguous names.

How to use it
- Prefer non-keyword identifiers; quote with "..." as needed. Consult the list below for the active fallback set.

The lexer checks a generated table `include/scratchbird/engine/keywords_generated.h` (if present). In this tree, keywords fall back to a built-in set in `src/engine/lexer.cpp`.

Fallback keywords (verbatim):

SELECT, FROM, WHERE, GROUP, HAVING, ORDER, BY, JOIN, LEFT, RIGHT, FULL, CROSS, NATURAL, ON, USING, LATERAL, AS, WITH, RECURSIVE, UNION, ALL, INTERSECT, EXCEPT, DISTINCT, OVER, PARTITION, ROWS, RANGE, FIRST, SKIP, FETCH, OFFSET, PLAN, INSERT, INTO, VALUES, UPDATE, SET, DELETE, DEFAULT, CREATE, ALTER, DROP, TABLE, INDEX, CONSTRAINT, TRIGGER, PRIMARY, KEY, FOREIGN, REFERENCES, UNIQUE, CHECK, NOT, NULL, DEFERRABLE, INITIALLY, IMMEDIATE, DEFERRED, BEFORE, AFTER, FOR, EACH, ROW, STATEMENT, WHEN, ACTIVE, INACTIVE, AND, OR, IN, EXISTS, BETWEEN, LIKE, IS, TRUE, FALSE.

Identifiers: unquoted identifiers remain as typed; quoted identifiers preserve case and can include otherwise invalid characters.

Code anchors: `src/engine/lexer.cpp` (kFallbackKeywords)

See also
- [Lexical](./sql-lexical.md) · [Operators](./sql-operators.md)

