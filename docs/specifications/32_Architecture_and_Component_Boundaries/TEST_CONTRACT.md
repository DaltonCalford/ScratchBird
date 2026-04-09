# Section 32 Test Contract

Required evidence for future hardening:
- process-mode coverage that distinguishes embedded/local IPC/server-managed paths
- interface-boundary evidence showing engine/listener/client separation
- local-only IPC endpoint and session identity proof across the threaded server
  wrapper
- layered deployment proof for embedded-direct versus listener-owned shared
  server selection
- proof that public or stable contracts are not inferred from internal message paths alone

Fail-closed rule:
- architecture claims remain bounded until explicit source and executed evidence exist
