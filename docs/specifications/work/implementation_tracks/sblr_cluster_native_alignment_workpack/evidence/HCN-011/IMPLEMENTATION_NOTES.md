# Implementation Notes - HCN-011

Code paths:
- `include/scratchbird/core/time_source.h`
- `src/core/time_source.cpp`
- `include/scratchbird/core/uuidv7.h`
- `src/core/uuidv7.cpp`
- `tests/unit/test_uuidv7_time_source.cpp`

Design:
- `TimeSource` exposes `nowMs()` and `nowMicros()`.
- `defaultTimeSource()` supplies platform/system implementation.
- UUIDv7 consumes an optional `TimeSource*`, enabling deterministic injection for tests.
