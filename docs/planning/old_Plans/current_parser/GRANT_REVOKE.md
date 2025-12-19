# GRANT and REVOKE Commands - Complete Syntax Reference

**Document Version:** 1.0
**Date:** 2025-12-06
**Parser Implementation:** `/src/parser/parser.cpp`
**AST Definitions:** `/include/scratchbird/parser/ast.h`

## Table of Contents

1. [Overview](#overview)
2. [GRANT Command](#grant-command)
3. [REVOKE Command](#revoke-command)
4. [Privilege Types](#privilege-types)
5. [Object Types](#object-types)
6. [Grantee Types](#grantee-types)
7. [Options and Modifiers](#options-and-modifiers)
8. [BNF Syntax](#bnf-syntax)
9. [Implementation Details](#implementation-details)
10. [Examples](#examples)
11. [Limitations and Notes](#limitations-and-notes)

---

## Overview

ScratchBird implements comprehensive SQL security commands for granting and revoking privileges on database objects. The parser supports two distinct forms of GRANT/REVOKE:

1. **Privilege Grant/Revoke** - Grants/revokes specific privileges on database objects
2. **Role Grant/Revoke** - Grants/revokes role membership to users/roles

**Parser Functions:**
- `Parser::parseGrant()` - Line 8677 in parser.cpp
- `Parser::parseRevoke()` - Line 8968 in parser.cpp

**AST Classes:**
- `GrantPrivilegeStmt` - Line 4156 in ast.h
- `RevokePrivilegeStmt` - Line 4238 in ast.h
- `GrantRoleStmt` - Line 4294 in ast.h
- `RevokeRoleStmt` - Line 4331 in ast.h

---

## GRANT Command

### GRANT Privilege Syntax

```sql
GRANT privilege_list ON object_type object_name TO grantee [WITH GRANT OPTION]
```

### GRANT Role Syntax

```sql
GRANT rolename TO grantee [WITH ADMIN OPTION]
```

### Description

The GRANT command assigns privileges on database objects or grants role membership to users/roles.

**Key Features:**
- Multiple privileges can be granted in a single command
- Column-level permissions are supported (Security Phase 3.3.3)
- WITH GRANT OPTION allows grantee to grant the privilege to others
- WITH ADMIN OPTION (for roles) allows grantee to grant the role to others

---

## REVOKE Command

### REVOKE Privilege Syntax

```sql
REVOKE privilege_list ON object_type object_name FROM grantee [CASCADE | RESTRICT]
```

### REVOKE Role Syntax

```sql
REVOKE rolename FROM grantee [CASCADE | RESTRICT]
```

### Description

The REVOKE command removes previously granted privileges or role memberships.

**Key Features:**
- Mirrors GRANT syntax with FROM instead of TO
- CASCADE removes dependent privileges/objects
- RESTRICT (default) prevents revocation if dependencies exist
- Column-level permissions are supported (Security Phase 3.3.3)

---

## Privilege Types

The following privilege types are supported (defined as bitmask flags):

| Privilege    | Hex Value  | Decimal | Description                                    |
|--------------|------------|---------|------------------------------------------------|
| `SELECT`     | 0x00000001 | 1       | Read data from tables/views                    |
| `INSERT`     | 0x00000002 | 2       | Insert new rows into tables                    |
| `UPDATE`     | 0x00000004 | 4       | Modify existing rows in tables                 |
| `DELETE`     | 0x00000008 | 8       | Delete rows from tables                        |
| `TRUNCATE`   | 0x00000010 | 16      | Truncate tables (remove all rows)              |
| `REFERENCES` | 0x00000020 | 32      | Create foreign key constraints referencing table|
| `TRIGGER`    | 0x00000040 | 64      | Create triggers on tables                      |
| `CREATE`     | 0x00000080 | 128     | Create objects in database/schema              |
| `USAGE`      | 0x00000100 | 256     | Use sequences, schemas, or other objects       |
| `EXECUTE`    | 0x00000800 | 2048    | Execute functions or procedures                |
| `CONNECT`    | 0x00001000 | 4096    | Connect to database                            |
| `ALL`        | 0xFFFFFFFF | 4294967295 | All available privileges                    |

### Privilege Applicability by Object Type

| Privilege    | TABLE | VIEW | SEQUENCE | FUNCTION | PROCEDURE | DATABASE | SCHEMA | DOMAIN |
|--------------|-------|------|----------|----------|-----------|----------|--------|--------|
| SELECT       | Yes   | Yes  | No       | No       | No        | No       | No     | No     |
| INSERT       | Yes   | Yes* | No       | No       | No        | No       | No     | No     |
| UPDATE       | Yes   | Yes* | No       | No       | No        | No       | No     | No     |
| DELETE       | Yes   | Yes* | No       | No       | No        | No       | No     | No     |
| TRUNCATE     | Yes   | No   | No       | No       | No        | No       | No     | No     |
| REFERENCES   | Yes   | No   | No       | No       | No        | No       | No     | No     |
| TRIGGER      | Yes   | No   | No       | No       | No        | No       | No     | No     |
| CREATE       | No    | No   | No       | No       | No        | Yes      | Yes    | No     |
| USAGE        | No    | No   | Yes      | No       | No        | No       | Yes    | Yes    |
| EXECUTE      | No    | No   | No       | Yes      | Yes       | No       | No     | No     |
| CONNECT      | No    | No   | No       | No       | No        | Yes      | No     | No     |
| ALL          | Yes   | Yes  | Yes      | Yes      | Yes       | Yes      | Yes    | Yes    |

*For updatable views only

### Column-Level Privileges

**Supported Since:** Security Phase 3.3.3
**Applicable To:** SELECT, INSERT, UPDATE, REFERENCES privileges on TABLE objects

**Syntax:**
```sql
GRANT privilege (column1, column2, ...) ON TABLE table_name TO grantee
```

**Parser Implementation:**
- Lines 8807-8830 (GRANT) and 9082-9105 (REVOKE) in parser.cpp
- Column names stored in `std::vector<StringPool::StringId> column_names_`
- Accessible via `columnNames()` and `hasColumnList()` methods

---

## Object Types

The following database object types are supported:

| Object Type | Description                              | Implemented in Parser |
|-------------|------------------------------------------|-----------------------|
| `TABLE`     | Database tables                          | Yes (line 8842)       |
| `VIEW`      | Database views                           | Yes (line 8846)       |
| `SEQUENCE`  | Sequence generators                      | Yes (line 8850)       |
| `FUNCTION`  | User-defined functions                   | Yes (line 8854)       |
| `PROCEDURE` | Stored procedures                        | Yes (line 8858)       |
| `DATABASE`  | Database-level privileges                | Yes (line 8862)       |
| `SCHEMA`    | Schema-level privileges                  | No (AST only)         |
| `DOMAIN`    | Domain type privileges                   | No (AST only)         |

**Note:** SCHEMA and DOMAIN are defined in the AST (`ObjectType` enum at line 4175-4185) but are not currently implemented in the parser. These are reserved for future implementation.

---

## Grantee Types

Privileges can be granted to different types of security principals:

### For Privilege Grants

| Grantee Type | Syntax                       | Description                    | Line in Parser |
|--------------|------------------------------|--------------------------------|----------------|
| `USER`       | `TO USER username`           | Specific database user         | 8900-8909      |
| `ROLE`       | `TO ROLE rolename`           | Database role                  | 8911-8921      |
| `GROUP`      | `TO GROUP groupname`         | User group                     | 8923-8933      |
| `PUBLIC`     | `TO PUBLIC`                  | All users (no name required)   | 8894-8897      |
| *(implicit)* | `TO username`                | Defaults to USER type          | 8935-8940      |

### For Role Grants

| Grantee Type | Syntax                       | Description                    | Line in Parser |
|--------------|------------------------------|--------------------------------|----------------|
| `USER`       | `TO username`                | Grant role to user             | 8734           |
| `ROLE`       | `TO rolename`                | Grant role to another role     | AST only       |

**Note:** When grantee type is omitted in privilege grants, the parser defaults to USER type (line 8937-8940).

---

## Options and Modifiers

### WITH GRANT OPTION

**Applies to:** Privilege grants
**Syntax:** `WITH GRANT OPTION`
**Parser Location:** Lines 8949-8960
**AST Field:** `bool with_grant_option_`

**Description:**
Allows the grantee to grant the same privilege to other users. The grantee becomes a grantor for this privilege.

**Current Implementation Status:**
- Partially implemented (line 8958 comment: "OPTION keyword is not in our token list, skip it for now")
- Parser consumes `WITH GRANT` but does not verify `OPTION` keyword
- AST field `with_grant_option_` is always set to `false` in current implementation

**Example:**
```sql
GRANT SELECT ON TABLE employees TO USER alice WITH GRANT OPTION;
```

### WITH ADMIN OPTION

**Applies to:** Role grants
**Syntax:** `WITH ADMIN OPTION`
**Parser Location:** Lines 8710-8730
**AST Field:** `bool with_admin_option_`
**Implementation:** WP-6 PARSE-L1

**Description:**
Allows the grantee to grant the role to other users/roles. Similar to WITH GRANT OPTION but for roles.

**Parser Implementation:**
- Fully implemented for GRANT ROLE
- Uses lookahead to check for ADMIN token (line 8714-8715)
- Requires all three keywords: WITH ADMIN OPTION

**Example:**
```sql
GRANT admin_role TO USER bob WITH ADMIN OPTION;
```

### CASCADE

**Applies to:** REVOKE statements
**Syntax:** `CASCADE`
**Parser Location:** Lines 8997-8999 (role), 9225-9227 (privilege)
**AST Enum:** `RevokeBehavior::CASCADE`

**Description:**
Automatically revokes dependent privileges or role memberships. If the revoked privilege was granted with GRANT OPTION, all privileges granted by the grantee are also revoked.

**Example:**
```sql
REVOKE SELECT ON TABLE employees FROM USER alice CASCADE;
REVOKE manager_role FROM USER bob CASCADE;
```

### RESTRICT

**Applies to:** REVOKE statements
**Syntax:** `RESTRICT`
**Parser Location:** Lines 9001-9003 (role), 9229-9231 (privilege)
**AST Enum:** `RevokeBehavior::RESTRICT` (default)

**Description:**
Prevents revocation if dependent privileges exist. This is the default behavior if neither CASCADE nor RESTRICT is specified.

**Example:**
```sql
REVOKE SELECT ON TABLE employees FROM USER alice RESTRICT;
REVOKE manager_role FROM USER bob RESTRICT;
```

### REVOKE GRANT OPTION FOR

**Status:** NOT IMPLEMENTED

This SQL standard syntax allows revoking only the grant option without revoking the privilege itself:

```sql
REVOKE GRANT OPTION FOR privilege ON object FROM grantee
```

This feature is not currently supported in the ScratchBird parser.

---

## BNF Syntax

### Complete GRANT Syntax

```bnf
<grant statement> ::=
    <grant privilege statement>
  | <grant role statement>

<grant privilege statement> ::=
    GRANT <privilege list>
    ON <object type> <object name>
    TO <grantee>
    [ WITH GRANT OPTION ]

<grant role statement> ::=
    GRANT <role name>
    TO <grantee>
    [ WITH ADMIN OPTION ]

<privilege list> ::=
    <privilege> [ ( <column list> ) ] [ { , <privilege> [ ( <column list> ) ] }... ]
  | ALL [ PRIVILEGES ]

<privilege> ::=
    SELECT
  | INSERT
  | UPDATE
  | DELETE
  | TRUNCATE
  | REFERENCES
  | TRIGGER
  | CREATE
  | USAGE
  | EXECUTE
  | CONNECT

<column list> ::=
    <column name> [ { , <column name> }... ]

<object type> ::=
    TABLE
  | VIEW
  | SEQUENCE
  | FUNCTION
  | PROCEDURE
  | DATABASE
  | SCHEMA      -- AST only, not parsed
  | DOMAIN      -- AST only, not parsed

<grantee> ::=
    [ USER ] <user name>
  | ROLE <role name>
  | GROUP <group name>
  | PUBLIC

<object name> ::= <identifier>

<role name> ::= <identifier>

<user name> ::= <identifier>

<group name> ::= <identifier>

<column name> ::= <identifier>
```

### Complete REVOKE Syntax

```bnf
<revoke statement> ::=
    <revoke privilege statement>
  | <revoke role statement>

<revoke privilege statement> ::=
    REVOKE <privilege list>
    ON <object type> <object name>
    FROM <grantee>
    [ CASCADE | RESTRICT ]

<revoke role statement> ::=
    REVOKE <role name>
    FROM <grantee>
    [ CASCADE | RESTRICT ]

-- <privilege list>, <object type>, <grantee> defined same as GRANT
```

---

## Implementation Details

### Parser Flow - GRANT

1. **Initial Token Check** (lines 8688-8742)
   - If first token is IDENTIFIER followed by TO → Parse as GRANT ROLE
   - Otherwise → Parse as GRANT PRIVILEGE

2. **Privilege Parsing** (lines 8748-8831)
   - Loop through privilege keywords (SELECT, INSERT, UPDATE, etc.)
   - For each privilege, check for optional column list in parentheses
   - Combine multiple privileges with comma separator
   - ALL privilege short-circuits the loop

3. **Object Specification** (lines 8833-8881)
   - Expect ON keyword
   - Parse object type (TABLE, VIEW, etc.)
   - Parse object name identifier

4. **Grantee Specification** (lines 8883-8947)
   - Expect TO keyword
   - Parse grantee type (USER, ROLE, GROUP, PUBLIC, or implicit USER)
   - Parse grantee name (except for PUBLIC)

5. **Options** (lines 8949-8960)
   - Check for WITH GRANT OPTION
   - Create GrantPrivilegeStmt AST node

### Parser Flow - REVOKE

Follows identical structure to GRANT with these differences:

1. **Initial Token Check** (lines 8975-9016)
   - If first token is IDENTIFIER followed by FROM → Parse as REVOKE ROLE
   - Otherwise → Parse as REVOKE PRIVILEGE

2. **Grantee Keyword** (lines 9158-9221)
   - Uses FROM instead of TO

3. **Drop Behavior** (lines 9223-9232)
   - Parses optional CASCADE or RESTRICT
   - Defaults to RESTRICT if neither specified

### AST Storage

**GrantPrivilegeStmt:**
```cpp
uint32_t privileges_;                                // Bitmask of privileges
ObjectType object_type_;                             // Type of object
StringPool::StringId object_name_;                   // Object identifier
GranteeType grantee_type_;                           // USER/ROLE/GROUP/PUBLIC
StringPool::StringId grantee_name_;                  // Grantee identifier (0 for PUBLIC)
bool with_grant_option_;                             // Grant option flag
std::vector<StringPool::StringId> column_names_;     // Column-level permissions
```

**RevokePrivilegeStmt:**
```cpp
uint32_t privileges_;                                // Bitmask of privileges
ObjectType object_type_;                             // Type of object
StringPool::StringId object_name_;                   // Object identifier
GranteeType grantee_type_;                           // USER/ROLE/GROUP/PUBLIC
StringPool::StringId grantee_name_;                  // Grantee identifier
RevokeBehavior revoke_behavior_;                     // CASCADE or RESTRICT
std::vector<StringPool::StringId> column_names_;     // Column-level permissions
```

**GrantRoleStmt:**
```cpp
StringPool::StringId rolename_;                      // Role to grant
GranteeType grantee_type_;                           // USER or ROLE
StringPool::StringId grantee_name_;                  // Grantee identifier
bool with_admin_option_;                             // Admin option flag
```

**RevokeRoleStmt:**
```cpp
StringPool::StringId rolename_;                      // Role to revoke
GranteeType grantee_type_;                           // USER or ROLE
StringPool::StringId grantee_name_;                  // Grantee identifier
RevokeBehavior revoke_behavior_;                     // CASCADE or RESTRICT
```

### Privilege Bitmask Encoding

Privileges are stored as a 32-bit bitmask (`uint32_t`), allowing multiple privileges to be efficiently combined using bitwise OR:

```cpp
privileges |= static_cast<uint32_t>(GrantPrivilegeStmt::PrivilegeType::SELECT);
privileges |= static_cast<uint32_t>(GrantPrivilegeStmt::PrivilegeType::INSERT);
```

Special case: `ALL` is represented as `0xFFFFFFFF` (all bits set).

---

## Examples

### Basic Privilege Grants

```sql
-- Grant single privilege
GRANT SELECT ON TABLE employees TO USER alice;

-- Grant multiple privileges
GRANT SELECT, INSERT, UPDATE ON TABLE employees TO USER bob;

-- Grant to PUBLIC
GRANT SELECT ON TABLE public_data TO PUBLIC;

-- Grant to role
GRANT SELECT ON TABLE sensitive_data TO ROLE auditors;

-- Grant to group
GRANT INSERT ON TABLE logs TO GROUP developers;

-- Grant all privileges
GRANT ALL ON TABLE admin_table TO USER superuser;
```

### Column-Level Privileges (Security Phase 3.3.3)

```sql
-- Grant SELECT on specific columns
GRANT SELECT (id, name, email) ON TABLE users TO USER alice;

-- Grant UPDATE on salary column only
GRANT UPDATE (salary) ON TABLE employees TO ROLE hr_managers;

-- Multiple privileges with column lists
GRANT SELECT (id, name), UPDATE (email) ON TABLE users TO USER bob;
```

### Privilege Grants with Options

```sql
-- WITH GRANT OPTION
GRANT SELECT ON TABLE employees TO USER alice WITH GRANT OPTION;

-- Multiple privileges with grant option
GRANT SELECT, INSERT ON TABLE projects TO USER manager WITH GRANT OPTION;
```

### Role Grants

```sql
-- Grant role to user
GRANT developer TO alice;

-- Grant role with admin option
GRANT admin_role TO bob WITH ADMIN OPTION;

-- Grant role to another role (role hierarchy)
GRANT basic_role TO advanced_role;
```

### Object-Specific Grants

```sql
-- Sequence privileges
GRANT USAGE ON SEQUENCE order_id_seq TO USER order_processor;

-- Function privileges
GRANT EXECUTE ON FUNCTION calculate_discount TO USER cashier;

-- Procedure privileges
GRANT EXECUTE ON PROCEDURE monthly_cleanup TO ROLE admin;

-- Database privileges
GRANT CONNECT ON DATABASE company_db TO USER employee;
GRANT CREATE ON DATABASE company_db TO ROLE developer;

-- View privileges
GRANT SELECT ON VIEW employee_summary TO PUBLIC;
```

### Basic Revocations

```sql
-- Revoke single privilege
REVOKE SELECT ON TABLE employees FROM USER alice;

-- Revoke multiple privileges
REVOKE SELECT, INSERT, UPDATE ON TABLE employees FROM USER bob;

-- Revoke from PUBLIC
REVOKE SELECT ON TABLE public_data FROM PUBLIC;

-- Revoke all privileges
REVOKE ALL ON TABLE admin_table FROM USER former_admin;
```

### Column-Level Revocations

```sql
-- Revoke SELECT on specific columns
REVOKE SELECT (salary, bonus) ON TABLE employees FROM USER alice;

-- Revoke UPDATE on specific columns
REVOKE UPDATE (salary) ON TABLE employees FROM ROLE managers;
```

### Revocations with Behavior Modifiers

```sql
-- RESTRICT (prevent if dependencies exist)
REVOKE SELECT ON TABLE employees FROM USER alice RESTRICT;

-- CASCADE (remove dependent privileges)
REVOKE SELECT ON TABLE employees FROM USER alice CASCADE;

-- Default is RESTRICT if not specified
REVOKE INSERT ON TABLE logs FROM USER bob;
```

### Role Revocations

```sql
-- Revoke role from user
REVOKE developer FROM alice;

-- Revoke with CASCADE (remove dependent role grants)
REVOKE admin_role FROM bob CASCADE;

-- Revoke with RESTRICT
REVOKE manager_role FROM charlie RESTRICT;
```

### Complex Scenarios

```sql
-- Grant privilege, then allow grantee to grant to others
GRANT SELECT ON TABLE departments TO USER alice WITH GRANT OPTION;
-- Alice can now: GRANT SELECT ON TABLE departments TO USER charlie;

-- Grant role with ability to grant role membership
GRANT team_lead TO USER alice WITH ADMIN OPTION;
-- Alice can now: GRANT team_lead TO USER bob;

-- Revoke cascading grants
-- If alice granted to charlie, this revokes both:
REVOKE SELECT ON TABLE departments FROM USER alice CASCADE;

-- Multiple object types in transaction
GRANT SELECT ON TABLE orders TO USER clerk;
GRANT USAGE ON SEQUENCE order_id_seq TO USER clerk;
GRANT EXECUTE ON FUNCTION validate_order TO USER clerk;
```

### Test Suite Examples

From `/tests/integration/test_security_phase2.cpp`:

```sql
-- Basic privilege grant (Test 8)
GRANT SELECT ON TABLE testtable TO USER testuser;
GRANT INSERT, UPDATE ON TABLE testtable TO USER testuser;
GRANT SELECT ON TABLE testtable TO PUBLIC;

-- Basic privilege revoke (Test 9)
REVOKE SELECT ON TABLE testtable2 FROM USER testuser2;

-- Role grant (Test 10)
GRANT worker TO emp;

-- Role revoke (Test 11)
REVOKE worker2 FROM emp2;

-- Privilege grant to role (Test 12)
GRANT SELECT, INSERT ON TABLE projects TO ROLE developers;

-- WITH GRANT OPTION (Test 13)
GRANT SELECT ON TABLE data TO USER user1 WITH GRANT OPTION;
```

From `/tests/integration/test_security_phase3_3.cpp`:

```sql
-- Column-level grants (Tests 6-7)
GRANT SELECT (salary) ON TABLE employees TO alice;
GRANT SELECT (id, name, email) ON TABLE users TO bob;

-- Column-level revoke (Test 8)
REVOKE SELECT (salary, bonus) ON TABLE employees FROM alice;
```

---

## Limitations and Notes

### Current Limitations

1. **WITH GRANT OPTION Token**
   - The OPTION keyword is not fully implemented in the lexer
   - Parser accepts `WITH GRANT` but does not verify `OPTION`
   - AST field `with_grant_option_` is always `false` in current implementation
   - See line 8958: "OPTION keyword is not in our token list, skip it for now"

2. **SCHEMA and DOMAIN Object Types**
   - Defined in AST (`ObjectType` enum) but not implemented in parser
   - Reserved for future implementation
   - Parser will reject `GRANT ... ON SCHEMA ...` syntax

3. **REVOKE GRANT OPTION FOR**
   - SQL standard syntax for revoking only the grant option (not the privilege)
   - Not implemented in current parser

4. **ALL PRIVILEGES Syntax**
   - Parser accepts `ALL` but does not recognize `ALL PRIVILEGES`
   - Token `KW_PRIVILEGES` exists (line 434 in token.h) but is not used in GRANT/REVOKE parsing

5. **Multiple Object Names**
   - Standard SQL allows: `GRANT SELECT ON TABLE t1, t2, t3 TO user`
   - ScratchBird requires separate GRANT statements for each object

6. **GRANTED BY Clause**
   - SQL standard `GRANTED BY grantor` clause is not supported
   - Grantor is implicitly the current user

### Parser Behavior Notes

1. **Default Grantee Type**
   - When grantee type keyword is omitted, defaults to USER
   - `GRANT SELECT ON TABLE t TO alice` is equivalent to `GRANT SELECT ON TABLE t TO USER alice`

2. **Default Revoke Behavior**
   - When neither CASCADE nor RESTRICT is specified, defaults to RESTRICT
   - Prevents accidental cascading revocations

3. **Role vs Privilege Grant Detection**
   - Parser uses lookahead to distinguish between role and privilege grants
   - If first token after GRANT is identifier followed by TO → role grant
   - Otherwise → privilege grant

4. **Privilege Bitmask**
   - Multiple privileges are combined using bitwise OR
   - `ALL` is represented as `0xFFFFFFFF` (all bits set)
   - Efficient storage and testing of multiple privileges

5. **Column-Level Permissions**
   - Implemented in Security Phase 3.3.3
   - Column list applies to the immediately preceding privilege
   - Multiple privileges can have different column lists in same statement

### Security Model Integration

These parser commands are part of ScratchBird's comprehensive security implementation:

- **Security Phase 2:** Basic GRANT/REVOKE for privileges and roles
- **Security Phase 3.3.3:** Column-level permissions
- **WP-6 PARSE-L1:** WITH ADMIN OPTION for role grants

Related security features:
- `SET ROLE` / `RESET ROLE` (lines 9240-9268)
- `SET SESSION AUTHORIZATION` / `RESET SESSION AUTHORIZATION` (lines 9270+)
- User/Role/Group management (CREATE/ALTER/DROP statements)

### Future Enhancements

Potential areas for enhancement based on SQL standards:

1. Implement full `WITH GRANT OPTION` token support
2. Add `SCHEMA` and `DOMAIN` object type parsing
3. Support `REVOKE GRANT OPTION FOR` syntax
4. Allow `ALL PRIVILEGES` variant
5. Support multiple object names in single statement
6. Add `GRANTED BY` clause
7. Implement privilege inheritance through role hierarchies

---

## Revision History

| Version | Date       | Description                           |
|---------|------------|---------------------------------------|
| 1.0     | 2025-12-06 | Initial comprehensive documentation   |

---

**End of Document**
