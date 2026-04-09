# Specification: UUID Mapping and Identity

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:92`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:76`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/catalog/sys_catalog.cpp:38`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_database_bootstrap.cpp:73`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_session_epoch_pinning.cpp:1`

## Synopsis

This specification defines UUID (Universally Unique Identifier) generation, assignment, and mapping rules for ScratchBird's catalog system. UUIDv7 is used as the primary identifier type for all database objects, providing time-ordered, globally unique identifiers that enable distributed identity without central coordination.

## Scope

### In Scope

- UUIDv7 generation rules and format
- Database UUID assignment and immutability
- Catalog table UUID assignment
- Row UUID stability across versions
- System domain UUID registry
- UUID-to-name resolution mechanisms
- Zero UUID semantics

### Out of Scope

- Specific catalog table schemas (see `catalog_table_layouts.md`)
- Bootstrap sequence (see `bootstrap_sequence.md`)
- Object naming conventions (see `object_identity_rules.md`)

## Background

ScratchBird uses UUIDv7 as its primary identifier format. Unlike traditional sequence-based OIDs (PostgreSQL) or generator-based IDs (Firebird), UUIDv7 provides:

1. **Time ordering**: Lexicographically sortable by creation time
2. **Global uniqueness**: No central coordination required
3. **Distributed safety**: Safe for multi-master replication
4. **Privacy**: No MAC address or other host information embedded

## Specification

### UUIDv7 Format

**Source:** `src/core/uuidv7.h` (implied by usage)

UUIDv7 structure (16 bytes):

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           unix_ts_ms                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          unix_ts_ms           |  ver  |       rand_a            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|var|                        rand_b                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            rand_b                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **unix_ts_ms** (48 bits): Unix timestamp in milliseconds
- **ver** (4 bits): Version = 0b0111 (7)
- **rand_a** (12 bits): Random data
- **var** (2 bits): Variant = 0b10
- **rand_b** (62 bits): Random data

### ID Type Definition

**Source:** `include/scratchbird/core/catalog_manager.h:76`

```cpp
using ID = UuidV7Bytes;

struct UuidV7Bytes {
    std::array<uint8_t, 16> bytes;
    
    bool operator==(const UuidV7Bytes& other) const {
        return bytes == other.bytes;
    }
};
```

### Zero UUID Semantics

**Source:** `src/catalog/sys_catalog.cpp:38`

```cpp
bool isZeroId(const core::ID& id) {
    for (uint8_t byte : id.bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}
```

The Zero UUID (`00000000-0000-0000-0000-000000000000`) has special semantics:

| Context | Zero UUID Meaning |
|---------|-------------------|
| `parent_schema_id` | Root schema (no parent) |
| `owner_id` | SYSTEM owner |
| `tablespace_id` | Default tablespace |
| `toast_table_id` | No TOAST table |
| `domain_id` | Not a domain-based column |
| `default_schema_id` | No default schema set |

### Database UUID

**Source:** `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md`

On database creation:

```cpp
// Generate new UUIDv7 for database identity
ID database_uuid = UUIDv7Generator::generate();

// Stored in:
// 1. Database bootstrap header
// 2. database catalog table
// 3. Root schema ID (SCHEMA_INV_001 invariant)
```

**Properties:**
- Generated once at database creation
- Immutable for database lifetime
- Identity boundary for workgroup/cluster collision detection
- Stored in both bootstrap header and `database` catalog table

### Catalog Table UUIDs

Each catalog table is a normal database object with its own `object_uuid`:

| Property | Rule |
|----------|------|
| Generation | At table creation time |
| Uniqueness | Unique per database (not fixed across databases) |
| Purpose | Prevents accidental ID overlap in multi-database deployments |
| Storage | `object` and `object_name` catalog tables |

### Row UUIDs

Every row inserted into any table is assigned a `row_uuid`:

**Source:** `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md`

```cpp
// Row UUID is stored in record header
struct RecordHeader {
    ID row_uuid;           // Stable across all versions
    uint64_t xmin;         // Creating transaction
    uint64_t xmax;         // Expiring transaction
    // ... other fields
};
```

**Properties:**
- Assigned at INSERT time
- Remains stable across all versions of the same logical row
- Global identity key: `(database_uuid, row_uuid)`
- Used for workgroup and cluster collision detection

### Row UUID Surface Optimization

For tables with UUID primary keys:

```cpp
// If table defines UUID column as PRIMARY KEY with row_uuid_surface flag:
if (column.is_row_uuid_surface) {
    // Column value derived from row_uuid header
    // Storage omits column payload
    // Catalog records is_row_uuid to prevent writes
}
```

### System Domain UUID Registry

System domains use fixed UUID values:

| Domain | Fixed UUID | Purpose |
|--------|------------|---------|
| `SBDB$KEY_TABLESPACE` | `018e...` | Tablespace references |
| `SBDB$KEY_CHARSET` | `018f...` | Character set references |
| `SBDB$KEY_TIMEZONE` | `0190...` | Timezone references |
| `SBDB$KEY_COLLATION` | `0191...` | Collation references |

**Source:** `docs/specifications/15_Complex_Types/SYSTEM_DOMAIN_UUID_REGISTRY.md`

### UUID Comparison

**Source:** `src/core/catalog_manager.cpp:103`

```cpp
int compareUuidBytesLocal(const ID& lhs, const ID& rhs) {
    return std::memcmp(lhs.bytes.data(), rhs.bytes.data(), lhs.bytes.size());
}
```

UUID comparison uses lexicographic byte order (memcmp), which for UUIDv7 provides time-based ordering.

### UUID Validation

**Source:** `src/core/catalog_manager.cpp:92`

```cpp
bool isUuidV7Local(const ID& id) {
    std::vector<uint8_t> bytes;
    bytes.reserve(16);
    for (auto b : id.bytes) {
        bytes.push_back(b);
    }
    return TypeExtractor::extractUUIDVersion(bytes) == 7;
}
```

## Algorithms

### Algorithm: Generate Object UUID

```
Input:  Object type, parent schema UUID
Output: New UUIDv7 for object

1. Generate UUIDv7 using timestamp + random
2. Verify version bits = 0b0111
3. Verify variant bits = 0b10
4. Return UUID
```

**Complexity:**
- Time: O(1)
- Space: O(1)

### Algorithm: Resolve Name to UUID

```
Input:  Schema path, object name, object type
Output: Object UUID or NOT_FOUND

1. Compute canonical name (uppercase for non-delimited)
2. Build lookup key: (schema_path, object_type, canonical_name)
3. Query object_name catalog table index
4. If found: return object_id from record
5. If not found: return NOT_FOUND
```

**Complexity:**
- Time: O(log n) with index
- Space: O(1)

### Algorithm: Collision Detection

```
Input:  Database UUID, Row UUID
Output: Collision status

1. For workgroup/cluster context:
   a. Extract database_uuid from row location
   b. Build composite key: (database_uuid, row_uuid)
   c. Check against known identity set
2. If collision detected:
   a. Generate new row UUID
   b. Log collision event
   c. Retry with new UUID
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|--------------|
| `UUID_INV_001` | All catalog object IDs are valid UUIDv7 | `isUuidV7Local()` check |
| `UUID_INV_002` | Database UUID is immutable | Header checksum + catalog consistency |
| `UUID_INV_003` | Row UUIDs are unique within database | Primary key constraint |
| `UUID_INV_004` | Zero UUID has consistent semantics | Schema invariant validation |
| `UUID_INV_005` | System domain UUIDs are fixed values | Registry validation at startup |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `INVALID_UUID` | UUID fails version check | Reject operation |
| `DUPLICATE_UUID` | UUID collision detected | Generate new UUID, retry |
| `ZERO_UUID_UNEXPECTED` | Zero UUID where non-zero required | Return error, log incident |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_catalog_database_bootstrap.cpp` | Database UUID immutability |
| `tests/unit/test_catalog_session_epoch_pinning.cpp` | UUID stability across epochs |
| `tests/unit/test_catalog_type_schema_contract.cpp` | System domain UUID registry |

## Migration Notes

- UUIDv7 migration from sequence-based IDs is not supported (greenfield only)
- External UUIDs (imported data) must be validated before storage
- UUID version check should allow future versions with warning

## Related Specifications

- `bootstrap_sequence.md` - Bootstrap sequence using UUIDs
- `object_identity_rules.md` - Object resolution using UUIDs
- `catalog_table_layouts.md` - Catalog tables storing UUIDs

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| UUIDv7 | RFC 9562 UUID version 7 (time-ordered) |
| Zero UUID | All-zero UUID with special semantics |
| Row UUID | Stable identifier for logical row across versions |
| System Domain | Built-in type with fixed UUID |

### References

- `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/UUID_IDENTITY_AND_COLLISION_RULES.md`
- RFC 9562: Universally Unique Identifiers (UUIDs)

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
