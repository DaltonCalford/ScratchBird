# Cluster Fabric, Manager Heartbeat, and Drift Gate Model

## Scope

This file defines the certification model for:

- manager heartbeat publication
- cluster-fabric link readiness
- DBBT and LPREFACE guarded admission
- remote drift inspection
- redacted event and error evidence

## Current code-backed authority

Current code-backed recovery proves:

1. manager token auth exists
2. DBBT signing and validation exists
3. LPREFACE encoding, decoding, and validation exists
4. listener-side DBBT and LPREFACE validation exists
5. cluster-fabric link, session, task, event, and error substrate exists
6. error retrieval is redaction-aware

## Gate objective

The gate must prove that the manager and cluster-fabric control lane:

- preserves bounded admission semantics
- preserves redacted evidence semantics
- distinguishes local heartbeat from link readiness and remote drift

## Required certification evidence

The certification lane must retain, at minimum:

- gate run identity
- manager-auth result
- DBBT validation result
- LPREFACE validation result
- link posture summary
- heartbeat posture summary
- drift posture summary
- redaction result summary

## Pass criteria

The gate passes only if:

1. manager auth succeeds or fails with deterministic result class
2. forged or mutated DBBT or LPREFACE material is rejected
3. link posture remains distinguishable from manager heartbeat posture
4. drift posture remains distinguishable from heartbeat posture
5. public evidence remains redacted

## Fail-closed rules

The gate fails if:

1. sensitive control-plane material is exposed
2. invalid DBBT or LPREFACE material is accepted
3. heartbeat, link, and drift posture are collapsed into one generic result
4. required evidence families are missing

## Reconstructed-required behavior

The rebuild requires later promotion of:

- manager-heartbeat bus certification
- queued remote-instruction and drift certification
- stronger executed run transcripts and preserved artifacts for this lane
