# Dependencies

## Primary implementation authorities
- `CatalogManager` routine APIs and storage paths in `catalog_manager.h` and `catalog_manager.cpp`
- `Executor` routine invocation, permission, and security-context handling in `executor.cpp`
- language UDR registry and compile boundary in `language_udr_runtime.cpp`
- builtin emulation package manifests in `emulation_package_manifest.cpp`
- remote connector introspection implementations in `src/udr/*.cpp`

## Important adjacent sections
- section `07` for catalog bootstrap and UUID identity
- section `08` for transaction and restart semantics that routine persistence relies on indirectly
- section `11` for TOAST-backed storage expectations used by stored code payloads
- section `13` and section `14` for runtime conversion behavior used during routine argument binding
- section `16` for executor session and context-variable interactions used by routine execution

## Boundary dependency notes
- parser or emitter surfaces are supporting evidence only in this pass; the runtime authority remains catalog plus executor plus UDR runtime code
- remote connector and cluster-fabric catalog extensions are dependencies for checklist material, but they do not by themselves prove full execution guarantees
- blob-filter catalog rows depend on catalog extension tables, not on a proven runtime filter engine in this section
