# SBLR Canonical Serialization and Container

Status: current_authority

## Container family

The current canonical container family is SBL3. All current implementation work must target the v3 container model used by the shared ScratchBird parser and compiler path.

## Serialization rules

- Serialization is deterministic for logically identical input.
- Field encoding is little-endian unless a field-specific rule states otherwise.
- Singleton sections must not appear more than once.
- Required sections must appear exactly once.
- Optional sections may be omitted only when the corresponding feature is absent.
- Unknown required section identifiers are fatal verifier errors.
- Unknown optional extension sections are fatal unless the compatibility manifest explicitly marks them ignorable for the current reader version.

## Canonical section order

1. container header and compatibility manifest
2. constant and symbol pools
3. statement inventory and payload blocks
4. domain payload blocks
5. optional diagnostics or renderer-assist blocks

## Determinism requirements

- Constant-pool ordering must be stable for equal logical input.
- Statement ordering follows parser-emitted logical statement order.
- Symbol and object references must preserve stable index assignment within a single container.
- Equivalent parser trees must not produce semantically divergent SBLR because of incidental parser traversal order.

## Durable identity rule

Durable catalog objects carried in SBLR must be represented by canonical resolved identity. Raw names may be retained only as renderer-assist or diagnostics fields; they are not execution authority when UUID-bound identity is required.
