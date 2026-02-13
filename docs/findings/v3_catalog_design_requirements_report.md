# V3 Catalog Design Requirements Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/development/CATALOG_DESIGN_REQUIREMENTS.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- Core catalog records largely use UUID references (owner_id, parent_schema_id, etc.) and the **18-schema hierarchy is bootstrapped**.
- Gaps vs this spec include **ObjectType enum values**, **constraint types (IN/NOT IN subquery)**, **absolute path semantics**, and **search_path default order**. Emulation tables exist, but **no code creates emulation schema views**.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### 1) UUID-Based References (CRITICAL)
[*] Catalog records use UUID references for ownership and parent relations (no `char owner[512]`).
    - Schema/Table/Index/Constraint/User/Role/Group records all use `ID owner_id` (or equivalent UUID fields).
    - `SchemaRecord.parent_schema_id` exists; owner names are resolved to UUID at creation time.

Key references:
- `src/core/catalog_manager.cpp:1276-1358` (SchemaRecord)
- `src/core/catalog_manager.cpp:1311-1398` (TableRecord)
- `src/core/catalog_manager.cpp:1529-1582` (ConstraintRecord)
- `src/core/catalog_manager.cpp:1864-1918` (User/Role/Group records)
- `src/core/catalog_manager.cpp:4194-4333` (createSchemaInternal uses owner->UUID)

### 2) Schema Hierarchy Structure
[*] `SchemaRecord` stores `parent_schema_id` and the bootstrap initializes the 18-schema tree.
[~] Root schema uses database UUID as schema_id, but parent is zero UUID (spec says database is parent).

Key references:
- `src/core/catalog_manager.cpp:1276-1298` (SchemaRecord with `parent_schema_id`)
- `src/core/catalog_manager.cpp:3021-3111` (bootstrap of root/sys/app/users/public/remote/emulation/mysql/postgresql/mssql/firebird)
- `src/core/catalog_manager.cpp:3048-3051` (root schema forced to DB UUID)

### 3) Dependencies System
[*] `DependencyRecord` exists and CRUD functions persist dependencies with dependent/referenced object UUIDs + types + dependency_type.

Key references:
- `src/core/catalog_manager.cpp:1743-1758` (DependencyRecord)
- `src/core/catalog_manager.cpp:25860-25899` (write/read dependency records)

### 4) TOAST Configuration / Activation
[~] Multiple catalog records carry TOAST OIDs and are stored/loaded via TOAST in several paths (comments, view definitions, constraint expressions).
[~] ACL/storage params/default/check expressions are stored as OIDs in records, but activation for **all** fields was not fully traced in this pass.

Key references:
- `src/core/catalog_manager.cpp:1759-1768` + `25899-25970` (CommentRecord + TOAST storage)
- `src/core/catalog_manager.cpp:1561-1579` + `26187-26336` (ViewRecord + TOAST definition/columns/base tables)
- `src/core/catalog_manager.cpp:1529-1582` + `27077-27230` (ConstraintRecord + TOAST check_expr)
- `src/core/catalog_manager.cpp:1276-1298` (SchemaRecord `acl_oid`)
- `src/core/catalog_manager.cpp:1330-1366` (TableRecord `storage_params_oid`)

### 5) Index Types - Full Implementation
[~] Catalog enum supports many index types (HNSW/IVF/LSM/etc), but on-disk IndexRecord enum in `catalog_manager.cpp` lists only a subset. Full implementation across storage/optimizer not verified here.

Key references:
- `include/scratchbird/core/catalog_manager.h:625-646` (IndexType enum with many types)
- `src/core/catalog_manager.cpp:1370-1418` (file-local IndexType on disk)

### 6) Object Types Enumeration
[ ] ObjectType enum values do **not** match the spec’s 32-type list and numbering; code includes additional types (EXCEPTION, SYNONYM, POLICY, JOB, etc.) and shifts numeric values.

Key references:
- `include/scratchbird/core/catalog_manager.h:702-743` (ObjectType enum in code)

### 7) Procedures and Functions
[*] ProcedureRecord stores procedures and functions together with `procedure_type`, `is_selectable`, and TOAST body/return type.
[*] Procedure parameters are stored in a separate table.

Key references:
- `src/core/catalog_manager.cpp:2031-2065` (ProcedureRecord)
- `src/core/catalog_manager.cpp:2067-2086` (ProcedureParameterRecord)

### 8) Constraints System
[~] ConstraintRecord supports primary/unique/foreign/check/not-null/default/exclusion, deferrable/enabled/validated flags, TOAST check_expr.
[ ] Spec-required IN/NOT IN subquery constraint types and `in_subquery_oid` are **not present**.

Key references:
- `src/core/catalog_manager.cpp:1495-1582` (ConstraintRecord)
- `src/core/catalog_manager.h:469-514` (ConstraintType enum)

### 9) Comments System
[*] CommentRecord is separate and uses TOAST for comment text.

Key references:
- `src/core/catalog_manager.cpp:1759-1768` (CommentRecord)
- `src/core/catalog_manager.cpp:25907-25970` (TOAST write/read)

### 10) Security Objects
[*] Users, Roles, Groups tables exist with TOAST metadata and UUID references.
[*] Role memberships table exists.

Key references:
- `src/core/catalog_manager.cpp:1864-1935` (User/Role/Group/RoleMembership records)
- `src/core/catalog_manager.cpp:3118-3181` (bootstrap SYSTEM/PUBLIC/DB_OWNER)

### 11) Emulation Support
[~] Emulation Types/Servers/Databases tables exist with CRUD and TOAST mapping/config metadata.
[ ] No code observed that **creates emulation schema views** as part of emulation database creation.

Key references:
- `src/core/catalog_manager.cpp:2100-2148` (Emulation records)
- `src/core/catalog_manager.cpp:36890-37560` (Emulation CRUD)

### 12) Missing System Tables List
[~] Dependencies, Comments, Users/Roles/Groups, Role Memberships, Procedures + Parameters, UDR, Packages, Emulation tables exist.
[~] Domains table is **removed** (handled by DomainManager), which conflicts with this spec’s requirement for a domains catalog table.

Key references:
- `src/core/catalog_manager.cpp:2067-2106` (ProcedureParameter/UDR/Package records)
- `src/core/catalog_manager.cpp:2087-2093` (Domain record removed comment)

### 13) Search Path and Session State
[*] `search_path_oid` is removed from SchemaRecord (session-only).
[ ] Default search_path in resolver is `{"public"}`; spec requires current schema → sys → public.
[ ] Absolute path semantics in the parser do **not** use leading dot; `.` is CURRENT, not ABSOLUTE. The spec example `.root.sys.sec.table` does not map to ABSOLUTE in v3 parser.

Key references:
- `src/core/catalog_manager.cpp:1276-1298` (SchemaRecord; no search_path_oid)
- `src/core/catalog_manager.cpp:6010-6066` (default search_path = public)
- `src/parser/schema_path_v3.cpp:70-160` (leading `.` means CURRENT; ABSOLUTE has no prefix)

## Key References
- Schema + hierarchy bootstrap: `src/core/catalog_manager.cpp:3021-3111`
- Catalog disk records: `src/core/catalog_manager.cpp:1276-2148`
- Resolver/search_path: `src/core/catalog_manager.cpp:5993-6660`
- Schema path parsing: `src/parser/schema_path_v3.cpp:70-160`
