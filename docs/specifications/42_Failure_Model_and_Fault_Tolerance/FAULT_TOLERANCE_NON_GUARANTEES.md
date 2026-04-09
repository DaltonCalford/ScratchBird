# Fault Tolerance Non Guarantees

This file owns the explicit exclusions for section 42.

## Fault-tolerance non-guarantee matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| high availability | fail_closed | HA claims remain outside current proof unless explicitly owned elsewhere | not active-active or active-passive equivalence |
| distributed consensus | fail_closed | no consensus subsystem is implied | not Raft/Paxos-style correctness by analogy |
| transparent failover | fail_closed | no transparent failover guarantee is claimed | not seamless session migration |
| self-healing fleet behavior | fail_closed | no autonomous fleetwide healing system is implied | not orchestrated remediation parity |
| WAL-authoritative recovery | fail_closed | MGA durable state is authoritative instead | not replay-log rebuild semantics |
| derivative archive or `wal_after` truth | fail_closed | derivative evidence lanes are optional downstream artifacts | not correctness dependency |

## Canonical rules

1. Section 42 must state exclusions more strongly than neighboring sections imply them.
2. Absence of contradiction is not proof of tolerance.
3. Fault-tolerance claims remain fail-closed until directly proven.
4. Derivative logs, remote archives, and shadow copies are not promoted into
   recovery authority by existence alone.

## Explicit non-guarantees

- no HA product claim
- no consensus or quorum architecture claim
- no transparent failover or session migration guarantee
- no WAL-replay recovery guarantee
- no requirement that `wal_after` export or remote archive delivery succeed for
  the database to remain correct
