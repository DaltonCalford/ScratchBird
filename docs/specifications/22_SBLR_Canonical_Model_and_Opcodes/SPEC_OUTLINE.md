# Section 22 Specification Outline

Status: current_authority

## Owned surfaces

1. SBLR container identity, serialization order, and compatibility rules.
2. Opcode registry ownership, family classification, and extension discipline.
3. Statement payload schemas and identifier binding requirements.
4. Domain payload schemas used by current v3 compiler and verifier paths.
5. Verifier rejection rules, stable diagnostic classes, and compatibility checks.
6. Parser normalization rules that must be satisfied before SBLR emission.
7. Catalog helper interaction rules needed to build committed-baseline SBLR without engine-side name guessing.

## Section boundaries

- Section 22 defines what a valid SBLR artifact is.
- Section 23 defines how a valid SBLR artifact is compiled, planned, and executed.
- Section 24 defines the catalog truth that parsers consume.
- Section 28 defines how each parser family constructs SBLR.
