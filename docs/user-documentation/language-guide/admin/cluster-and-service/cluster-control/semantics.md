# Admin CLUSTER CONTROL COMMANDS: Semantics
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Syntax](syntax.md)
- Next: [Examples](examples.md)

## Coverage
- Status: Partial

## Form
~~~sql
CREATE/ALTER/DROP CLUSTER WORKLOAD ...; CLUSTER SET STATE ...; SHOW CLUSTER ...;
~~~

## Notes
- Details: Cluster object and show command families are explicit parser surfaces.
- Runtime note: Current runtime routes cluster opcode families through vNext semantic bridge in 0.1.0.
- Error/contract note: Bridge path returns deterministic BRG_0406 until explicit handlers are implemented.
- Usage rationale: Workload routing and admission policy control plane for cluster deployments.
