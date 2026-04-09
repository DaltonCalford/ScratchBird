# Time Source Interface (Gate Snapshot)

From HCN-011 closure:
- Introduced `TimeSource` abstraction.
- UUIDv7 now accepts injectable source via `generateUuidV7(const TimeSource* ...)`.

Validation anchor:
- `UuidV7TimeSourceTest.*` passed.
