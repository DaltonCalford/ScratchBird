### Task: Server/protocol/auth documentation authoring

Goal: Author server, protocol, provider dispatch, auth, TLS, pooling, and buffers docs.

Input:
- Server/protocol/auth sources and headers
- ProjectPlan OverallPlan Phase 11; Phase_11.7 performance docs

Steps:
1. Summarize process model: listener → session → protocol handler → provider dispatch.
2. Document protocols (Firebird compatibility, ScratchBird native) with message framing and auth flow.
3. Cover auth providers (password, trusted, 2FA) and TLS setup.
4. Implementation References to `network_server.cpp`, `protocol_handler.cpp`, `firebird_protocol*.cpp`, `authentication.cpp`, `tls_server.cpp`, `connection_pool.cpp`.
5. Note performance features (batching, buffer sizes) from 11.7 TODO.

Output:
- Updated `subprojects/server/index.md` and a `compliance.md` summarizing in-progress vs spec.

Validation:
- At least five concrete code anchors spanning server, protocol, and auth modules.
