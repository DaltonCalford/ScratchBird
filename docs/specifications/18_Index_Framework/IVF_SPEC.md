# IVF Specification

Status: current_authority_with_reconstructed_expansion

## Purpose

This document defines the current ScratchBird IVF-class surface and the stronger residency and planner obligations that apply while IVF remains routed through the shared vector runtime.

## Current implementation boundary

`IVF` and related IVF-class surfaces are currently routed through the shared vector-family runtime defined by `HNSW_SPEC.md`.

Distinct IVF centroid pages, probe lists, or standalone coarse-quantizer storage are not current implementation authority unless separately promoted.

## Current routed behavior

While routed through the shared vector family, IVF-class surfaces currently inherit:

- MGA visibility recheck on every returned candidate
- candidate-only index truth
- routed ANN search behavior rather than distinct IVF file or page layouts
- family metrics through the shared vector metrics envelope

## Required reconstructed behavior

If IVF-class surfaces are exposed as first-class operator-visible families, they must still satisfy the family-level vector residency model from:

- `VECTOR_INDEX_RESIDENCY_AND_ACCELERATOR_MIRROR_MODEL.md`

And they must additionally provide IVF-specific operator semantics for:

- coarse quantizer identity
- list count or partition count
- probe count
- list-skew and list-density metrics
- exact-rerank or post-filter behavior when stronger semantics are claimed

## Required semantic contract while still routed

While routed through the shared vector family, IVF-class surfaces shall still expose:

- probe-count style planner controls only where the runtime can honor them safely
- MGA visibility recheck on every returned candidate
- exact-distance post-filter or rerank when the surface promises stronger IVF semantics than raw ANN routing
- typed metrics for candidate expansion, post-filter rejection, recall calibration, visibility rejection, and residency readiness

## Planner rule

The planner must not treat a named IVF surface as optimizer-complete merely because the parser admits the syntax.

Allowed states are:

- routed through shared vector runtime with conservative costing
- promoted distinct IVF runtime with full family-native metrics
- fail-closed when a stronger IVF promise cannot be honored safely

## Non-authority and rejection rules

The following claims are incorrect:

- named IVF syntax implies a distinct current IVF storage family
- IVF probe controls may be accepted when the routed runtime cannot safely honor them
- IVF-class surfaces bypass the shared vector residency and accelerator mirror rules
