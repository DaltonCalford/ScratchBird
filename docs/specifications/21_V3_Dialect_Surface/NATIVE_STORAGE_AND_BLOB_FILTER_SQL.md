# Native Storage and BLOB Filter SQL

## Current code-backed truth
- Parser-side storage DDL families are partially anchored by real create, alter, and drop tablespace and related DDL entry points.
- Broader BLOB filter runtime truth is owned by section `17`, which currently keeps generic blob-filter runtime fail-closed.

## Boundary
- This file does not prove generic blob-filter runtime invocation.
- Storage relocation and maintenance SQL must defer to sections `02`, `11`, `17`, and `18` for runtime semantics.
- Treat this file as parser-front-door and naming inventory only until a deeper code pass closes the exact storage and blob-filter surface.
- package `03` therefore supports blob-filter presence and UDR-handling setup
  only; generic blob-filter runtime parity remains intentionally deferred
