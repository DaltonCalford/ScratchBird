Status: reconstructed_required

# Logical and Physical Backup Consistency and Optional WAL Rollforward Model

## Purpose

This document defines the canonical consistency boundary for logical backups, physical page backups, and the optional wal_after rollforward lane used for backup advancement.

## Canonical Rule

Logical backups and physical page backups do not represent the same time boundary.

- Logical backups are frozen at the snapshot captured when backup starts.
- Physical page backups are current as of backup completion.

The optional wal_after lane may advance a logical backup to a later timestamp, but it does not replace MGA durability truth inside the live engine.

## Logical Backup Model

Logical backup captures logical object state through a snapshot-frozen export boundary.

Required logical-backup properties:
1. backup start establishes the authoritative logical snapshot boundary
2. all logical rows or objects exported belong to the committed MGA-visible state for that boundary
3. statements committing after the logical-backup start boundary are not automatically part of the backup
4. long-running export duration does not move the logical snapshot boundary forward

## Optional WAL Rollforward for Logical Backups

wal_after may be used as an optional rollforward lane for logical backups.

Required rollforward properties:
1. rollforward begins from the logical backup start boundary
2. wal_after segments are applied in committed source order up to a selected target timestamp or target sequence
3. every applied segment remains derivative backup-advancement evidence, not live restart authority
4. if a segment is missing, corrupt, or not fully applied, rollforward stops at the last verified applied boundary
5. the resulting advanced logical backup is valid only through that applied boundary

## Physical Page Backup Model

Physical page backup copies durable page-image truth.

Required physical-backup properties:
1. included pages must come from canonical durable page images
2. checksum and integrity markers are part of the backup truth
3. the backup completion boundary defines the timestamp through which the backup is current
4. restore of that physical backup recovers the database to the backup end state without requiring wal_after advancement to reach that same boundary

## Comparison Rule

When comparing logical and physical backups:
1. a logical backup without rollforward represents start-of-backup state
2. a physical backup represents end-of-backup state
3. a logical backup with completed wal_after rollforward may reach a later selected target timestamp than its original start boundary
4. the comparison shall preserve which boundary each backup actually represents

## Restore Rule

Logical backup restore and physical backup restore are distinct:

1. logical restore reconstructs objects from exported logical content
2. optional wal_after rollforward may then advance that restored logical image to the selected target timestamp
3. physical restore reattaches durable page-image state as of the physical backup end boundary

## Non-Guarantees

This file does not redefine wal_after as core recovery truth. wal_after remains optional derivative lineage used here only for backup advancement where explicitly admitted.
