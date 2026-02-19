# Admin CREATE DATABASE EMULATED: Syntax
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Topic README](README.md)
- Next: [Semantics](semantics.md)

## Coverage
- Status: Partial

## Form
~~~sql
CREATE DATABASE EMULATED <engine> ON SERVER <server_name> '<remote_path>' WITH OPTIONS (...) ALIAS <alias_name>;
~~~

## Notes
- Details: Parser captures full emulated-database contract and options.
- Runtime note: Runtime routing for this family still spans mixed opcode paths and needs normalization in 0.1.0.
- Error/contract note: Contract checks validate source spec and option structure.
- Usage rationale: Registers external emulated-engine mapping as native v3 database construct.
