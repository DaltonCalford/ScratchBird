# SBLR To V3 Rendering And Fidelity Rules

## Status

Authoritative current rendering contract plus required reconstructed fidelity rules for one-way V3 rendering.

## Purpose

This file defines the current code-backed text-rendering behavior for SBLR-derived V3 output, the exact name-rehydration order, and the boundary between the current renderer and the fuller SBLR-to-V3 converter required by section `28`.

## Current code-backed rendering authority

Current rendering truth is owned by:
- `native_sql_render_contract`
- `native_sql_renderer`
- the current native render endpoint surfaces

The current renderer is a structured payload renderer, not a donor-dialect recovery engine.

## Current renderer output model

Current render surfaces may emit:
- SQL text
- contract id
- canonical opcode symbol
- result-shape metadata
- resolver-assisted name hydration

The current renderer works from verified SBLR instruction payloads and does not attempt to reconstruct the original donor dialect.

## Current code-backed rendering algorithm

1. accept a verified instruction tree or verified payload value
2. interpret payload fields by exact type:
   - object
   - list
   - string
   - bytes
   - bool
   - u64 or i64 where safe
3. render schema paths by joining ordered path components with `.`
4. render string literals by single-quoting and doubling embedded `'`
5. preserve `NULL`, booleans, and scalar numerics by canonical V3 rules
6. resolve UUID-looking name tokens only through the configured resolver
7. leave non-UUID tokens unchanged rather than guessing alternate names
8. map current enumerated payload families through deterministic lookup tables such as:
   - compare operators
   - scorer names
   - vector metric names
   - hybrid bridge mode names

## Current name rehydration contract

The current renderer uses `NativeSqlNameResolver::resolveNameByUuid` only when all of the following are true:
- a resolver is present
- the token is non-empty
- the token text looks exactly like UUID text
- the resolver returns a non-empty resolved name

If any of those conditions fail, the renderer leaves the token unchanged.

## Fidelity order for rendering

For current rendering surfaces the name source order is:
1. inline retained name already present in the SBLR payload
2. resolver-based UUID-to-name recovery for durable objects
3. unchanged token text when no legal recovery path exists

For the required full converter owned by section `28`, the order becomes:
1. normalized retained-symbol payload
2. inline retained payload name
3. current-transaction local overlay when rendering inside the same uncommitted session context
4. committed catalog resolver result for UUID-backed objects
5. deterministic non-durable fallback only when allowed by the converter contract

## Current renderer guarantees

The current renderer guarantees:
- deterministic output for supported payload families
- no donor-dialect guessing
- UUID-to-name recovery only through the configured resolver
- stable string quoting rules
- explicit treatment of missing fields as absence, not inferred syntax

## Current renderer non-claims

The current renderer does not yet prove:
- full AST reification for every statement family
- universal coverage of every opcode family in canonical SBLR
- donor-dialect recovery
- round-trip fidelity back to the exact original SQL text

## Required fidelity rules for canonical V3 rendering

1. Durable object identity must remain UUID-authoritative.
2. User-visible names must come from retained payload or resolver truth, never from heuristic guessing.
3. Uncommitted local DDL overlays must outrank committed resolver state when rendering inside the same session context.
4. Rename and move operations must alter rendered path or label text without altering durable UUID identity.
5. If current state cannot be rendered without fabricating a user-significant name, the renderer or converter must fail closed.

## Explicit exclusions

This file does not authorize:
- conversion to any donor dialect other than V3
- style-preserving re-emission of the original SQL source
- heuristic inference of omitted aliases, labels, or variable names
