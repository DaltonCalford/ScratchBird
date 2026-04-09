# Catalog Root Slot Expansion Audit

Ticket: `CAT-002`
Status: `Completed`
Date: `2026-02-14`

## Purpose
Define the strict slot-expansion model for catalog root metadata so canonical catalog families can be added without structural ambiguity or ad-hoc root page field growth.

## Current State (Observed)
1. `CatalogRootPage` persistence currently writes many hardcoded table-page fields.
2. Legacy model is fixed-field and not scalable to canonical inventory growth.
3. Existing physical slot set (`60`) is smaller than canonical on-disk requirements (`265`).

## Required Expansion Model (Normative)

### A. Root Header Compatibility Contract
1. Keep current root page (`PAGE_TYPE_CATALOG_ROOT`) as anchor page.
2. Add compatibility-safe extension fields:
- `catalog_slot_dir_gpid` (global page id of slot-directory root)
- `catalog_slot_format_version` (uint16)
- `catalog_slot_count` (uint32)
- `catalog_slot_checksum` (uint32)
3. Legacy fixed fields remain readable for migration but become non-authoritative after cutover.

### B. Slot Directory Structure
Create persisted slot directory records (`catalog_slot_dir` internal structure):
- `slot_id` (uint16, deterministic)
- `catalog_name` (canonical table/view identifier)
- `catalog_object_type` (TABLE/VIEW/IN_MEMORY)
- `root_gpid` (current root page for this catalog object)
- `flags` (active, deprecated, virtual, reserved)
- `schema_branch` (canonical branch token)
- `created_epoch` / `modified_epoch`
- `is_valid`

### C. Deterministic Slot ID Policy
1. `0..127`: reserved system-fixed canonical core (`database/schema/object/object_name/...`).
2. `128..511`: canonical optional families (parser capability, replication, fabric, olap, text search, engine-specific).
3. `512..1023`: future expansion reserve.
4. Slot IDs are immutable once assigned.

### D. Read/Write Resolution Order
1. On open: read root page header.
2. If `catalog_slot_format_version >= 1` and `catalog_slot_dir_gpid != 0`, slot directory is authoritative.
3. If slot directory not present, fall back to legacy fixed fields.
4. During migration window, dual-write both legacy fixed fields and slot directory for legacy tables only.
5. After cutover gate, write legacy fixed fields only for backward-diagnostic visibility; readers must use slot directory.

### E. Migration Protocol (From CAT-001 Crosswalk)
1. Load `CAT-001/CATALOG_NAME_CROSSWALK.csv`.
2. For each legacy table mapping row:
- allocate canonical slot id
- point slot to existing legacy root page where compatible
- for `one_to_many_split`, create new roots and initialize split targets
3. For `canonical_only_new`, allocate new roots and slot records.
4. Persist slot directory and update root extension fields atomically.

### F. Validation Rules
1. `slot_id` uniqueness required.
2. `catalog_name` uniqueness required among active slots.
3. `root_gpid` must reference valid page type for each catalog kind.
4. `catalog_slot_checksum` must match deterministic checksum of ordered active slots.
5. Startup fails hard if slot directory checksum mismatch is detected.

## Rollback and Recovery
1. If slot directory write fails before root pointer update, rollback transaction and keep legacy mode.
2. If root pointer update fails after slot directory write, mark slot directory orphan record and retry recovery on startup.
3. If checksum mismatch exists, enter recovery mode and rebuild checksum from last valid slot snapshot.

## Gate-Relevant Output
This model is strict enough for implementation in `CAT-003+` without additional design inference.

## Linkage
- Depends on: `CAT-001/CATALOG_NAME_CROSSWALK.csv`
- Enables: `CAT-003` schema-tag alignment and `CAT-004+` table family implementation
