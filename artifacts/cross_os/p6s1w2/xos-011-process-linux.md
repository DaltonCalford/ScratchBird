# XOS-011 Linux Process Adapter Implementation
Last-Modified: 2026-02-22

## Linux Adapter Surface
The POSIX branch in `src/core/process_control.cpp` implements:
1. `fork + execvp` launch path.
2. Optional process-group setup (`setpgid`).
3. Optional working-directory switch (`chdir`).
4. Environment override application (`setenv`).
5. Wait semantics (`waitpid` with timeout polling).
6. Termination (`kill(SIGTERM|SIGKILL)`).
7. Running-state checks (`kill(pid, 0)`).

## Verification
Focused unit tests added:
1. `tests/unit/test_process_control.cpp`

Executed tests:
1. `ProcessControlTest.SpawnsAndWaitsForExit`
2. `ProcessControlTest.TerminatesProcess`
3. `ProcessControlTest.ForkSelfCreatesChildProcess`

Result:
1. `3/3` tests passed.

Evidence:
1. `artifacts/cross_os/p6s1w2/xos-013-014-process-ctest.txt`
2. `artifacts/cross_os/p6s1w2/xos-013-014-command-log.txt`

## Gate Binding
- Gate: `XOS-GATE-02`
- Tracker row: `XOS-011`
