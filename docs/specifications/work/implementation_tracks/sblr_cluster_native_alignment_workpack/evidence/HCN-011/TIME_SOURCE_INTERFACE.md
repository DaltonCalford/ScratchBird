# TimeSource Interface

Interface:
- `class TimeSource`
  - `uint64_t nowMs() const`
  - `uint64_t nowMicros() const`
- `const TimeSource& defaultTimeSource()`

Bindings:
- `generateUuidV7(const TimeSource* time_source = nullptr)`
  - Uses injected source when provided.
  - Falls back to `defaultTimeSource()` otherwise.
