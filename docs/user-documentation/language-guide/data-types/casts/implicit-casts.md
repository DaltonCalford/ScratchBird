# Implicit Conversion
Last modified: 2026-02-19

Back links:
- [Casts README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Explicit Casts](explicit-casts.md)
- Next: [Strict Mode](strict-mode.md)

Implemented implicit coercion behavior (policy controlled):
- text-to-numeric coercion for eligible arithmetic operator families
- text-to-temporal coercion for eligible temporal paths
- text-to-comparison coercion for supported comparison families

Known partial/unsupported combinations:
- some implicit comparison combinations remain unsupported
- wide numeric support is partial for `DIV`, modulo, and bitwise families

Examples:
~~~sql
SELECT '10' + 5;
SELECT '42' = 42;
~~~
