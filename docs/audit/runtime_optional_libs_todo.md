# TODO: Runtime-Optional Library Support (“install later, enable in config”)

Goal: allow binaries to run without optional libs present at build time, but load them at runtime if installed and enabled in config (except OpenSSL, which remains a build-time requirement).

## Target Libraries (runtime-load candidates)
- LZ4 (compression)
- libxml2 (XML/XPath)
- GEOS (spatial)
- PROJ (coordinate transforms)
- (Keep OpenSSL build-time; security-sensitive and pervasive)

## Approach
1) Add runtime loader shims (dlopen/dlsym on POSIX; LoadLibrary/GetProcAddress on Windows).  
2) Build call sites against a function-table interface, populated at startup if the library is found and enabled in config.  
3) Provide soft-fallback paths (no-op or reduced functionality) when library is absent.  
4) Add config flags to force enable/disable each optional lib; expose clear diagnostics when missing.  
5) Keep compile-time checks minimal: always compile the shim; do not require the dev package to build.  
6) Tests: add loader unit tests with/without the shared lib present (can simulate with LD_LIBRARY_PATH).  

## Work Items
- [ ] Define per-lib loader interfaces (lz4_loader.h/.cpp, xml2_loader.h/.cpp, geos_loader.h/.cpp, proj_loader.h/.cpp).  
- [ ] Refactor call sites to use loader function tables instead of direct link-time symbols.  
- [ ] Add config parsing for enabling/disabling optional libs at runtime; default to “auto” (load if present).  
- [ ] Implement graceful degradation messages when a feature is invoked but the lib is unavailable.  
- [ ] Add tests that run with libs present/absent to verify behavior and error paths.  
- [ ] Documentation: update BUILD_ENVIRONMENT/README to clarify runtime-optional model and how to install libs post-build.  

## Notes
- OpenSSL remains a build-time dependency; do not runtime-load for security reasons.  
- Ensure symbol versions are stable or gated by version checks in loaders.  
