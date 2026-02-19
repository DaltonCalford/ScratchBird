# Explicit Casts
Last modified: 2026-02-19

Back links:
- [Casts README](README.md)
- [Data Types README](../README.md)

Next in series:
- [Implicit Conversion](implicit-casts.md)

Explicit cast forms accepted in native v3:
- `CAST(expr AS type [USING format])`
- `expr::type`

Executor-recognized `USING` formats:
- `HEX`
- `BASE64`
- `ESCAPE`

Examples:
~~~sql
SELECT CAST('2026-02-19' AS DATE);
SELECT '7f'::BYTEA;
~~~
