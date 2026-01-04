# Plan Authoring Guide for Low-Capability Agents

## Purpose
This guide defines how to write specifications and implementation plans that a low-capability agent can execute without ambiguity, architectural guesswork, or missing key work.

## Core Principles
- **Decisions must be resolved**: no multiple-option lists. Every design choice is fixed.
- **Exact locations**: always list file paths, class names, function names, and enums.
- **Byte-level clarity**: for on-disk changes, specify layout, offsets, sizes, and version bumps.
- **Explicit error handling**: define corruption rules and failure modes.
- **Zero hidden dependencies**: call out every related subsystem that must be updated.
- **Test-first completeness**: define test types, names, directories, and expected outcomes.

## Required Sections (Template)
1) **Scope and Non-Goals**
   - Clear boundaries; what is explicitly out of scope.

2) **Decision Gates (Resolved)**
   - Example format:
     - "Use PageHeader table_id inserted after database_uuid; PageHeader is 80 bytes; no upgrade path." 

3) **Data Layout / On-Disk Format**
   - Struct definitions or diffs with sizes and offsets.
   - Version bump policy.
   - Corruption detection rules.

4) **Code Touchpoints**
   - File paths and exact symbols to modify.
   - Switch statements that must include new enum values.
   - Call chains that must be updated to avoid bypassing the change.

5) **Algorithm / Control Flow**
   - Step-by-step procedure with exact state transitions.
   - State diagrams if applicable.

6) **Error Handling / Failure Modes**
   - Define what is a hard error vs recoverable.
   - Define cleanup/rollback behavior.

7) **Concurrency / Transaction Semantics (MGA)**
   - Specify which XID determines visibility.
   - Define the safety condition for GC / deletion (e.g., `retired_xid < oldest_active_xid`).

8) **Config / Feature Flags**
   - If behavior is gated, specify the exact config key and default value.

9) **Testing Requirements**
   - Test location (`/tests/`).
   - Required test types: unit, integration, restart, negative, concurrency.
   - Exact assertions and expected outcomes.

10) **Acceptance Criteria**
   - Concrete pass/fail statements (not subjective).

## Precision Checklist (Must Be Answered)
- Which file(s) and symbol(s) change?
- Exact struct layout and size?
- Which existing API is called (exact signature)?
- What happens on corruption or invalid state?
- What is the transaction visibility rule?
- How does the change persist across restart?
- Which tests are required and where are they added?

## Common Ambiguity Patterns to Eliminate
- "Use checksum" → specify algorithm and function (e.g., `scratchbird::core::crc32cCompute`, init 0xFFFFFFFF, final XOR).
- "Dual meta pages" → specify where meta page IDs are stored and how to choose the winner.
- "Shadow index" → specify state transitions and visibility logic with XIDs.
- "Update catalog" → list exact tables and columns, plus DDL and load/save paths.

## Example Clarifications That Remove Ambiguity
- **PageHeader change**:
  - Add `table_id` immediately after `database_uuid`.
  - Header becomes 80 bytes, offset changes to 0x40.
  - No upgrade path required; zero `table_id` on heap page is corruption.

- **Columnstore meta pages**:
  - Dual meta pages must specify:
    - How meta pages are allocated and persisted.
    - Where both page IDs are stored.
    - CRC32C function and checksum range.
    - Which page wins on read (newest valid generation).

- **Index shadow rebuild**:
  - `BUILDING -> ACTIVE -> RETIRED` with `valid_from_xid` and `retired_xid`.
  - Old index GC when `retired_xid < oldest_active_xid`.
  - Index names unique per table; shadow index must be hidden or internal.

## Audit Checklist (For Reviewer)
- All decision gates are resolved and not phrased as options.
- All file paths/symbol names are listed.
- Every on-disk change has explicit size/offset/version update.
- All error cases and cleanup logic are described.
- Tests exist in `/tests/` and cover restart + negative cases.

## Stop Conditions
If any required detail is missing, the agent must stop and ask before implementation.
