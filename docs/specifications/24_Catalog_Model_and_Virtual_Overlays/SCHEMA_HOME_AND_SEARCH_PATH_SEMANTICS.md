# Schema Home And Search Path Semantics

## Purpose
Define deterministic home-schema assignment and search-path resolution rules for all sessions.

## Scope
- Native and emulated session startup.
- Home schema derivation.
- Current schema requirements.
- Search path ordering and object resolution.

## Non-Negotiable Rules
1. Every session must have a `current_schema_uuid`.
2. Every session must have an ordered search path.
3. Current schema may differ from the first search path entry.
4. Unqualified names resolve by first match in search-path order.
5. Fully qualified names bypass search-path scanning.

## Home Schema Resolution Order (Native)
Evaluate in order and stop at first valid result:
1. User personal home schema: `root.users.<user_name>` or scoped user path (`root.users.<workgroup_name>.<user_name>`, `root.users.<cluster_scope_path...>.<user_name>`).
2. Role home schema: `root.users.roles.<active_role_name>`.
3. Group home schema: `root.users.groups.<effective_group_name>`.
Rule for multiple eligible groups:
- choose the binding with highest priority.
- priority ordering is numeric `precedence` ascending (`0` is highest priority).
- if multiple bindings have the same `precedence`, tie-break by `group_uuid` ascending.
4. Fallback schema: `root.users.public`.

If no candidate exists, session startup fails with `HOME_SCHEMA_NOT_FOUND`.

## Home Schema Resolution (Emulated Parsers)
For emulated parser sessions:
1. Home schema is the emulated database base schema.
2. Base schema format:
`root.remote.emulation.<dialect>.<server_alias>.databases.<db_name>`
3. If the base schema does not exist, parser must fail session start with dialect-native connect error.

## Search Path Construction
At session start:
1. Load assigned search path profile if present.
2. If absent, construct default path:
- native: `[home_schema, root.users.public]` (deduplicate).
- emulated: `[emulated_base_schema, dialect_catalog_schema...]`.
3. Deduplicate while preserving first occurrence.
4. Validate each schema UUID exists and is visible to session security context.

## Search Path Mutability
1. Search path is a session setting.
2. A default search path may be inherited from user, role, group, or emulated profile settings.
3. Session search path may be altered at runtime.
4. Altering search path does not change `home_schema_uuid`.
5. Altering search path may change unqualified name resolution immediately for subsequent statements.

## Unqualified Name Resolution Algorithm
Given `(object_type, identifier)`:
1. For each schema in search path order:
- normalize identifier according to parser/dialect rules.
- lookup in name registry by `(parent_schema_uuid, object_type, canonical_name)`.
2. Return first match.
3. If no match, return `NAME_NOT_FOUND`.

## Example
Search path:
1. `root.users.daltoncalford`
2. `root.users.public`

Query:
`select a.*, b.* from foo a join bar b on a.key = b.key`

Resolution:
1. `foo` checked in `root.users.daltoncalford`, then `root.users.public`.
2. `bar` checked in same order.
3. First match in ordered path is used for each name.

## Collision Rule
If two objects of same type and canonical name exist in different search-path schemas, resolution is deterministic by search-path order only.

## Catalog Requirements
The following catalogs must exist:
1. `home_schema_binding`
2. `search_path_profile`
3. `search_path_entry`

## Test Clauses
1. Session startup fails when home schema cannot be resolved.
2. Current schema is always non-null for active sessions.
3. Search-path order controls duplicate-name resolution deterministically.
4. Emulated session always resolves base schema as first path entry.
5. Fully qualified path resolves regardless of search path order.
6. Multiple-group home-schema selection uses highest priority and deterministic tie-break (`group_uuid` ascending).
