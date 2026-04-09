# Engine Authentication Hardening and Manager Option Specification

Status: current_authority

## Hardening rules

- unknown authentication manager options are fatal configuration errors
- ambiguous overlapping method selection is rejected
- security-critical defaults must prefer refusal over permissive fallback
- authentication must complete before engine session attach succeeds
- downgrade from a stronger configured method to a weaker unconfigured method is forbidden
- security failures must produce auditable incident classes
- authenticated management or proxy entry does not widen privilege beyond the
  explicitly granted operator role
- derivative retry, quarantine release, shadow promotion, and failback
  inspection require explicit post-auth authorization checks
- a successful authentication result must not be treated as authorization for
  route-changing or derivative-mutating operations by itself
- cluster-managed remote administrative change requires an explicit
  management-plane privilege decision separate from transport authentication
- remote changes to plugins, authentication, security policy, memory budgets,
  or other promoted administrative settings require a durable audit identity,
  target scope, and instruction identity

## Transaction rule

Because ScratchBird is always in a transaction, successful authentication enters a live transaction context for subsequent command processing. Authentication does not create a non-transactional execution mode.

## Authorization boundary rule

Authorization for derivative and shadow administrative operations is evaluated
after authentication and before the requested action is bound into command
execution. Management or proxy transport mode does not bypass this rule.

The same rule applies to cluster-managed remote administrative operations.

## Required reconstructed cluster-management security rule

The cluster layer may remotely add, remove, or alter promoted administrative
settings only through a privileged management transaction that is:
- authenticated
- explicitly authorized for the requested target scope
- durably audited
- bound into the engine-owned admin path before local apply

Neither the optional manager nor the listener may widen privilege on behalf of
the cluster layer.

Successful transport authentication proves identity only.
It does not authorize:
- plugin mutation
- authentication policy mutation
- security policy mutation
- memory-budget mutation
- listener-topology mutation
- parser-pool policy mutation

Those remain separate privileged management actions.
