# Canonical Status Vocabulary and Normalization Rule

## Scope

This file defines the canonical status vocabulary for implementation-driving
specification files during and after the rebuild stage.

## Governing rule

Status text is not decorative.

Canonical rule:

- status labels must communicate authority class unambiguously
- status labels must not require implementers to infer whether a file is
  shipped truth, reconstructed requirement, drift-bearing canon, or unsupported
  boundary

## Canonical status set

Implementation-driving canonical files shall use one of the following status
classes:

### `current_authority`

Use when:

- current code-backed behavior is the controlling authority for the file
- no stronger reconstructed-required rule in the same file overrides it

### `current_authority_with_reconstructed_expansion`

Use when:

- current code-backed behavior is real and controlling
- the file also carries explicit reconstructed-required detail that extends the
  implementation target

### `reconstructed_required_with_current_substrate`

Use when:

- the file is authoritative product behavior
- substantial current code-backed substrate already exists
- implementation is still partial or drift-bearing

### `reconstructed_required`

Use when:

- the file is authoritative product behavior
- current code does not yet prove a substantial shipped substrate for the full
  file scope

### `unsupported_boundary`

Use when:

- the file defines an explicit fail-closed unsupported or non-authoritative
  runtime boundary

### `target_state_only`

Use only when:

- the file is quarantined future-lane canon
- it is not implementation-driving for the current rebuild lane

## Forbidden ambiguity

Implementation-driving files shall not use free-form status prose such as:

- `current implementation authority for ...`
- `authoritative current-plus-reconstructed ...`
- `recovered current authority plus ...`
- `bounded current authority ...`

unless that prose is expressed through one of the canonical status classes and
then elaborated in body text.

## Body-text rule

Files may still explain nuance in body text, including:

- current code-backed boundary
- required reconstructed behavior
- implementation drift
- unsupported sub-lanes

But the `Status:` line must use the canonical status set.

## Evidence and audit exception

Evidence packs, findings reports, and implementation-track artifacts are not
required to use the same status vocabulary. They may use:

- `PASS`
- `Completed`
- `Active`
- similar workflow states

This file governs implementation-driving canonical section files.

## Rebuild normalization rule

The rebuild must progressively normalize canonical section files to this
vocabulary.

Until normalization is complete:

- newer canonical rule files and newly added canonical files must use the
  canonical status set
- older files with non-canonical status text are treated as legacy wording, not
  a different authority model
