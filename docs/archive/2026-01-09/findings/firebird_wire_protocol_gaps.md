# Firebird Wire Protocol Compatibility Review (Findings)

Goal: 100% remote network compatibility for Firebird clients against ScratchBird server. This document enumerates potential gaps/unclear areas to verify in `docs/specifications/wire_protocols/firebird_wire_protocol.md` against Firebird clients and toolchains.

## Potential Gaps / Clarifications Needed
- **Service Manager details**: op_service_* packets exist but spec lacks full service parameter blocks, response formats, and common service actions (backup/restore/trace). Confirm required service requests for popular tools.
- **Event handling**: op_que_events/op_event are listed but the event payload structure, event count encoding, and re-arming semantics need explicit definition for compatibility.
- **BLOB segmentation rules**: segment size limits, segment continuation, and status codes for end-of-blob need concrete limits and examples.
- **SQLDA / XSQLDA completeness**: SQLDA layout is present but needs full alignment/packing rules, and handling for array/nullable fields.
- **Character set/collation negotiation**: DPB has charset but runtime column-level collations and client/server negotiation rules should be specified.
- **Wire compression**: DPB flags exist but compression framing and error handling should be fully defined.
- **Auth plugin interaction**: DPB auth plugin fields exist; confirm full SRP, legacy auth, and plugin negotiation flow across protocol versions 10–13.
- **Protocol version quirks**: differences between v10, v11, v12, v13 are referenced but not enumerated (e.g., op_prepare2 vs op_prepare). Define version-specific behavior.
- **Status vector mapping**: status vector format exists, but mapping to SQLSTATE/Firebird error codes and client-visible error strings should be clarified.

## Decisions / Constraints (Alpha)
- **Compatibility target**: Any native Firebird client must connect and be fully supported.\n+- **Replication**: Not required for emulated engines in Alpha; replication is deferred.\n+- **Legacy migration**: Passthrough/migration is handled by Firebird UDRs that connect to legacy databases.

## Suggested Validation Matrix
- **Connection**: attach/create/detach, protocol versions 10–13.
- **Auth**: SRP, legacy, and plugin-based auth with DPB negotiation.
- **Statements**: allocate/prepare/execute/fetch, including DSQL and BLR requests.
- **Transactions**: TPB variants (read committed, no auto-undo, etc.).
- **BLOBs**: create/open/segment read/write.
- **Events**: queue/cancel and event delivery.
- **Service Manager**: backup/restore/trace/replication tasks.
