# Third-Party Dependency Assessment (Client-Side Footprint)

Goal: minimize external dependencies; identify current third-party libraries, role, and feasibility of vendoring/bundling.

## Build-Time/Runtime Dependencies
- **nlohmann/json** (header-only) — fetched via FetchContent; already vendored into the build tree.
- **googletest** (tests only) — fetched via FetchContent; not shipped in release binaries.
- **OpenSSL** — required for TLS/auth/security (Crypto/SSL). Heavy and security-sensitive; recommend keep as system dependency (do not vendor).
- **libcrypt** — password hashing fallback; system lib, small; keep system.
- **libxml2** (optional) — XML/XPath support; if absent, code falls back to basic string handling. Vendoring possible but large; better as optional system dep.
- **LZ4** (optional) — compression; could be bundled (small C library), currently system optional.
- **GEOS** (optional) — spatial ops; large; keep optional system dep.
- **PROJ** (optional) — coordinate transforms; large; keep optional system dep.
- **Threads/pthread** — system.

## Vendoring Feasibility
- Already in-tree: nlohmann/json; googletest (tests).
- Safe to bundle if needed: LZ4 (small), but currently optional.
- Not recommended to vendor: OpenSSL, GEOS, PROJ, libxml2 (size/security/maintenance).

## Suggested Actions
1) Keep release build minimal: require only OpenSSL + libc/pthreads; all others optional/off by default.
2) If “no external deps” is mandated for non-crypto features, vendor LZ4 only, and gate GEOS/PROJ/libxml2 behind OFF-by-default options.
3) Document optional deps and their feature flags in README/BUILD_ENVIRONMENT.
