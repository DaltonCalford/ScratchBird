Status: reconstructed_required_with_current_substrate

# USER ROLE GROUP MEMBERSHIP HOME SCHEMA AND EXTERNAL MAPPING MODEL

## Purpose

This file defines the canonical principal-membership model for ScratchBird users, roles,
groups, external-group mappings, effective membership expansion, and session home-schema
selection.

This area is in rebuild state. The current code already persists and evaluates users,
roles, groups, external mappings, effective-role closure, effective-group closure, and
home-schema resolution. The canon below records both:

- current code-backed authority
- required reconstructed behavior where the old specification intent is stronger than the
  currently recovered implementation

## Principal classes

ScratchBird authorization uses the following principal classes:

- `USER`
- `ROLE`
- `GROUP`
- `PUBLIC`
- `SUPERUSER`

`USER` is the login and ownership principal.

`ROLE` is a grantable privilege bundle and may itself receive grants from other roles.

`GROUP` is a shared-rights aggregation principal and may be local or externally mapped.

`PUBLIC` is the ambient grantee class for object and permission grants that apply to all
sessions.

`SUPERUSER` is a direct authorization short-circuit. If a user account is marked
superuser, normal permission search terminates successfully.

## Current catalog-backed principal records

### Role

Current code-backed role records carry:

- `role_id`
- `role_name`
- `owner_id`
- `role_metadata`
- `default_schema_id`
- `is_active`
- `created_time`
- `last_modified_time`

### Group

Current code-backed group records carry:

- `group_id`
- `group_name`
- `external_id`
- `group_type`
- `group_metadata`
- `default_schema_id`
- `created_time`
- `last_modified_time`

### External group mapping

Current code-backed external-group mapping records carry:

- `mapping_id`
- `external_group_name`
- `auth_method`
- `auto_create_users`
- `internal_group_id`
- `created_time`
- `last_modified_time`

Current recovered `auth_method` families are:

- `LDAP`
- `KERBEROS`
- `ACTIVE_DIRECTORY`

## Membership records

### Role memberships

Role membership records bind:

- `membership_id`
- `user_id`
- `role_id`
- `granted_by`
- `with_admin_option`
- `granted_time`

The current implementation uses `user_id` as the generic principal field for both:

- direct user-to-role membership
- role-to-role membership used for transitive role closure

The canon therefore treats role membership as principal-to-role, even though the current
record field name is user-shaped.

### Group memberships

Group membership records bind:

- `membership_id`
- `group_id`
- `user_id` as the generic member principal id
- `member_type`
- `granted_by`
- `granted_time`

`member_type = USER` means a direct user member.

`member_type = GROUP` means a nested group member.

The canon therefore treats group membership as principal-to-group, even though the current
record field name is user-shaped.

## Effective membership closure

### Effective roles

Current code-backed role expansion uses breadth-first traversal:

1. seed the traversal with the session user principal
2. load direct memberships for the current principal
3. append each previously unseen role to the effective set
4. enqueue each newly found role as a principal that may itself hold more roles
5. continue until the queue is empty

This produces:

- direct roles
- roles granted to those roles
- all further transitive role grants

### Effective groups

Current code-backed group expansion also uses breadth-first traversal:

1. seed the traversal with the session user principal
2. load groups for the current principal
3. append each previously unseen group to the effective set
4. enqueue each newly found group as a principal that may itself belong to more groups
5. continue until the queue is empty

This produces:

- direct groups
- nested groups
- all further transitive group memberships

## Required cycle rules

Current code-backed behavior already prevents role-grant cycles before inserting a new
role membership.

Required reconstructed behavior is stronger:

- role membership admission must remain cycle-free
- group membership admission must also be cycle-free
- any principal-to-principal membership graph that would make effective-closure traversal
  self-referential must be rejected before catalog publication

Implementation note:

- the current recovered `addGroupMember` path supports nested groups but does not show the
  same explicit cycle detection that already exists for role grants
- this is implementation drift, not specification permission

## Security epoch and cache interaction

Current code-backed role grant and revoke operations:

- persist the membership mutation
- bump the security-policy epoch
- invalidate the affected user in the permission cache

Required reconstructed behavior applies the same cache and epoch discipline to group and
external-mapping mutations that change effective authorization.

## Home schema model

ScratchBird sessions carry:

- `home_schema_id`
- `current_schema_id`
- `search_path_profile_id`
- ordered `search_path_schema_ids`
- ordered `search_path`

Current code-backed home-schema resolution evaluates candidates from:

- the user principal
- each effective role
- each effective group

The current selection order is:

1. user default schema
2. role default schemas
3. group default schemas ordered by explicit precedence where present
4. `users.public`
5. `public`

Where native canonical mode is in effect, `users.public` may be canonicalized back to
`public` for session-facing native behavior.

## Search-path persistence model

Current code-backed behavior persists home-schema and search-path binding state in catalog
tables rather than treating it as process-only metadata.

This means:

- effective membership influences schema context deterministically
- the resulting home-schema binding is catalog-backed
- session search-path materialization is derived from published membership state

## Overlay-schema materialization

Current code-backed group create and membership paths also materialize overlay schema
children for group and group-member visibility mounts.

This means the security model is not only permission-graph based. It also has a namespace
projection side effect:

- group creation materializes a group overlay child
- adding a member materializes a nested overlay child under that group
- removing a member removes the nested overlay child when no remaining membership survives

Required reconstructed rule:

- overlay publication must remain subordinate to committed catalog membership state
- failed overlay materialization must fail the membership change, not silently diverge from
  the catalog graph

## External group mapping model

Current code-backed external-group mapping is a bridge from an external authenticated group
identity to an internal `GROUP`.

The canonical contract is:

1. external authentication completes first
2. external group identities are collected from the provider
3. matching group mappings are resolved by `auth_method + external_group_name`
4. the mapped internal groups are added to the session's effective-group seed set
5. normal group closure then expands nested groups transitively

`auto_create_users` is a controlled admission flag. It does not bypass later authorization
or sandbox rules.

## Shared-rights model

Shared rights in ScratchBird are granted through:

- direct user permissions
- role memberships and transitive role closure
- group memberships and transitive group closure
- public grants
- security-definer and sandboxed execution surfaces defined elsewhere in section `19`

The session therefore sees shared rights as a resolved principal graph, not as a single
flat role list.

## Required reconstructed behavior

The recovered specification requires the following even where implementation is still
catching up:

- full parity between role and group cycle protection
- catalog-backed epoch and cache invalidation for all authorization-affecting membership
  changes
- external-group mapping changes must be transaction-scoped and published only on commit
- session home-schema and search-path resolution must always be derived from committed
  principal state, never from out-of-band listener or manager hints

## Non-authority and fail-closed boundary

The following are not authoritative:

- listener-local security guesses about effective roles or groups
- client-side caching of principal membership as a permission source of truth
- external provider claims that have not been mapped into committed ScratchBird principal
  state

If principal closure, external mapping, or overlay publication cannot be reconciled, the
engine must fail closed and refuse the affected membership or session-binding publication.
