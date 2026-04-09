# Weak Donor Capability and Extraction Mode Catalog Model

## Scope

This file defines the canonical persisted model for donor capability and
extraction posture used by:

- passthrough connectors
- migration planning
- weak-donor reconciliation
- cutover assessment

## Current code-backed authority

Current code-backed recovery proves that the catalog and migration substrate
already support persisted migration and replication-runtime families.

Canonical rule:

- donor capability and extraction posture must be persisted and queryable
- cutover logic may not rely on undocumented connector-local heuristics alone

## Donor capability classes

The canonical donor capability vocabulary must distinguish, at minimum:

- native transactional donor
- weak transactional donor
- non-transactional donor
- snapshot-capable donor
- change-feed donor
- passthrough-only donor

Canonical rule:

- donor capability class is first-class persisted state
- it is not derived only from free-form connector notes

## Extraction mode classes

The canonical extraction-mode vocabulary must distinguish, at minimum:

- consistent snapshot extraction
- bounded window extraction
- read-through passthrough extraction
- replay or catch-up extraction
- bulk baseline extraction

Each migration or proxy plan must persist which extraction mode is authorized.

## Drift and unresolved-state recording

The catalog substrate must remain able to record:

- unresolved drift class
- unresolved object count
- last assessed donor capability
- last assessed extraction mode
- cutover readiness class

Canonical rule:

- unresolved drift is auditable persisted state
- it is not temporary console text

## Relationship to remote connectors

Remote connectors may expose additional capability detail, but the canonical
cutover and migration lane must normalize that detail into persisted donor
capability and extraction-mode classes that other sections can consume.

## MGA boundary

These rows remain ordinary MGA-governed state. They do not bypass transaction
rules simply because the donor system may be weak or non-transactional.

## Reconstructed-required behavior

The rebuilt migration lane requires later control surfaces and runbooks to bind
to these persisted capability and extraction classes rather than inventing a
parallel status vocabulary.
