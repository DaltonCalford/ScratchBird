# Subsystem Boundary and Ownership Matrix

This file owns the architectural ownership and seam-arbitration model for ScratchBird subsystems.

## Canonical ownership matrix

| Subsystem | Primary ownership | Boundary label | What section 32 owns | What section 32 does not own |
| --- | --- | --- | --- | --- |
| core engine runtime | existing engine canonical sections | hard_boundary | the fact that this is a distinct subsystem and where its outer boundaries begin | storage, transaction, type, execution, or recovery local detail |
| parser/front-door | sections 21 and 28 | hard_boundary | the rule that parser/front-door is outside core engine semantic ownership | parser feature lists, lowering details, or dialect-local semantics |
| listener/server orchestration | section 29 | shared_boundary | the top-level relationship between listener orchestration and engine/runtime surfaces | listener control-plane specifics or session-local message details |
| native wire and handshake | sections 26 and 27 | shared_boundary | where transport/session entry sits in the architecture graph | full protocol grammar or handshake-state local detail |
| client/tooling | section 30 | hard_boundary | the rule that client/tooling is an external control/API surface | driver/tool semantics and language-lane local detail |
| conformance/gates | section 31 | adjacent_boundary | the role gates play in proving or bounding subsystem behavior | local subsystem truth itself |
| security/auth/plugin surfaces | section 19 and adjacent sections | shared_boundary | the attachment point of auth/security/extensibility in the architecture map | low-level auth plugin or key-management semantics |

## Dependency-direction matrix

| From subsystem | Allowed dependency direction | Architectural reason | Refusal rule |
| --- | --- | --- | --- |
| parser/front-door | inward to shared internal execution forms only | parser compiles user syntax into engine-consumable internal forms | parser must not redefine storage or transaction semantics |
| client/tooling | inward through public/bounded protocol or control surfaces only | external tooling must not bypass owned entry contracts | client/tooling must not claim engine-internal ownership |
| listener/server orchestration | inward to runtime/session and protocol-owned surfaces | listener manages process/session routing, not core semantics | listener must not silently absorb protocol or storage truth |
| conformance/gates | read-only proof relationship to lower sections | gates prove or bound behavior without becoming behavior | gate language must not replace local section truth |
| security/auth/extensibility | attach through explicit seam ownership only | security and extension surfaces constrain entry and execution boundaries | no implicit widening into general plugin ABI |

## Seam-arbitration algorithm

1. Identify the subsystem where the local semantic rule originates.
2. If the change alters only local semantics, that owning section changes first.
3. If the change alters a cross-section handoff, section 32 must be updated to restate the seam and dependency rule.
4. If the change alters transport, entry, or orchestration behavior, the protocol/handshake/listener sections remain the detailed owners and section 32 restates only the architectural boundary.
5. If a change would blur a hard boundary, the change is rejected until a new explicit ownership decision is made.

## Boundary rules

1. Hard boundaries prevent ownership bleed. Section 32 may summarize them but must not absorb their detailed semantics.
2. Shared boundaries require explicit seam language. Where two sections touch, section 32 owns the seam description, not the local internals.
3. Adjacent boundaries mark proof or governance surfaces that constrain other sections without replacing them.
4. A section may depend on a neighboring section's proof, but it may not silently inherit that section's detailed semantics.

## Architecture leak checks

A boundary is architecturally unsafe if any of the following occurs:
- parser behavior is described as core engine truth
- client/tooling behavior is described as engine-local truth
- internal transport/control-plane detail is described as a stable public API without an owning contract section
- gate language is treated as if it were local subsystem behavior rather than proof/boundary language
- a hard-boundary section starts claiming another section's algorithmic detail without an explicit ownership transfer
