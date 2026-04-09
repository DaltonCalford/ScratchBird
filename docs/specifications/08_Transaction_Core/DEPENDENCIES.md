# Dependencies

## Purpose

This file defines the authoritative dependency contract for section `08`.

## Upstream Dependencies

### Section `03`

Section `03` owns dirty-page inventory, buffer-pool flush execution, writeback ordering, and write-admission fence support used by transaction terminal publication.

### Section `05`

Section `05` owns page-header structure, checksum structure, and on-disk page validity rules consumed by transaction restart and validation flows.

### Section `06`

Section `06` owns fixed bootstrap placement, including the fixed transaction-map bootstrap root.

### Section `07`

Section `07` owns bootstrap catalog identity and durable UUID-backed object identity used by transaction provenance and replay binding.

## Internal Runtime Dependencies

### `TransactionManager`

`TransactionManager` owns transaction-map truth, snapshot capture, OIT/OAT/OST horizon management, startup normalization, and terminal publication.

### `ConnectionContext`

`ConnectionContext` owns savepoints, runtime transaction context, lineage capture, and replay-session binding.

### `Database`

`Database` owns checkpoint-state load, startup classification inputs, and service-state fencing.

### `CatalogManager`

`CatalogManager` owns runtime context rows, lineage rows, committed schema epochs, and forensic snapshot capsules used by replay binding.

### `BackupManager`

`BackupManager` owns restore execution, restore rehearsal, and restore validation orchestration while consuming the transaction guarantees defined here.

## Downstream Consumers

Section `09` depends on section `08` for transaction identity, snapshot, and conflict semantics.

Section `24` depends on section `08` for commit-bound schema publication and metadata visibility.

Section `28` depends on section `08` for parser-facing schema visibility and transaction-local overlay rules.

Section `35` depends on section `08` for transaction-map truth and recovery-safe publication semantics.

Section `37` depends on section `08` for committed schema epoch publication, metadata invalidation boundaries, and DDL transaction scope.

## Dependency Rules

A downstream section shall not redefine transaction visibility or terminal publication truth.

A downstream section may add section-local constraints, but it shall not weaken the always-in-transaction MGA model owned here.

A component may consume transaction-core state only through the authoritative transaction structures and publication rules defined by this section.

## Explicit Non-Goals

This file does not define a machine-readable dependency export format.
