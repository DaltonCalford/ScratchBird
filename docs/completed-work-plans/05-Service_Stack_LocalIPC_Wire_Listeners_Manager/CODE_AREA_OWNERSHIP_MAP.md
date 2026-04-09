# Code Area Ownership Map

## Primary Write Scopes

| Ticket | Primary write scope | Conflict surfaces | Parallelization rule |
| --- | --- | --- | --- |
| B1-05-001 | assigned section specs plus this package | all package control files | serial only |
| B1-05-002 | primary canonical targets for sections `25,26,27,29,32` plus package control files | all package control files and primary section README targets | serial only |
| B1-05-003 | include/scratchbird/server/ipc_server.h, src/server/ipc_unix.cpp, src/server/ipc_windows.cpp, src/server/ipc_tcp.cpp, src/ipc/ipc_server.cpp, src/server/server_session.cpp, include/scratchbird/core/connection_context.h, src/core/database.cpp | local IPC session identity deployment and bootstrap seams | after ownership freeze |
| B1-05-004 | include/scratchbird/server/service_controller.h, src/server/service_controller.cpp, src/server/sb_manager_main.cpp, include/scratchbird/network/control_plane.h, src/network/control_plane.cpp, src/network/sb_listener_main.cpp, include/scratchbird/protocol/wire_protocol.h, src/protocol/wire_protocol.cpp, src/protocol/adapters/native_adapter.cpp, src/core/catalog_manager.cpp, src/core/database.cpp | wire listener manager catalog and topology seams with overlap on database bootstrap and service-controller launch paths | after lane A foundation |
| B1-05-005 | wire and service-stack gates and benchmarks | shared public-beta compatibility and listener test runners | after implementation tickets |

## Unsafe Parallel Boundaries

- any ticket that updates SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- any ticket that changes the same canonical spec file as another ticket
- any ticket that changes the same gate or benchmark artifact family
- any ticket that changes `src/core/database.cpp`,
  `src/core/catalog_manager.cpp`, `src/server/service_controller.cpp`,
  `src/server/sb_manager_main.cpp`, `src/network/sb_listener_main.cpp`,
  `src/network/control_plane.cpp`, `src/protocol/wire_protocol.cpp`,
  or `src/ipc/ipc_server.cpp`
