# Admin SET SCHEMA AND CURRENT_SCHEMA: Semantics
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Syntax](syntax.md)
- Next: [Examples](examples.md)

## Coverage
- Status: Supported

## Form
~~~sql
SET SCHEMA [=|TO] <schema_path|DEFAULT>; SET CURRENT_SCHEMA [=|TO] <schema_path|DEFAULT>;
~~~

## Notes
- Details: Schema path commands are explicitly parsed and update session schema context.
- Runtime note: Current schema context is applied by connection/executor context for name resolution.
- Error/contract note: Invalid schema path values are rejected deterministically.
- Usage rationale: Controls search-path style resolution and home schema behavior.
