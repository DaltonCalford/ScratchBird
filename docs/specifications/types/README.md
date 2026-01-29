# Type System Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains data type system specifications for ScratchBird's comprehensive type system, including built-in types, domains, arrays, and specialized types.

## Overview

ScratchBird implements a rich type system supporting standard SQL types, PostgreSQL-compatible arrays, geometric types, UUIDs, and user-defined domains. The type system integrates with the MGA storage layer and supports type coercion, casting, and persistence.

## SBLR Encoding Coverage (Alpha)
SBLR type markers and literal opcodes must exist for all DataTypes to be fully
executable in the bytecode VM. Coverage gaps and planned opcode additions are
tracked in:
- `docs/specifications/sblr/SBLR_OPCODE_REGISTRY.md`
- `docs/findings/SBLR_TYPE_OPCODE_GAPS.md`

## Specifications in this Directory

### Core Type System

- **[03_TYPES_AND_DOMAINS.md](03_TYPES_AND_DOMAINS.md)** (285 lines) - Type system overview and domain specifications
- **[DATA_TYPE_PERSISTENCE_AND_CASTS.md](DATA_TYPE_PERSISTENCE_AND_CASTS.md)** (129 lines) - Type storage format and casting rules

### User-Defined Domains

- **[DDL_DOMAINS_COMPREHENSIVE.md](DDL_DOMAINS_COMPREHENSIVE.md)** (963 lines) - **Complete** domain specification
  - Simple domains with constraints
  - Enum domains
  - Set domains
  - Variant (tagged union) domains
  - Advanced domain features

### Array Types

- **[POSTGRESQL_ARRAY_TYPE_SPEC.md](POSTGRESQL_ARRAY_TYPE_SPEC.md)** (883 lines) - PostgreSQL-compatible array types
  - Multidimensional arrays
  - Array operators and functions
  - Array storage format

### Specialized Types

- **[UUID_IDENTITY_COLUMNS.md](UUID_IDENTITY_COLUMNS.md)** (536 lines) - UUID type and identity column support
- **[MULTI_GEOMETRY_TYPES_SPEC.md](MULTI_GEOMETRY_TYPES_SPEC.md)** (718 lines) - Geometric types (POINT, LINE, POLYGON, etc.)
- **[character_sets_and_collations.md](character_sets_and_collations.md)** (780 lines) - Character set and collation support
- **[COLLATION_TAILORING_LOADER_SPEC.md](COLLATION_TAILORING_LOADER_SPEC.md)** (stub) - Collation tailoring loader contract
- **[I18N_CANONICAL_LISTS.md](I18N_CANONICAL_LISTS.md)** - Canonical charset/collation lists and conformance reporting

### Time & Timezone

- **[TIMEZONE_SYSTEM_CATALOG.md](TIMEZONE_SYSTEM_CATALOG.md)** - Timezone support and IANA timezone database integration

## Supported Type Categories

### Numeric Types
- **Integer:** SMALLINT, INTEGER, BIGINT, INT128
- **Decimal:** NUMERIC, DECIMAL, MONEY, DECFLOAT(16/34)
- **Floating-point:** REAL, DOUBLE PRECISION

### Character Types
- **Fixed-length:** CHAR(n)
- **Variable-length:** VARCHAR(n), TEXT
- **Binary:** BYTEA

### Date/Time Types
- **DATE** - Calendar date
- **TIME** - Time of day
- **TIMESTAMP** - Date and time
- **TIMESTAMP WITH TIME ZONE** - Date and time with timezone
- **INTERVAL** - Time interval

### Boolean
- **BOOLEAN** - True/false/null

### Binary Types
- **BYTEA** - Variable-length binary data
- **BLOB** - Binary large object

### Specialized Types
- **UUID** - Universally unique identifier (v4, v7, v8)
- **JSON / JSONB** - JSON data
- **XML** - XML documents
- **Arrays** - Multidimensional arrays of any type
- **Geometric** - POINT, LINE, POLYGON, CIRCLE, etc.
- **Network** - INET, CIDR, MACADDR

### User-Defined Types
- **Domains** - Constrained built-in types
- **Enum domains** - Enumerated types
- **Set domains** - Set membership types
- **Variant domains** - Tagged unions (sum types)
- **Composite types** - Row types

## Domain System

ScratchBird supports advanced domain features:

### Simple Domains
```sql
CREATE DOMAIN email_address AS VARCHAR(255)
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}$');
```

### Enum Domains
```sql
CREATE DOMAIN order_status AS ENUM (
    'pending', 'processing', 'shipped', 'delivered', 'cancelled'
);
```

### Set Domains
```sql
CREATE DOMAIN permission_set AS SET (
    'read', 'write', 'execute', 'admin'
);
```

### Variant Domains
```sql
CREATE DOMAIN payment_method AS VARIANT (
    credit_card { card_number VARCHAR(19), cvv CHAR(3) },
    paypal { email VARCHAR(255) },
    bank_transfer { account_number VARCHAR(20) }
);
```

## Type Coercion & Casting

See [DATA_TYPE_PERSISTENCE_AND_CASTS.md](DATA_TYPE_PERSISTENCE_AND_CASTS.md) for:

- **Implicit casts** - Automatic type conversion
- **Explicit casts** - CAST() and :: operators
- **Assignment casts** - Conversion on assignment
- **Storage format** - On-disk type representation

## Related Specifications

- [Storage Engine](../storage/) - Type storage formats
- [Catalog](../catalog/) - Type definitions in system catalog
- [SBLR Bytecode](../sblr/) - Type-specific opcodes
- [DDL Operations](../ddl/) - CREATE DOMAIN, CREATE TYPE
- [Indexes](../indexes/) - Type-specific index support

## Critical Reading

Before working on type system implementation:

1. **MUST READ:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ IN ORDER:**
   - [03_TYPES_AND_DOMAINS.md](03_TYPES_AND_DOMAINS.md) - Type system overview
   - [../ddl/DDL_TYPES.md](../ddl/DDL_TYPES.md) - CREATE/ALTER/DROP TYPE
   - [DDL_DOMAINS_COMPREHENSIVE.md](DDL_DOMAINS_COMPREHENSIVE.md) - Domain specification
   - [DATA_TYPE_PERSISTENCE_AND_CASTS.md](DATA_TYPE_PERSISTENCE_AND_CASTS.md) - Storage and casts

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
