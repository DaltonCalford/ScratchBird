# Section 05 Dependencies

## Upstream dependencies

- section `03` for allocation and free-space lifecycle
- section `04` for page-size policy
- section `07` for bootstrap and catalog fixed-page expectations

## Downstream dependents

- section `08` for startup and transaction-adjacent restart behavior
- section `10` for sweep, audit, and reclaim behavior
- section `18` for index-family integration
- section `20` for observability and repair reporting
- section `31` for format, compatibility, and integrity gate coverage

## Contract split

Section `05` owns page-image structure and legality.
Adjacent sections own allocator, publication, recovery, sweep, and observability behavior that consume those page-image rules.

## Non-ownership

Section `05` does not own allocator policy, transaction visibility, sweep policy, or observability policy. It owns only the durable page contract those subsystems consume.
