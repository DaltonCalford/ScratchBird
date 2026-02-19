# Operators: Pattern, JSON, And Array
Last modified: 2026-02-19

Back links:
- [Operators README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Logical And Bitwise](logical-bitwise.md)

Pattern and special predicate operators:
- `LIKE`, `ILIKE`, regex (`~`, `~*`, `!~`, `!~*`)
- `STARTING WITH`, `CONTAINING`

JSON and collection operators:
- closed: `->`, `->>`, `#>`, `#>>`, `@>`, `<@`, `&&`, `||`
- partial semantics: `?`, `?|`, `?&` collapse into generic JSON exists opcode family

Examples:
~~~sql
SELECT payload->'user', payload#>>'{user,id}' FROM events;
SELECT tags @> ARRAY['beta'] FROM releases;
~~~
