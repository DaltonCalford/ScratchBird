# XOS-021 Monotonic Timer And Sleep Abstraction

Last-Modified: 2026-02-22

## Scope
Added a core timer abstraction for monotonic timing, wall-clock milliseconds, and sleep control.

## Code Changes
- Added `include/scratchbird/core/clock_control.h`
  - `ClockControl` interface:
    - `monotonicNow()`
    - `realtimeNowMs()`
    - `sleepFor(std::chrono::milliseconds)`
- Added `src/core/clock_control.cpp`
  - Default platform implementation based on `std::chrono` and `std::this_thread::sleep_for`
- Added `tests/unit/test_clock_control.cpp`
  - monotonic progression/sleep behavior
  - realtime millisecond availability

## Result
`XOS-021` completed.
