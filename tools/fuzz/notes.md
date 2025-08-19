Phase H — Fuzz and Performance Notes

Seeds
- DDL: CREATE TABLE/ALTER TABLE/VIEW/SEQUENCE/DOMAIN/GRANT
- PSQL: EXECUTE BLOCK/PROCEDURE/TRIGGER

Fuzz method
- Deterministic RNG (mt19937, seed 1234567)
- Simple token mutations: insert parentheses/commas/quotes, delete chars, inject comments, add INTO clauses
- 200 mutations per seed in fuzz_smoke

Bench method
- Fixed corpus of ~7 statements, 2000 iterations, wall-clock ms

Outcomes
- fuzz_smoke: zero crashes; warnings collected via helper with spans
- parser_bench_smoke: time printed to stdout; visually compare between runs

Future
- Expand seeds from Firebird corpus; wire to sanitizer builds in CI for deeper coverage
