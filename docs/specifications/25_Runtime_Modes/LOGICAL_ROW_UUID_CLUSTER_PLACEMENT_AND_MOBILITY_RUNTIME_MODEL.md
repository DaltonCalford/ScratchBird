Status: reconstructed_required

# Logical Row UUID Cluster Placement and Mobility Runtime Model

## Purpose

This document defines how logical row UUIDs are used by runtime cluster placement, movement, and lineage tracking.

## Canonical Rule

The system row UUID is the canonical logical-row tracking identity used by cluster movement and placement logic. Physical record location, page position, and MGA version instance do not replace it.

## Placement Rule

Runtime placement and movement logic shall track:

- logical row UUID
- current node or placement identity
- current version lineage state
- current transaction visibility state

## Mobility Rule

When a logical row is moved, replicated, or reconciled across cluster nodes:

- the logical row UUID remains stable
- the UUID tracks the logical row across placement changes
- MGA version lineage remains subordinate to the logical row UUID

## Alias Rule

If the row UUID is surfaced through a user-visible UUID identity column, cluster placement and movement shall still use that same logical identity rather than generating a second internal tracking UUID.

## Non-Guarantees

This file does not require every cluster mode to move rows independently today. It defines the canonical runtime identity for row-level cluster tracking.
