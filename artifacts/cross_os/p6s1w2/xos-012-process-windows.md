# XOS-012 Windows Process Adapter Implementation
Last-Modified: 2026-02-22

## Windows Adapter Surface
The `_WIN32` branch in `src/core/process_control.cpp` now implements:
1. Process launch via `CreateProcessA`.
2. Command-line construction and argument quoting.
3. Optional new process-group creation (`CREATE_NEW_PROCESS_GROUP`).
4. Wait semantics via `WaitForSingleObject`.
5. Exit-code collection via `GetExitCodeProcess`.
6. Termination via `TerminateProcess`.
7. Running-state checks via `GetExitCodeProcess`.
8. Handle lifecycle cleanup via `close()`.

## Validation Scope This Slice
1. Code implemented and compiled in the Linux host build (non-Windows branch active).
2. Windows runtime validation is deferred to cross-OS CI gates:
   - `XOS-053` (matrix jobs)
   - `XOS-058` (portable Windows suite)
   - `XOS-059` (cross-compiled artifact smoke)

## Risk Note
Windows launch path currently ignores environment override injection; if Windows-specific env override parity is required, add a follow-up row before `XOS-GATE-04`.

## Evidence
1. `artifacts/cross_os/p6s1w2/xos-010-012-command-log.txt`

## Gate Binding
- Gate: `XOS-GATE-02`
- Tracker row: `XOS-012`
