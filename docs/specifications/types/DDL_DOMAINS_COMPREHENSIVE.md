# ScratchBird Domain Specification (Comprehensive)

**Version:** 1.0 ALPHA
**Status:** SPECIFICATION DRAFT FOR PLAN 04
**Last Updated:** 2025-12-22
**Architecture Basis:** Firebird-compatible with SQL Standard support (dual syntax where conflicts exist)

---

## 1. OVERVIEW

### 1.1 Purpose

Domains in ScratchBird serve two primary purposes:

1. **Type Aliases with Constraints** (Traditional SQL-92 usage)
   - Named data types with CHECK constraints, DEFAULT values, and NOT NULL enforcement
   - Reusable across tables for consistency

2. **Transparent Datatype Conversion** (ScratchBird Extension)
   - Enable emulated database parsers to convert non-native types transparently
   - Example: MySQL's TINYINT(1) for BOOLEAN → Domain converts to/from ScratchBird BOOLEAN
   - Example: PostgreSQL's JSONB → Domain handles binary format conversion
   - Client compatibility without changing storage engine

### 1.2 Design Principles

1. **Dual Syntax Support** - When SQL Standard and Firebird conflict, implement BOTH for Firebird compatibility while allowing SQL Standard to be followed moving forward
2. **Parser Architecture**:
   - **ScratchBird Parser**: Context-aware design allows flexible keyword usage based on current parsing state
   - **Emulated Parsers** (Firebird/PostgreSQL/MySQL): Follow the actual engine's behavior including reserved keyword lists and grammar restrictions
   - Each parser implementation matches its target engine's limitations while ScratchBird retains its advanced context-aware design
3. **Explicit Over Implicit** - All features require explicit declaration
4. **Schema-Scoped** - Domains belong to schemas (not global like some databases)
5. **MGA-Compliant** - Domain constraints enforced using transaction visibility
6. **Dialect-Aware** - Support multi-dialect domain compatibility via `dialect_tag`

---

## 2. DOMAIN TYPES

ScratchBird supports FIVE domain types:

```
DomainType ::=
    BASIC       -- Type alias with constraints
  | RECORD      -- Composite/ROW type with named fields
  | ENUM        -- Enumerated type with ordered values
  | SET         -- Unordered collection of unique elements
  | VARIANT     -- Polymorphic type (runtime type checking)
```

---

## 3. BASIC DOMAINS (SQL-92 + Firebird Extensions)

### 3.1 Grammar (BNF)

```bnf
<create_basic_domain> ::=
    CREATE DOMAIN [<schema_name>.]<domain_name>
    [AS] <base_type>
    [<domain_options>]

<base_type> ::=
    <builtin_type> [(<precision> [, <scale>])]
  | CHAR [(<length>)]
  | VARCHAR [(<length>)]
  | DECIMAL [(<precision> [, <scale>])]
  | NUMERIC [(<precision> [, <scale>])]
  | -- ... all ScratchBird base types

<domain_options> ::=
    [DEFAULT <default_value>]
    [NOT NULL | NULL]
    [CHECK (<check_expression>)]
    [COLLATE <collation_name>]
    [WITH DIALECT (<dialect_tag>)]
    [WITH COMPAT (<compat_name>)]

<default_value> ::=
    <literal>
  | <expression>
  | CURRENT_TIMESTAMP
  | CURRENT_DATE
  | CURRENT_TIME
  | USER
  | SYSTEM_USER
  | GEN_UUID()

<check_expression> ::=
    <boolean_expression> using VALUE as domain value reference
```

### 3.2 Examples

```sql
-- Simple type alias
CREATE DOMAIN email_address AS VARCHAR(255);

-- With NOT NULL constraint
CREATE DOMAIN positive_integer AS INTEGER NOT NULL;

-- With CHECK constraint (VALUE references domain value)
CREATE DOMAIN percentage AS DECIMAL(5,2)
    CHECK (VALUE >= 0 AND VALUE <= 100);

-- With DEFAULT
CREATE DOMAIN status_flag AS CHAR(1)
    DEFAULT 'N'
    CHECK (VALUE IN ('Y', 'N'));

-- Dialect-specific domain for MySQL compatibility
CREATE DOMAIN mysql_tinyint AS SMALLINT
    CHECK (VALUE BETWEEN -128 AND 127)
    WITH DIALECT ('mysql')
    WITH COMPAT ('TINYINT');

-- Firebird-compatible TIMESTAMP domain
CREATE DOMAIN fb_timestamp AS TIMESTAMP
    DEFAULT CURRENT_TIMESTAMP
    WITH DIALECT ('firebird')
    WITH COMPAT ('TIMESTAMP');
```

### 3.3 Semantic Rules

1. **VALUE Keyword** - In CHECK constraints, `VALUE` references the domain's value being validated
2. **NULL Handling** - Default is NULL allowed unless NOT NULL specified
3. **Constraint Naming** - System auto-generates constraint names if not provided
4. **Base Type Validity** - Base type must be a valid ScratchBird DataType
5. **Recursive Prohibition** - Domain cannot reference itself (directly or indirectly)

### 3.4 SBLR Encoding

See `docs/specifications/SBLR_DOMAIN_PAYLOADS.md` for the current
EXT_CREATE_DOMAIN payload encoding (BASIC domains + WITH blocks).

---

## 4. RECORD DOMAINS (Composite Types)

### 4.1 Grammar (BNF)

```bnf
<create_record_domain> ::=
    CREATE DOMAIN [<schema_name>.]<domain_name>
    AS RECORD
    ( <field_definition_list> )
    [WITH DIALECT (<dialect_tag>)]
    [WITH COMPAT (<compat_name>)]

<field_definition_list> ::=
    <field_definition> [, <field_definition>]...

<field_definition> ::=
    <field_name> <field_type> [NOT NULL] [DEFAULT <default_value>]

<field_type> ::=
    <builtin_type>
  | [<schema_name>.]<domain_name>  -- Can use other domains as field types
```

### 4.2 Examples

```sql
-- Simple RECORD domain
CREATE DOMAIN address_type AS RECORD (
    street VARCHAR(100),
    city VARCHAR(50),
    state CHAR(2),
    zip VARCHAR(10)
);

-- RECORD with domain-typed fields
CREATE DOMAIN email_address AS VARCHAR(255)
    CHECK (VALUE LIKE '%@%');

CREATE DOMAIN contact_info AS RECORD (
    name VARCHAR(100) NOT NULL,
    email email_address,
    phone VARCHAR(20)
);

-- PostgreSQL-compatible composite type
CREATE DOMAIN pg_point AS RECORD (
    x DOUBLE PRECISION,
    y DOUBLE PRECISION
) WITH DIALECT ('postgresql')
  WITH COMPAT ('point');

-- Nested RECORD (RECORD fields can be other RECORDs)
CREATE DOMAIN full_address AS RECORD (
    street_address address_type,
    country VARCHAR(50)
);
```

### 4.3 Field Access Syntax (Firebird Style)

```sql
-- Field access using dot notation (Firebird/SQL standard)
SELECT customer.address.city FROM customers;

-- NOT PostgreSQL style (customer.address).city
-- We follow Firebird: customer.address.city
```

### 4.4 Semantic Rules

1. **Field Names** - Must be unique within RECORD
2. **Field Types** - Can be base types or other domains (including other RECORDs)
3. **Recursion Check** - Cannot create circular references
4. **NULL Fields** - Fields are NULL-able by default unless NOT NULL specified
5. **Field Ordering** - Order matters for construction and deconstruction

### 4.5 SBLR Encoding

See `docs/specifications/SBLR_DOMAIN_PAYLOADS.md` for the current payload
layout. RECORD domains will require payload extensions once implemented.

### 4.6 Storage Format

**In-Memory Representation:**
- Sequential field storage (field1, field2, ...)
- NULL bitmap prefix (1 bit per field)
- Field alignment per DataType requirements

**TOAST Storage:**
- RECORD values > 1KB stored in TOAST
- Entire RECORD stored as single TOAST chunk
- No partial TOAST (all or nothing)

---

## 5. ENUM DOMAINS (Ordered Enumeration)

### 5.1 Grammar (BNF)

```bnf
<create_enum_domain> ::=
    CREATE DOMAIN [<schema_name>.]<domain_name>
    AS ENUM
    ( <enum_value_list> )
    [WITH OPTIONS ( <enum_options> )]
    [WITH DIALECT (<dialect_tag>)]
    [WITH COMPAT (<compat_name>)]

<enum_value_list> ::=
    <enum_value> [, <enum_value>]...

<enum_value> ::=
    '<label>' [= <position>]

<enum_options> ::=
    WRAP = TRUE | FALSE  -- Allow wrapping to first value after last
```

### 5.2 Examples

```sql
-- Simple ENUM
CREATE DOMAIN status_enum AS ENUM (
    'PENDING',
    'ACTIVE',
    'SUSPENDED',
    'TERMINATED'
);

-- ENUM with explicit positions (must be sequential)
CREATE DOMAIN priority_level AS ENUM (
    'LOW' = 1,
    'MEDIUM' = 2,
    'HIGH' = 3,
    'CRITICAL' = 4
);

-- ENUM with WRAP option (SET NEXT VALUE wraps from last to first)
CREATE DOMAIN day_of_week AS ENUM (
    'MONDAY',
    'TUESDAY',
    'WEDNESDAY',
    'THURSDAY',
    'FRIDAY',
    'SATURDAY',
    'SUNDAY'
) WITH OPTIONS (WRAP = TRUE);

-- MySQL-compatible ENUM
CREATE DOMAIN mysql_user_status AS ENUM (
    'ACTIVE',
    'INACTIVE',
    'BANNED'
) WITH DIALECT ('mysql')
  WITH COMPAT ('ENUM');
```

### 5.3 ENUM Operations (Firebird Style)

```sql
-- Get next value (Firebird SET NEXT VALUE syntax)
SET NEXT VALUE FOR status_enum FROM 'PENDING' INTO :next_status;
-- Result: 'ACTIVE'

-- Get value for position (Firebird GET VALUE FOR)
GET VALUE FOR priority_level POSITION 3 INTO :value;
-- Result: 'HIGH'

-- Get position for value (Firebird GET POSITION FOR)
GET POSITION FOR priority_level VALUE 'CRITICAL' INTO :pos;
-- Result: 4

-- Comparison (based on position, not alphabetical)
WHERE priority_level > 'MEDIUM'  -- Returns HIGH and CRITICAL
```

### 5.4 Semantic Rules

1. **Position Assignment** - If not explicit, positions assigned sequentially from 1
2. **Position Gaps** - NOT allowed (must be 1, 2, 3, ... N)
3. **Label Uniqueness** - Labels must be unique within ENUM
4. **Case Sensitivity** - Labels are case-sensitive (Firebird style)
5. **Ordering** - Comparison uses position, not alphabetical order
6. **WRAP Behavior** - If WRAP=TRUE, SET NEXT VALUE from last returns first

### 5.5 SBLR Encoding

See `docs/specifications/SBLR_DOMAIN_PAYLOADS.md` for the current payload
layout. ENUM domains will require payload extensions once implemented.

### 5.6 Storage Format

**Internal Storage:** INT32 (position value)
**Display Format:** VARCHAR (label)

---

## 6. SET DOMAINS (Unordered Collection)

### 6.1 Grammar (BNF)

```bnf
<create_set_domain> ::=
    CREATE DOMAIN [<schema_name>.]<domain_name>
    AS SET OF <element_type>
    [WITH DIALECT (<dialect_tag>)]
    [WITH COMPAT (<compat_name>)]

<element_type> ::=
    <builtin_type>
  | [<schema_name>.]<domain_name>  -- Can be domain reference
```

### 6.2 Examples

```sql
-- Simple SET of integers
CREATE DOMAIN tag_ids AS SET OF INTEGER;

-- SET of domain-typed elements
CREATE DOMAIN email_address AS VARCHAR(255)
    CHECK (VALUE LIKE '%@%');

CREATE DOMAIN email_list AS SET OF email_address;

-- PostgreSQL-compatible array type (using SET as approximation)
CREATE DOMAIN pg_int_array AS SET OF INTEGER
    WITH DIALECT ('postgresql')
    WITH COMPAT ('integer[]');
```

### 6.3 SET Operations (Firebird/SQL Standard Operators)

```sql
-- Contains operator (@>) - Firebird style
WHERE tags @> 'important'  -- Set contains element

-- Overlap operator (&&) - PostgreSQL/Firebird compatible
WHERE tags1 && tags2  -- Sets have common elements

-- Union
SELECT set_union(tags1, tags2) FROM ...

-- Intersection
SELECT set_intersection(tags1, tags2) FROM ...

-- Difference
SELECT set_difference(tags1, tags2) FROM ...

-- Element count
SELECT CARDINALITY(tags) FROM ...  -- SQL standard
```

### 6.4 Semantic Rules

1. **Uniqueness** - SET enforces uniqueness automatically (duplicates removed on insert)
2. **Unordered** - No guaranteed order (unlike arrays)
3. **Element Type** - All elements must be same type
4. **NULL Elements** - NULL elements NOT allowed in SET
5. **Comparison** - SET equality uses unordered comparison

### 6.5 SBLR Encoding

See `docs/specifications/SBLR_DOMAIN_PAYLOADS.md` for the current payload
layout. SET domains will require payload extensions once implemented.

### 6.6 Storage Format

**Internal Storage:** Array of sorted elements (for fast lookup)
**Encoding:** [count:uint32] [element1] [element2] ... [elementN]

---

## 7. VARIANT DOMAINS (Polymorphic Type)

### 7.1 Grammar (BNF)

```bnf
<create_variant_domain> ::=
    CREATE DOMAIN [<schema_name>.]<domain_name>
    AS VARIANT
    ( <allowed_type_list> )
    [WITH DIALECT (<dialect_tag>)]
    [WITH COMPAT (<compat_name>)]

<allowed_type_list> ::=
    <variant_type> [, <variant_type>]...

<variant_type> ::=
    <builtin_type>
  | [<schema_name>.]<domain_name>  -- Can be domain reference
```

### 7.2 Examples

```sql
-- Numeric variant (INTEGER or DECIMAL)
CREATE DOMAIN numeric_variant AS VARIANT (
    INTEGER,
    DECIMAL(15,4)
);

-- JSON-like variant (PostgreSQL compatibility)
CREATE DOMAIN json_value AS VARIANT (
    VARCHAR,
    INTEGER,
    DOUBLE PRECISION,
    BOOLEAN
) WITH DIALECT ('postgresql')
  WITH COMPAT ('jsonb');

-- Domain-typed variant
CREATE DOMAIN email_address AS VARCHAR(255);
CREATE DOMAIN phone_number AS VARCHAR(20);

CREATE DOMAIN contact_method AS VARIANT (
    email_address,
    phone_number
);
```

### 7.3 VARIANT Operations (Firebird EXTRACT + IS OF TYPE)

```sql
-- Extract datatype from variant (Firebird EXTRACT syntax)
EXTRACT(DATATYPE FROM contact_value) -- Returns DataType enum

-- Type check (SQL standard IS OF TYPE)
WHERE contact_value IS OF TYPE (email_address)

-- Safe cast with type check
SELECT
    CASE
        WHEN json_val IS OF TYPE (INTEGER) THEN CAST(json_val AS INTEGER)
        ELSE 0
    END
FROM ...
```

### 7.4 Semantic Rules

1. **Runtime Type** - Actual type determined at runtime, not compile time
2. **Type Safety** - Executor enforces type is in allowed list
3. **NULL Handling** - VARIANT can be NULL (separate from contained value being NULL)
4. **Type Coercion** - No automatic coercion between allowed types
5. **Storage Overhead** - Extra 2 bytes to store actual type discriminant

### 7.5 SBLR Encoding

See `docs/specifications/SBLR_DOMAIN_PAYLOADS.md` for the current payload
layout. VARIANT domains will require payload extensions once implemented.

### 7.6 Storage Format

**Structure:**
```
[discriminant:uint16] (actual DataType)
[value_data:variable] (actual value based on discriminant)
```

---

## 8. DOMAIN INHERITANCE (INHERITS Clause)

### 8.1 Grammar (BNF)

```bnf
<domain_with_inheritance> ::=
    CREATE DOMAIN [<schema_name>.]<domain_name>
    [AS] <domain_definition>
    [INHERITS ([<schema_name>.]<parent_domain>)]
    [<additional_constraints>]

<additional_constraints> ::=
    [CHECK (<additional_check_expression>)]
    [NOT NULL]  -- Can add NOT NULL even if parent allows NULL
```

### 8.2 Examples

```sql
-- Base domain
CREATE DOMAIN email_address AS VARCHAR(255)
    CHECK (VALUE LIKE '%@%');

-- Derived domain with additional constraint
CREATE DOMAIN corporate_email AS VARCHAR(255)
    INHERITS (email_address)
    CHECK (VALUE LIKE '%@company.com');
-- Now has TWO checks: basic email format AND corporate domain

-- NOT NULL strengthening
CREATE DOMAIN nullable_positive AS INTEGER
    CHECK (VALUE > 0);

CREATE DOMAIN required_positive AS INTEGER
    INHERITS (nullable_positive)
    NOT NULL;
-- Inherits CHECK constraint and adds NOT NULL
```

### 8.3 Inheritance Rules

1. **Constraint Accumulation** - Child inherits ALL parent constraints PLUS adds own
2. **Strengthening Only** - Cannot weaken parent constraints (e.g., remove NOT NULL)
3. **Type Compatibility** - Child base type must match parent base type exactly
4. **Single Inheritance** - Only ONE parent domain allowed (no multiple inheritance)
5. **Cycle Detection** - Circular inheritance chains prohibited
6. **Depth Limit** - Maximum inheritance depth: 10 levels (configurable)

### 8.4 SBLR Encoding

See `docs/specifications/SBLR_DOMAIN_PAYLOADS.md` for the current payload
layout. INHERITS metadata will require payload extensions once implemented.

---

## 9. ADVANCED FEATURES (WITH Blocks)

### 9.1 WITH SECURITY

**Full Alpha Implementation Required**

```sql
CREATE DOMAIN ssn AS CHAR(11)
    CHECK (VALUE ~ '^\d{3}-\d{2}-\d{4}$')
    WITH SECURITY (
        MASKING = PARTIAL,           -- Show only last 4 digits
        MASK_PATTERN = 'XXX-XX-####',
        ENCRYPTION = AES256,         -- Encrypt at rest
        AUDIT_ACCESS = TRUE,         -- Log all access
        REQUIRE_PRIVILEGE = 'VIEW_SSN'
    );
```

**Implementation Requirements:**
- Parse and store all SECURITY options
- Implement masking logic (PARTIAL, FULL, NONE)
- Implement encryption/decryption at storage layer (AES256, AES128)
- Implement access auditing with transaction logging
- Implement privilege checking for REQUIRE_PRIVILEGE
- Enforce all security options during INSERT/UPDATE/SELECT operations

### 9.2 WITH INTEGRITY

**Full Alpha Implementation Required**

```sql
CREATE DOMAIN username AS VARCHAR(50)
    WITH INTEGRITY (
        UNIQUENESS = TRUE,           -- Enforce global uniqueness
        NORMALIZATION = LOWERCASE,   -- Auto-convert to lowercase
        NORMALIZATION_FUNCTION = 'TRIM(LOWER(VALUE))'
    );
```

**Implementation Requirements:**
- Parse and store all INTEGRITY options
- Implement global uniqueness checking across all columns using this domain
- Implement auto-normalization (LOWERCASE, UPPERCASE, TRIM)
- Support custom normalization functions
- Enforce integrity constraints during INSERT/UPDATE operations

### 9.3 WITH VALIDATION

**Full Alpha Implementation Required**

```sql
CREATE DOMAIN credit_card AS VARCHAR(20)
    WITH VALIDATION (
        FUNCTION = 'validate_luhn_checksum',
        ERROR_MESSAGE = 'Invalid credit card number (failed Luhn check)'
    );
```

**Implementation Requirements:**
- Parse and store all VALIDATION options
- Support custom validation function calls
- Return custom error messages on validation failure
- Enforce validation during INSERT/UPDATE operations
- Validation functions must be callable from domain constraints

### 9.4 WITH QUALITY

**Full Alpha Implementation Required**

```sql
CREATE DOMAIN phone_number AS VARCHAR(20)
    WITH QUALITY (
        PARSE_FUNCTION = 'parse_phone_e164',
        STANDARDIZE_FUNCTION = 'format_phone_national',
        ENRICH_FUNCTION = 'lookup_phone_carrier'
    );
```

**Implementation Requirements:**
- Parse and store all QUALITY options
- Implement parsing function calls to validate/extract components
- Implement standardization function calls for auto-formatting
- Implement enrichment function calls for data augmentation
- Execute quality functions during INSERT/UPDATE operations
- Support function chaining (parse → standardize → enrich)

---

## 10. ALTER DOMAIN

### 10.1 Grammar (BNF)

```bnf
<alter_domain> ::=
    ALTER DOMAIN [<schema_name>.]<domain_name>
    <alter_domain_action>

<alter_domain_action> ::=
    SET DEFAULT <default_value>
  | DROP DEFAULT
  | ADD CHECK (<check_expression>)
  | DROP CONSTRAINT <constraint_name>
  | RENAME TO <new_domain_name>
  | SET COMPAT <compat_name>
  | DROP COMPAT
```

### 10.2 Examples

```sql
-- Change default
ALTER DOMAIN status_flag SET DEFAULT 'Y';

-- Remove default
ALTER DOMAIN status_flag DROP DEFAULT;

-- Add constraint
ALTER DOMAIN percentage ADD CHECK (VALUE >= 0 AND VALUE <= 100);

-- Rename domain
ALTER DOMAIN old_email RENAME TO email_address;

-- Set compatibility name
ALTER DOMAIN mysql_bool SET COMPAT 'TINYINT';

-- Remove compatibility name
ALTER DOMAIN mysql_bool DROP COMPAT;
```

### 10.3 Restrictions

1. **Type Change** - CANNOT change base type (must drop and recreate)
2. **RECORD Fields** - CANNOT alter fields of RECORD domain (must drop and recreate)
3. **ENUM Values** - CANNOT alter ENUM values (must drop and recreate)
4. **Active Usage** - CANNOT alter if columns currently use domain (dependency check)
5. **Inheritance** - CANNOT change INHERITS parent (must drop and recreate)

### 10.4 SBLR Encoding

See `docs/specifications/SBLR_DOMAIN_PAYLOADS.md` for the current
EXT_ALTER_DOMAIN payload encoding.

---

## 11. DROP DOMAIN

### 11.1 Grammar (BNF)

```bnf
<drop_domain> ::=
    DROP DOMAIN [IF EXISTS] [<schema_name>.]<domain_name> RESTRICT

-- Note: CASCADE not supported in Alpha (safety measure)
```

### 11.2 Examples

```sql
-- Drop domain (fails if in use)
DROP DOMAIN email_address RESTRICT;

-- Drop if exists
DROP DOMAIN IF EXISTS old_domain RESTRICT;
```

### 11.3 Restrictions

1. **RESTRICT Only** - CASCADE not supported (cannot drop if columns use domain)
2. **Dependency Check** - Must check `findColumnsByDomain()` before drop
3. **Child Domains** - Must drop child domains (INHERITS) before parent

### 11.4 SBLR Encoding

See `docs/specifications/SBLR_DOMAIN_PAYLOADS.md` for the current
EXT_DROP_DOMAIN payload encoding.

---

## 12. DOMAIN USAGE IN TABLES

### 12.1 Column Definition Syntax

```sql
-- Using domain as column type
CREATE TABLE customers (
    customer_id INTEGER PRIMARY KEY,
    email email_address NOT NULL,
    status status_enum DEFAULT 'PENDING',
    tags tag_ids,
    address address_type  -- RECORD domain
);

-- Array of domain (Firebird style)
CREATE TABLE users (
    user_id INTEGER PRIMARY KEY,
    email_list email_address[10]  -- Array of email_address domain
);
```

### 12.2 SBLR Encoding for Domain-Typed Columns

```
[COLUMN_DEF]
[column_name:string]
[TYPE_DOMAIN]
[domain_id:UUID]
[is_array:uint8]
[if is_array: array_size:uint32]
[column_flags:uint8] (NOT NULL, etc.)
```

---

## 13. COMPATIBILITY MAPPINGS

### 13.1 Firebird Domains

```sql
-- Firebird uses same syntax as ScratchBird
CREATE DOMAIN fb_domain AS INTEGER CHECK (VALUE > 0);

-- ScratchBird parser supports Firebird domains natively
```

**Firebird Compatibility:** 100% - ScratchBird IS Firebird extension

### 13.2 PostgreSQL Domains

```sql
-- PostgreSQL basic domain
CREATE DOMAIN pg_email AS VARCHAR(255)
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');

-- PostgreSQL composite type → RECORD domain
CREATE TYPE pg_address AS (
    street VARCHAR(100),
    city VARCHAR(50),
    state CHAR(2)
);
-- Maps to:
CREATE DOMAIN pg_address AS RECORD (
    street VARCHAR(100),
    city VARCHAR(50),
    state CHAR(2)
) WITH DIALECT ('postgresql')
  WITH COMPAT ('address');
```

**PostgreSQL Compatibility:** Parser converts TYPE to DOMAIN

### 13.3 MySQL Compatibility

```sql
-- MySQL has NO domain support
-- Parser MUST reject CREATE DOMAIN in MySQL dialect

-- However, we can create ScratchBird domains for MySQL type conversion:
CREATE DOMAIN mysql_tinyint AS SMALLINT
    CHECK (VALUE BETWEEN -128 AND 127)
    WITH DIALECT ('mysql')
    WITH COMPAT ('TINYINT');

-- Used internally for transparent conversion
```

**MySQL Compatibility:** Reject CREATE DOMAIN; use internal domains for type conversion

---

## 14. IMPLEMENTATION NOTES

### 14.1 Catalog Storage

**Domain metadata stored in:**
- `sys.catalog.domains` - Main domain table (managed by DomainManager)
- TOAST storage for:
  - CHECK expressions (serialized SBLR)
  - DEFAULT expressions (serialized SBLR)
  - RECORD field definitions (serialized)
  - ENUM value lists (serialized)
  - WITH block options (JSON or serialized)

### 14.2 Constraint Enforcement

**Check Timing:**
1. **INSERT/UPDATE** - Domain constraints checked BEFORE table constraints
2. **Visibility** - Uses MGA visibility (xmin/xmax) for constraint versions
3. **Inheritance** - All ancestor constraints checked in order (parent first)

**Execution Order:**
```
1. NOT NULL check
2. Parent domain constraints (if INHERITS)
3. Domain CHECK constraints
4. Table CHECK constraints
5. Foreign key constraints
```

### 14.3 Performance Considerations

1. **Constraint Caching** - Compiled CHECK expressions cached in DomainManager
2. **Inheritance Chain** - Cached to avoid repeated catalog lookups
3. **RECORD Alignment** - Fields aligned per DataType for fast access
4. **ENUM Lookup** - Hash table for label→position mapping

---

## 15. ERROR HANDLING

### 15.1 Error Codes

```
DOMAIN_NOT_FOUND          - Domain does not exist
DOMAIN_ALREADY_EXISTS     - Domain name conflict
DOMAIN_IN_USE             - Cannot drop, columns reference it
DOMAIN_CONSTRAINT_FAILED  - Value violates domain constraint
DOMAIN_TYPE_MISMATCH      - Inheritance type mismatch
DOMAIN_CIRCULAR_REF       - Circular inheritance detected
DOMAIN_DEPTH_EXCEEDED     - Inheritance depth > 10
INVALID_ENUM_POSITION     - ENUM position out of range or gap
SET_ELEMENT_TYPE_MISMATCH - SET element wrong type
VARIANT_TYPE_NOT_ALLOWED  - VARIANT value type not in allowed list
```

### 15.2 Error Messages (Firebird Style)

```
"Domain EMAIL_ADDRESS not found in schema PUBLIC"
"Cannot drop domain EMAIL_ADDRESS: used by 3 columns"
"Value 'invalid@' violates domain EMAIL_ADDRESS constraint"
"Domain CORPORATE_EMAIL inherits from EMAIL_ADDRESS with different base type"
```

---

## APPENDIX A: COMPLETE BNF GRAMMAR

```bnf
<domain_statement> ::=
    <create_domain>
  | <alter_domain>
  | <drop_domain>

<create_domain> ::=
    CREATE DOMAIN [<schema_name>.]<domain_name>
    <domain_definition>

<domain_definition> ::=
    AS <domain_type_spec>
    [INHERITS ([<schema_name>.]<parent_domain>)]
    [<domain_options>]
    [<with_blocks>]

<domain_type_spec> ::=
    <base_type>                              -- BASIC domain
  | RECORD ( <field_definition_list> )       -- RECORD domain
  | ENUM ( <enum_value_list> )               -- ENUM domain
  | SET OF <element_type>                    -- SET domain
  | VARIANT ( <allowed_type_list> )          -- VARIANT domain

<domain_options> ::=
    [DEFAULT <default_value>]
    [NOT NULL | NULL]
    [CHECK (<check_expression>)]
    [COLLATE <collation_name>]
    [WITH DIALECT (<dialect_tag>)]
    [WITH COMPAT (<compat_name>)]

<with_blocks> ::=
    [WITH SECURITY ( <security_options> )]
    [WITH INTEGRITY ( <integrity_options> )]
    [WITH VALIDATION ( <validation_options> )]
    [WITH QUALITY ( <quality_options> )]
    [WITH OPTIONS ( <enum_options> )]

<alter_domain> ::=
    ALTER DOMAIN [<schema_name>.]<domain_name>
    <alter_action>

<alter_action> ::=
    SET DEFAULT <default_value>
  | DROP DEFAULT
  | ADD CHECK (<check_expression>)
  | DROP CONSTRAINT <constraint_name>
  | RENAME TO <new_name>
  | SET COMPAT <compat_name>
  | DROP COMPAT

<drop_domain> ::=
    DROP DOMAIN [IF EXISTS] [<schema_name>.]<domain_name> RESTRICT
```

---

## APPENDIX B: SBLR OPCODE SUMMARY

```
EXT_CREATE_DOMAIN    = 0x5C   -- Create domain (all types)
EXT_ALTER_DOMAIN     = 0x010E -- Alter domain
EXT_DROP_DOMAIN      = 0x010F -- Drop domain
EXT_SHOW_DOMAIN      = 0x64   -- Show domain definition
```

---

**END OF SPECIFICATION**
