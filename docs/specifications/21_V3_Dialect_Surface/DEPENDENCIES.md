# Dependencies - 21_V3_Dialect_Surface

## Primary implementation dependencies
- parser AST and schema path model in `ast_v3.h` and `schema_path_v3.*`
- SBLR v3 payload and container contracts used by `v3_emitter` and `ast_sblr_lowerer`
- listener-path execution and wire entry surfaces in section `26`
- object, catalog, and UUID resolution behavior in section `24`
- runtime semantics for routines, connectors, cluster fabric, and blob filters in section `17`
- context-variable, security, and diagnostics ownership in sections `16`, `19`, and `20`

## Downstream dependents
- section `22` for canonical model and opcode expectations
- section `26` for listener or wire proof surfaces
- section `28` for emulated parser behavior and dialect-specific front doors
- client and tooling surfaces that rely on parser-level compatibility behavior

## Explicit boundary
- section `21` owns parser and lowering truth
- section `21` does not own executor, catalog, transport, or security parity for every accepted statement form
