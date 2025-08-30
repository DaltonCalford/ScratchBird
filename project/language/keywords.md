## Keywords

Keywords are recognized by the lexer from a generated table (`include/scratchbird/engine/keywords_generated.h`) or a fallback list when the table is empty. The current fallback set includes:

SELECT, FROM, WHERE, GROUP, HAVING, ORDER, BY, JOIN, LEFT, RIGHT, FULL, CROSS, NATURAL, ON, USING, LATERAL, AS, WITH, RECURSIVE, UNION, ALL, INTERSECT, EXCEPT, DISTINCT, OVER, PARTITION, ROWS, RANGE, FIRST, SKIP, FETCH, OFFSET, PLAN, INSERT, INTO, VALUES, UPDATE, SET, DELETE, DEFAULT, CREATE, ALTER, DROP, TABLE, INDEX, CONSTRAINT, TRIGGER, PRIMARY, KEY, FOREIGN, REFERENCES, UNIQUE, CHECK, NOT, NULL, DEFERRABLE, INITIALLY, IMMEDIATE, DEFERRED, BEFORE, AFTER, FOR, EACH, ROW, STATEMENT, WHEN, ACTIVE, INACTIVE, AND, OR, IN, EXISTS, BETWEEN, LIKE, IS, TRUE, FALSE.

### Implementation References
- Lexer: `src/engine/lexer.cpp` (kFallbackKeywords, SCRATCHBIRD_KEYWORDS lookup)
- Generated table: `include/scratchbird/engine/keywords_generated.h`

