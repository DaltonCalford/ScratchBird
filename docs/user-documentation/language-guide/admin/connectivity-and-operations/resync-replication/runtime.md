# Admin RESYNC REPLICATION CHANNEL: Runtime
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Partial

## Form
~~~sql
RESYNC REPLICATION CHANNEL <channel_name>;
~~~

## Notes
- Details: RESYNC command is explicit and normalized through replication system-key contract.
- Runtime note: Runtime command is parsed/emitted but semantic bridge closure is partial.
- Error/contract note: Invalid channel identifiers fail deterministic contract checks.
- Usage rationale: Manual replication convergence after channel disruption.
