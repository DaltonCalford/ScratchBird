# SBLR Name Symbol and Context Retention Expansion

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines which user-significant names, aliases, labels, and scope markers SBLR already retains in current code, which additional retained structures are required for a full one-way SBLR-to-V3 converter, and where the engine must fail closed instead of fabricating text.

## Core Rule

UUID identity remains the durable engine truth for persistent objects. Name retention exists to preserve deterministic V3 reconstruction, diagnostics, and user-facing fidelity. Name retention does not replace UUID identity.

## Current Code-Backed Authority

Current code already retains substantially more user-facing context than the earlier canonical text admitted.

Two current lowering surfaces exist and must remain schema-equivalent:
- `parser::v3::AstSblrLowerer`
- `parser::v3::V3Emitter`

Those two surfaces expose the same public lowering contract:
- `emitStatementToContainer`
- statement-family emitters
- expression emitters
- helper encoders for schema paths, aliases, variable refs, and statement lists

They therefore define one current retained-payload contract, not two incompatible dialects.

## Current Retained Payload Families

### Path and durable-object naming payloads

Current lowering already stores:
- schema paths as ordered lists of identifier strings
- table paths inside table references
- referenced table paths in foreign-key constraints
- object paths and target schema paths across DDL and utility statements
- path-local object names where the AST carries them explicitly

### Statement-local alias and label payloads

Current lowering already stores:
- select-item aliases through `select_aliases`
- table aliases inside table references
- cursor names
- loop and block labels where the AST carries them
- procedure names for `EXECUTE PROCEDURE`
- exception names and event names where syntax requires them

### Variable and procedural symbol payloads

Current lowering already stores:
- variable names through variable-reference emitters
- cursor names through cursor statements
- record or target variable references for procedural fetch and loop statements
- parameter and option names where those appear as named option assignments or keyed utility options
- scope flags on statement families that already model scope explicitly

### Expression and output-label payloads

Current lowering already stores:
- column identifiers through `{ path, column }` structures
- select-item alias strings when present
- enum labels and ordinals for enum and enum-set literals
- constraint names, collation names, charset names, and index-method names where present in the AST

### Table-reference context payloads

Current lowering already stores:
- table flags
- `lateral`
- `with_ordinality`
- sampling method, sample percent, and repeatable seed when present
- subquery or function table references as explicit structured placeholders rather than guessed text

## Current Retention Model Boundaries

Current code retains names inline in statement payloads and, for the current
native-V3 lowering surface, also emits a versioned normalized retained-symbol
section with stable symbol ids, scope graphs, display-name registries,
output-label registries, and source-order registries.

That means the current retained-name model is:
- real
- substantial
- usable for supported renderer and converter families
- versioned and container-carried for the current native-V3 lowering surfaces
- still bounded to the currently shipped statement families rather than every
  future or donor-dialect reconstruction case

## Required Reconstructed Expansion

Commercial-grade canon requires SBLR to carry a normalized retained-symbol layer in addition to the already emitted inline names.

Package `03` implements this normalized retained-symbol layer for the current
native-V3 lowering surfaces. Inline retention remains current substrate and
compatibility input for degraded legacy payloads, but Beta 1 closure requires
the normalized carrier to be emitted and verified for new native-V3 SBLR.

The required additional retained structures are:
- `symbol_registry`
- `scope_registry`
- `scope_parent_map`
- `display_name_registry`
- `parameter_display_registry`
- `output_label_registry`
- `placeholder_binding_registry`
- `source_order_registry`

## Required Symbol Classes

The normalized retained-symbol layer must distinguish at least:
- `variable_symbol`
- `parameter_symbol`
- `relation_alias_symbol`
- `cte_symbol`
- `output_label_symbol`
- `cursor_symbol`
- `package_local_symbol`
- `block_label_symbol`
- `exception_symbol`
- `event_symbol`
- `option_key_symbol`

## Converter-Facing Name Resolution Order

When reconstructing V3, the engine must prefer names in this order:
1. explicit retained symbol payload for the relevant scope and symbol class
2. current inline payload name already present in the statement payload
3. catalog or resolver-derived durable object name for UUID-backed objects
4. deterministic generated fallback for non-durable unnamed symbols only

## UUID and Catalog Resolver Boundary

Current code-backed name recovery already includes UUID-text resolution against the catalog:
- UUID text is accepted only when it parses as canonical UUID text
- object-type hints are mapped to concrete catalog object types
- a hint mismatch must fail closed
- the resolver may return `object_name` or `full_path`

This makes catalog name recovery:
- legal for durable UUID-backed objects
- illegal for scope-local symbols such as aliases, variables, and block labels

## Normalization Rules

1. Durable object references remain UUID-authoritative even when retained display names are present.
2. Inline names already emitted by the current lowerers remain valid current authority and must survive compatibility migration.
3. The normalized retained-symbol layer must not silently reinterpret inline names; it must either reference them exactly or supersede them by explicit versioned rule.
4. Scope-local identifiers with the same spelling but different ownership must not collapse into one symbol.
5. Generated fallback names are never durable truth; they are renderer-side or converter-side emergency names only for non-durable symbols.
6. If the converter cannot reconstruct a user-significant symbol without guessing, conversion must fail closed.

## Verifier Rules

The verifier must enforce:
- symbol-class validity
- scope-parent validity
- no dangling symbol references
- no duplicate stable symbol ids inside one container
- no scope-local ambiguity after normalization
- no reference from a statement payload to a missing retained-symbol entry when that payload declares normalized retention usage

## Fail-Closed Rules

The engine must refuse SBLR-to-V3 reconstruction when:
- a required alias or label is missing and no non-guess fallback is legal
- a retained symbol references a missing scope
- scope-local ambiguity would force donor-dialect guessing
- a statement family depends on reconstructed names that were not preserved

## Relationship to Sections `22` and `28`

Section `22` owns the retained payload contract.
Section `28` owns the one-way SBLR-to-V3 conversion algorithm.
The converter must not invent new retention semantics outside the payload contract owned here.
