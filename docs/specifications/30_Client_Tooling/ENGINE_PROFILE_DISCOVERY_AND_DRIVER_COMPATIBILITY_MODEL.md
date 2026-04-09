# Engine Profile Discovery and Driver Compatibility Model

## Purpose

Define how drivers and operator tooling consume the engine profile contract.

## Normative Rules

1. Drivers and tooling shall treat the engine profile as the primary compatibility authority.
2. Listener port, emulation family, proxy mode, or parser dialect mode do not by themselves authorize feature enablement.
3. Drivers shall enable only the behavior explicitly allowed for the discovered engine profile.
4. Unknown or unsupported profile identities are a refusal condition, not a heuristic downgrade condition.

## Compatibility Classes

Every driver or tool feature shall classify the current profile into one of the following states:

- `EXACT_SUPPORTED`
- `SUPPORTED_WITH_BOUNDED_DIFFERENCE`
- `INSPECT_ONLY`
- `REFUSED`

The classification shall be deterministic and table-driven.

## Discovery Contract

Profile discovery shall yield, directly or through a stable compatibility surface:

- engine profile identity
- contract version
- profile family
- optional capability flags needed by the consumer

If the consumer cannot obtain this data with enough certainty to classify the connection, it shall refuse the profile-sensitive feature.

## Reattach and Restart

When a connection is detached, dormant, or rebound after process restart, the client shall not assume that the previous profile contract remains authoritative. Any profile-sensitive state shall be revalidated before reuse.

## Manager, Listener, and Engine Boundary

The optional manager and listener may expose dialect-facing attachment or routing surfaces, but the engine profile remains the authoritative contract for:

- runtime capability enablement
- callable-surface enablement
- optional runtime admission
- advanced feature activation

## Fail-Closed Examples

Drivers and tools shall refuse or reduce capability when:

- the engine profile is unknown
- the contract version is newer than the local compatibility table
- the profile family is known but the required capability flag is absent
- a dormant or restarted connection cannot re-establish prior compatibility state safely

## Certification Implication

Driver compatibility is not complete until compatibility tables, public profile exposure, and refusal behavior are frozen under the section `31` certification model.
