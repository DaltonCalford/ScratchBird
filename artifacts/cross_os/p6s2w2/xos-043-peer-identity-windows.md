# XOS-043 Windows Peer Identity Mapping Behavior
Last-Modified: 2026-02-22

## Implemented
- Hardened Windows Named Pipe peer credential semantics:
  - PID remains collected for observability.
  - `PeerCredentials.available` now stays `false` until UID/GID-equivalent token mapping is implemented.
  - File:
    - `src/server/ipc_windows.cpp`
- Updated server session observability behavior for PID-only peers:
  - Adds PID-only annotations in client info for Named Pipe sessions when mapping is unavailable.
  - Exposes `SB$PEER_PID` session variable for PID-only Named Pipe sessions.
  - Keeps peer-auth gating behavior strict via `peer_credentials_available_`.
  - File:
    - `src/server/server_session.cpp`

## Security/Policy Outcome
- Windows local sessions no longer appear as fully peer-verified identities when only PID is known.
- Peer-required auth policies continue to fail closed until full Windows identity mapping is implemented.

## Validation
- Covered by targeted suite run:
  - `ctest --test-dir build -R 'WindowsServiceHostTest|LocalIPCPolicyTest|ServiceControllerListenerBootstrapTest|PortableRuntimeGuard|VNextScopeScanContract' --output-on-failure`
  - See `artifacts/cross_os/p6s2w2/xos-040-043-ctest.txt`
