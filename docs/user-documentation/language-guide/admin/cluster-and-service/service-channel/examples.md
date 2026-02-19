# Admin SERVICE CHANNEL COMMANDS: Examples
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Semantics](semantics.md)
- Next: [Runtime](runtime.md)

## Coverage
- Status: Partial

## Form
~~~sql
SERVICE CHANNEL <action> ...;
~~~

## Notes
- Details: Service channel command family is explicitly parsed/emitted.
- Runtime note: Runtime semantic handlers are partial and remain bridge-routed in 0.1.0.
- Error/contract note: Unsupported actions fail deterministic bridge error contracts.
- Usage rationale: Service-plane integration and channel orchestration controls.
