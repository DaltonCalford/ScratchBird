# Native Listener Control SQL

## Current code-backed truth
- Real listener and manager-proxy runtime code exists in `service_controller.cpp` and `sb_manager_main.cpp`.
- Real listener binaries and protocol routing names exist for native, PostgreSQL, MySQL, and Firebird front doors.
- Section `21` can therefore claim a code-backed control-plane runtime anchor for listener management, not just parser prose.

## Boundary
- Exact SQL grammar for every listener control statement remains partially audited.
- Runtime listener orchestration is real, but statement-by-statement SQL parity still needs tighter parser proof.
- Defer wire and listener-path semantics to section `26` and security ingress boundaries to section `19`.
