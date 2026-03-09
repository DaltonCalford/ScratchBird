# Specification: ACL Format

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/authorization/acl |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:5216-5294`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/permission_cache.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_security_acl_abac_graph_token_quota_settings_contract.cpp`

## Synopsis

This specification defines the Access Control List (ACL) storage format in ScratchBird, including permission record structures, ACL item encoding, and the catalog storage layout for grants and permissions.

## Scope

### In Scope

- Permission record storage structures
- ACL item binary format
- ACL textual representation
- Privilege bitmask encoding
- Grant option tracking

### Out of Scope

- Permission checking algorithms (see `authorization_model.md`)
- GRANT/REVOKE SQL syntax
- Permission cache implementation

## Background

ScratchBird stores permissions in the system catalog using an ACL (Access Control List) format compatible with PostgreSQL. Each database object that supports privileges has an associated ACL.

## Specification

### Permission Record Structure

```cpp
// From catalog_manager.cpp:5216-5228

struct PermissionRecord {
    ID permission_id;           // UUIDv7 - unique identifier
    ID grantee_id;              // User/role UUID receiving permission
    ID grantor_id;              // User UUID who granted permission
    ID object_id;               // Object UUID (table, schema, etc.)
    uint32_t permissions;       // Bitmask of privileges
    uint8_t grant_option;       // 1 if WITH GRANT OPTION
    uint8_t object_type;        // ObjectType enum value
    // ... timestamps
};
```

### ACL Item Binary Format

An ACL item represents a single grant entry:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ACL Item Binary Format                        │
├─────────────────┬─────────┬─────────────────────────────────────────┤
│ Field           │ Size    │ Description                             │
├─────────────────┼─────────┼─────────────────────────────────────────┤
│ grantee_id      │ 16 bytes│ UUID of grantee (user/role)             │
│ grantor_id      │ 16 bytes│ UUID of grantor                         │
│ permissions     │ 4 bytes │ Privilege bitmask                       │
│ grant_option    │ 1 byte  │ 1 if grant option included              │
│ padding         │ 3 bytes │ Alignment padding                       │
├─────────────────┼─────────┼─────────────────────────────────────────┤
│ Total           │ 40 bytes│ Per ACL item                            │
└─────────────────┴─────────┴─────────────────────────────────────────┘
```

### ACL Text Format

The textual representation is used for display and system catalog storage:

```
aclitem = grantee=privs/grantor

Examples:
  "alice=arwdDxt/alice"      -- alice has ALL on table she owns
  "bob=r/alice"              -- bob has SELECT granted by alice
  "group_staff=ar*/alice"    -- group_staff has ALL with grant option
  "=r/alice"                 -- PUBLIC has SELECT
```

**Privilege Abbreviations:**

| Abbrev | Privilege | Abbrev | Privilege |
|--------|-----------|--------|-----------|
| `a` | INSERT | `r` | SELECT |
| `w` | UPDATE | `d` | DELETE |
| `D` | TRUNCATE | `x` | REFERENCES |
| `t` | TRIGGER | `X` | EXECUTE |
| `U` | USAGE | `C` | CREATE |
| `c` | CONNECT | `T` | TEMPORARY |

**Suffixes:**
- `*` - WITH GRANT OPTION included

### Full ACL Example

```
Text: {alice=arwdDxt/alice,bob=rw/alice,=r/alice}

Decodes to:
- alice: ALL (INSERT, SELECT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER)
         granted by alice (owner)
- bob: SELECT, UPDATE granted by alice
- "": SELECT granted to PUBLIC by alice
```

### Catalog Storage Layout

```
System Catalog Tables:

┌─────────────────────────────────────────────────────────────────┐
│ sb_permissions                                                  │
├─────────────────┬─────────────────┬─────────────────────────────┤
│ permission_id   │ UUID (PK)       │ Unique permission ID        │
│ grantee_id      │ UUID (FK)       │ User/role receiving grant   │
│ grantor_id      │ UUID (FK)       │ User who made grant         │
│ object_id       │ UUID (FK)       │ Object being granted on     │
│ object_type     │ uint8           │ Type of object              │
│ privileges      │ uint32          │ Privilege bitmask           │
│ grant_option    │ uint8           │ Has grant option?           │
│ created_at      │ timestamp       │ When granted                │
│ updated_at      │ timestamp       │ Last modification           │
└─────────────────┴─────────────────┴─────────────────────────────┘

Indexes:
- (grantee_id, object_id, object_type) - Permission lookup
- (object_id) - Object permission enumeration
- (grantor_id) - Grantor tracking
```

### Column Permission Storage

```cpp
// From catalog_manager.cpp:5253-5264

struct ColumnPermissionRecord {
    ID permission_id;        // UUIDv7
    ID grantee_id;           // User/role UUID
    ID grantor_id;           // Who granted
    ID object_id;            // Table UUID
    uint32_t column_id;      // Column ordinal position
    uint32_t permissions;    // Column-specific bitmask
    uint8_t grant_option;
};
```

```
┌─────────────────────────────────────────────────────────────────┐
│ sb_column_permissions                                           │
├─────────────────┬─────────────────┬─────────────────────────────┤
│ permission_id   │ UUID (PK)       │ Unique permission ID        │
│ grantee_id      │ UUID (FK)       │ User/role receiving grant   │
│ grantor_id      │ UUID (FK)       │ User who made grant         │
│ object_id       │ UUID (FK)       │ Table being granted on      │
│ column_id       │ uint32          │ Column ordinal (0-based)    │
│ privileges      │ uint32          │ Privilege bitmask           │
│ grant_option    │ uint8           │ Has grant option?           │
└─────────────────┴─────────────────┴─────────────────────────────┘

Indexes:
- (grantee_id, object_id, column_id) - Column permission lookup
- (object_id, column_id) - Column permission enumeration
```

### Permission Bitmask Encoding

```cpp
// Object-level permissions
enum ObjectPermissions : uint32_t {
    EXECUTE     = 1 << 0,   // 0x0001
    SELECT      = 1 << 1,   // 0x0002
    INSERT      = 1 << 2,   // 0x0004
    UPDATE      = 1 << 3,   // 0x0008
    DELETE      = 1 << 4,   // 0x0010
    USAGE       = 1 << 5,   // 0x0020
    CREATE      = 1 << 6,   // 0x0040
    DROP        = 1 << 7,   // 0x0080
    ALTER       = 1 << 8,   // 0x0100
    INDEX       = 1 << 9,   // 0x0200
    TRIGGER     = 1 << 10,  // 0x0400
    REFERENCES  = 1 << 11,  // 0x0800
    TRUNCATE    = 1 << 12,  // 0x1000
    CONNECT     = 1 << 13,  // 0x2000
    TEMPORARY   = 1 << 14,  // 0x4000
};

// Column-level permissions
enum ColumnPermission : uint32_t {
    COLUMN_SELECT     = 1 << 0,  // 0x01
    COLUMN_UPDATE     = 1 << 1,  // 0x02
    COLUMN_REFERENCES = 1 << 2,  // 0x04
};
```

### ACL Encoding/Decoding

#### Encode to Text

```
Function: encodeAclItem(grantee, privileges, grantor, has_grant_option)

1. FORMAT grantee name:
   - Empty string for PUBLIC
   - User/role name otherwise

2. FORMAT privileges:
   result = ""
   if privileges & INSERT:     result += "a"
   if privileges & SELECT:     result += "r"
   if privileges & UPDATE:     result += "w"
   if privileges & DELETE:     result += "d"
   if privileges & TRUNCATE:   result += "D"
   if privileges & REFERENCES: result += "x"
   if privileges & TRIGGER:    result += "t"
   if privileges & EXECUTE:    result += "X"
   if privileges & USAGE:      result += "U"
   if privileges & CREATE:     result += "C"
   if privileges & CONNECT:    result += "c"
   if privileges & TEMPORARY:  result += "T"

3. ADD grant option suffix:
   if has_grant_option: result += "*"

4. FORMAT: "grantee=privileges/grantor"

Example:
  grantee="bob", privs=SELECT|UPDATE, grantor="alice", has_grant_option=false
  Result: "bob=rw/alice"
```

#### Decode from Text

```
Function: decodeAclItem(text)

1. SPLIT at '=':
   grantee_part = before '='
   rest = after '='

2. HANDLE grantee:
   if grantee_part == "": grantee = PUBLIC
   else: grantee = lookupUser(grantee_part)

3. SPLIT rest at '/':
   privs_part = before '/'
   grantor_part = after '/'

4. PARSE privileges:
   for each char in privs_part:
     'a' -> INSERT
     'r' -> SELECT
     'w' -> UPDATE
     'd' -> DELETE
     'D' -> TRUNCATE
     'x' -> REFERENCES
     't' -> TRIGGER
     'X' -> EXECUTE
     'U' -> USAGE
     'C' -> CREATE
     'c' -> CONNECT
     'T' -> TEMPORARY
     '*' -> set grant_option flag

5. LOOKUP grantor by name

6. RETURN (grantee, privileges, grantor, grant_option)
```

### Object ACL Storage

Different object types store ACLs differently:

```
Object Type    │ ACL Storage Location
───────────────┼─────────────────────────────────────
Table          │ pg_class.relacl column
Column         │ pg_attribute.attacl column
Schema         │ pg_namespace.nspacl column
Database       │ pg_database.datacl column
Function       │ pg_proc.proacl column
Language       │ pg_language.lanacl column
Tablespace     │ pg_tablespace.spcacl column
Type/Domain    │ pg_type.typacl column
Large Object   │ pg_largeobject_metadata.lomacl
```

### ACL Modification

```
Algorithm: Update ACL on GRANT

Input: object, grantee, privileges, grant_option, grantor
Output: Updated ACL

1. LOAD current ACL for object

2. FIND existing grant to grantee:
   existing = findInAcl(acl, grantee)

3. IF existing found:
     // Merge with existing
     existing.privileges |= privileges
     existing.grant_option |= grant_option
     existing.grantor = grantor  // Update grantor
   ELSE:
     // Create new ACL item
     new_item = {
       grantee: grantee,
       privileges: privileges,
       grant_option: grant_option,
       grantor: grantor
     }
     acl.add(new_item)

4. WRITE updated ACL to catalog

5. INVALIDATE permission cache
```

```
Algorithm: Update ACL on REVOKE

Input: object, grantee, privileges, grant_option, cascade
Output: Updated ACL

1. LOAD current ACL for object

2. FIND existing grant to grantee:
   existing = findInAcl(acl, grantee)

3. IF not found: RETURN (nothing to revoke)

4. IF grant_option only:
     // Revoking grant option only
     existing.grant_option = false
   ELSE IF privileges specified:
     // Revoke specific privileges
     existing.privileges &= ~privileges
     IF existing.privileges == 0:
       acl.remove(existing)
   ELSE:
     // Revoke all privileges
     acl.remove(existing)

5. IF cascade:
     // Find all grants made by this grantee
     dependent = findGrantsByGrantor(grantee, object)
     for each grant in dependent:
       revokeGrant(grant, cascade)

6. WRITE updated ACL to catalog

7. INVALIDATE permission cache
```

## Invariants

1. **Owner Present**: Object owner always has entry with all privileges
   - Verification: Owner entry added on object creation

2. **No Duplicate Grants**: Single entry per (object, grantee) pair
   - Verification: Merge on duplicate grant

3. **Grantor Valid**: Grantor must have privilege with grant option
   - Verification: Check before GRANT operation

4. **Valid Privilege Sets**: Only valid privilege combinations stored
   - Verification: Validate against object type

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `INVALID_ACL_FORMAT` | Cannot parse ACL text | Fix catalog data |
| `DUPLICATE_GRANT` | Grant already exists | Use UPDATE instead |
| `MISSING_GRANT` | Revoke non-existent grant | No action needed |
| `DEPENDENT_GRANTS` | Cascade required | Use CASCADE or RESTRICT |

## Related Specifications

- `authorization_model.md` - Authorization and permission checking
- `privilege_types.md` - Detailed privilege definitions
- `default_privileges.md` - Default privileges system

## Appendix

### ACL Text Examples

```
-- Table owned by alice with various grants
{alice=arwdDxt/alice,"group staff"=r/alice,bob=rw*/alice,=r/alice}

Decodes to:
- alice: ALL (owner)
- group staff: SELECT (no grant option)
- bob: SELECT, UPDATE with grant option
- PUBLIC: SELECT

-- Function ACL
{alice=X/alice,public=X/alice}

Decodes to:
- alice: EXECUTE (owner)
- public: EXECUTE
```

### PostgreSQL Compatibility

ScratchBird ACL format is fully compatible with PostgreSQL:
- Same text format
- Same privilege abbreviations
- Same bitmask values

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
