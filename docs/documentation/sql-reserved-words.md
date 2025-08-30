### Reserved Words and Keywords

The lexer checks a generated table `include/scratchbird/engine/keywords_generated.h` (if present). In this tree, keywords fall back to a built-in set in `src/engine/lexer.cpp`.

Fallback keywords (verbatim):

SELECT, FROM, WHERE, GROUP, HAVING, ORDER, BY, JOIN, LEFT, RIGHT, FULL, CROSS, NATURAL, ON, USING, LATERAL, AS, WITH, RECURSIVE, UNION, ALL, INTERSECT, EXCEPT, DISTINCT, OVER, PARTITION, ROWS, RANGE, FIRST, SKIP, FETCH, OFFSET, PLAN, INSERT, INTO, VALUES, UPDATE, SET, DELETE, DEFAULT, CREATE, ALTER, DROP, TABLE, INDEX, CONSTRAINT, TRIGGER, PRIMARY, KEY, FOREIGN, REFERENCES, UNIQUE, CHECK, NOT, NULL, DEFERRABLE, INITIALLY, IMMEDIATE, DEFERRED, BEFORE, AFTER, FOR, EACH, ROW, STATEMENT, WHEN, ACTIVE, INACTIVE, AND, OR, IN, EXISTS, BETWEEN, LIKE, IS, TRUE, FALSE.

Identifiers: unquoted identifiers remain as typed; quoted identifiers preserve case and can include otherwise invalid characters.

Code anchors: `src/engine/lexer.cpp` (kFallbackKeywords)

