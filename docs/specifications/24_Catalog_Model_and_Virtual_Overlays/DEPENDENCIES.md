# Dependencies

## Upstream dependencies

- section `07` for core persisted storage and catalog record-family ownership
- section `08` and section `35` for transaction, publication, and recovery semantics
- section `19` for security control-plane rows and secrecy boundaries
- section `20` for diagnostics and observability surfaces exposed through virtual system catalogs
- section `22` for `SBL3` artifact references that land in catalog-backed families
- section `23` for execution-artifact and plan metadata surfaces exposed through catalog families
- section `37` for statistics, metadata, and schema DDL behavior layered onto catalog publication

## Downstream dependents

- section `17` consumes catalog truth for routines, remote metadata, and cluster metadata row families
- section `20` consumes system catalog and virtual overlay visibility
- section `28` and section `29` consume catalog and overlay truth for parser, management, and tooling paths
- section `30` consumes metadata helper contracts for client tooling and parser assist surfaces

## Explicit non-ownership

Section `24` does not own the deeper runtime semantics of every family it exposes. It owns persisted and virtual exposure boundaries only.
