# Isolation Level and Phantom Protection Matrix

Status: current_authority

## 1. Supported isolation levels

Current connection-context isolation levels are:

1. `READ_COMMITTED`
2. `READ_COMMITTED_READ_CONSISTENCY`
3. `SNAPSHOT`
4. `SNAPSHOT_TABLE_STABILITY`

## 2. Canonical behavior matrix

| Isolation level | Visibility owner | Snapshot owner | Lock posture | Current guarantee boundary |
| --- | --- | --- | --- | --- |
| `READ_COMMITTED` | current MGA inventory | none | ordinary lock policy | sees current committed state under MGA rules |
| `READ_COMMITTED_READ_CONSISTENCY` | statement snapshot when active | per statement | ordinary lock policy plus restart handling | statement-consistent reads with restart on conflict |
| `SNAPSHOT` | retained transaction snapshot | per transaction | ordinary lock policy | transaction-stable snapshot visibility |
| `SNAPSHOT_TABLE_STABILITY` | retained transaction snapshot | per transaction | reserved table locks | snapshot visibility plus stronger table reservation posture |

## 3. Phantom and predicate boundary

ScratchBird does not currently claim universal predicate-range locking or serializable phantom elimination across arbitrary predicates.

Current canonical boundary is:

1. snapshot levels provide snapshot visibility stability
2. table-stability mode adds explicit table reservation semantics
3. read-consistency mode provides statement-stable visibility and restart on conflict
4. full predicate-range lock guarantees are not current canonical truth unless a concrete section proves them

## 4. Read committed modes

The connection context also tracks read-committed submodes:

1. `DEFAULT`
2. `READ_CONSISTENCY`
3. `RECORD_VERSION`
4. `NO_RECORD_VERSION`

Where current code routes through the shared visibility and restart machinery, the engine must use those explicit submodes and not invent hidden isolation variants.

## 5. DDL and role-change interaction

Transaction boundaries remain in force across isolation changes, role switches, and DDL.
If policy requires commit or rollback before a context switch, the engine performs commit or rollback and immediately opens the next transaction boundary.

## 6. Negative requirements

The following are not canonical:

1. claiming serializable predicate locking without a concrete implementation surface
2. claiming phantom freedom outside the stated current guarantee boundary
3. claiming that DDL escapes isolation rules

## 7. Implementation contract

Any implementation against this file must prove:

1. only the published isolation levels are accepted
2. each level maps to the correct visibility context selection
3. statement read consistency uses statement snapshot and restart policy
4. unsupported phantom guarantees are not overclaimed
