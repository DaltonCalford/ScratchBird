# Cross-Section Precedence and Contradiction Resolution Rule

## Scope

This file defines how contradictions are resolved across the canonical
specification tree during the rebuild stage.

## Governing rule

If two sections appear to disagree, the contradiction must be resolved by
explicit precedence, not by implementer inference.

## Precedence order

Unless a section explicitly states a narrower override, precedence is:

1. section `00` governance invariants
2. the most specific canonical section owning the behavior
3. adjacent runtime or protocol sections that must conform to that owner
4. tooling, inspection, gate, and observability sections
5. planning artifacts

Planning artifacts never outrank canonical section text.

## Specificity rule

When a general section and a specific section overlap:

- the specific owner defines the behavior
- the general section defines cross-cutting constraints and inherited
  invariants

Examples:

- section `08` owns transaction lifecycle details
- section `35` owns durability and recovery details built on section `08`
- section `42` owns failure-class framing built on sections `08` and `35`
- section `31` defines certification requirements but does not redefine runtime
  behavior

## Rebuild-stage contradiction rule

Where a contradiction exists between:

- older placeholder canon
- newer recovered canon
- current code-backed notes

the newer recovered canon wins if it explicitly states:

- current code-backed truth
- required reconstructed behavior
- implementation drift where present

## Anti-donor rule

Donor-engine behavior never wins by implication.

Canonical rule:

- donor behavior is used only when promoted into ScratchBird canon
- similarity to PostgreSQL, Firebird, or any other donor does not override
  ScratchBird section ownership

## MGA and anti-WAL precedence

If any section appears to imply WAL-authoritative truth, redo-log recovery
authority, or non-transactional DDL or DML behavior, the MGA-governed sections
and section `00` invariants take precedence.

## Parser precedence

If any parser-related section appears to imply cross-parser lowering
dependencies, section `00` invariants and section `28` parser isolation rules
take precedence:

- each parser lowers its own syntax locally
- parser packages are optional
- parser packages may not depend on one another for lowering or execution

## Optimizer parity precedence

If any section appears to imply that some admitted index families are secondary
or ignorable by default, the index-parity and typed-metrics rules in sections
`18` and `36` take precedence.

## Security precedence

If any tooling or protocol section appears to imply broader mutation authority
than the security sections allow, the security sections take precedence:

- inspection authority is distinct from mutation authority
- redaction rules still apply on public inspection lanes

## Resolution output rule

Every resolved contradiction must produce one of:

1. canonical section edit
2. explicit non-guarantee
3. implementation drift record

Silent unresolved contradictions are non-conforming.
