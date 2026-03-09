<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# Domain-Level Security

[Prev](./03_column_security_and_masking_patterns.md) | [Topic README](./README.md) | [Security Hardening README](../../README.md)

## Coverage and Evidence Status

Status: Complete

## Synopsis

Domain-Level Security applies security policies at the data type level. When a column uses a secured domain, all access to that data is automatically protected according to domain policies.

## What is a Domain?

A domain is a named data type with:
- Base type (INTEGER, TEXT, etc.)
- Constraints (CHECK, NOT NULL)
- Default values
- **Security policies (SB extension)**

```sql
-- Basic domain
CREATE DOMAIN email_type AS TEXT
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');

-- Secured domain with masking
CREATE DOMAIN ssn_type AS TEXT
    CHECK (VALUE ~ '^\d{3}-\d{2}-\d{4}$')
    MASKED WITH (FUNCTION = 'partial(VALUE, 7)');
```

## Creating Secured Domains

### Syntax

```sql
CREATE DOMAIN domain_name [ AS ] data_type
    [ DEFAULT default_expr ]
    [ NOT NULL | NULL ]
    [ CHECK (check_expr) ]
    [ MASKED WITH (FUNCTION = mask_function) ]
    [ ENCRYPTED WITH (ALGORITHM = algo, KEY_ID = key_id) ]
    [ AUDIT ACCESS ]
    [ GRANT USAGE TO role [, ...] ];
```

### Parameters

| Parameter | Description |
|-----------|-------------|
| `MASKED WITH` | Default masking for all columns of this domain |
| `ENCRYPTED WITH` | Automatic encryption at rest |
| `AUDIT ACCESS` | Log all access to this domain |
| `GRANT USAGE` | Control which roles can use the domain |

## Domain Security Examples

### PII Domains

```sql
-- SSN Domain
CREATE DOMAIN ssn_domain AS TEXT
    CHECK (VALUE ~ '^\d{3}-\d{2}-\d{4}$')
    MASKED WITH (FUNCTION = 'partial(VALUE, 7, NULL, "XXX-XX-")')
    AUDIT ACCESS;

-- Email Domain
CREATE DOMAIN email_domain AS TEXT
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$')
    MASKED WITH (FUNCTION = 'email(VALUE)');

-- Phone Domain
CREATE DOMAIN phone_domain AS TEXT
    CHECK (VALUE ~ '^\+?\d{10,15}$')
    MASKED WITH (FUNCTION = 'phone(VALUE)');

-- Use in tables
CREATE TABLE customers (
    id UUID PRIMARY KEY,
    ssn ssn_domain,        -- Automatically masked
    email email_domain,     -- Automatically masked
    phone phone_domain      -- Automatically masked
);

-- Query results are automatically masked
SELECT ssn, email FROM customers;
-- Result: XXX-XX-6789 | ***@example.com
```

### Financial Domains

```sql
-- Encrypted credit card domain
CREATE DOMAIN credit_card_domain AS TEXT
    CHECK (LENGTH(VALUE) >= 13 AND LENGTH(VALUE) <= 19)
    MASKED WITH (FUNCTION = 'credit_card(VALUE)')
    ENCRYPTED WITH (ALGORITHM = 'AES-256-GCM', KEY_ID = 'payment_keys');

-- Salary domain with rounding
CREATE DOMAIN salary_domain AS DECIMAL(10,2)
    CHECK (VALUE >= 0)
    MASKED WITH (FUNCTION = 'ROUND(VALUE / 10000) * 10000');

-- Use in tables
CREATE TABLE employees (
    id UUID PRIMARY KEY,
    credit_card credit_card_domain,  -- Encrypted + masked
    salary salary_domain              -- Masked (rounded)
);
```

### Classification Levels

```sql
-- Confidential domain - full encryption and masking
CREATE DOMAIN confidential_text AS TEXT
    MASKED WITH (FUNCTION = 'full(VALUE)')
    ENCRYPTED WITH (ALGORITHM = 'AES-256-GCM')
    AUDIT ACCESS;

-- Internal domain - partial masking
CREATE DOMAIN internal_text AS TEXT
    MASKED WITH (FUNCTION = 'partial(VALUE, 50)');

-- Public domain - no masking
CREATE DOMAIN public_text AS TEXT;

-- Use classification
CREATE TABLE documents (
    id UUID PRIMARY KEY,
    title public_text,
    summary internal_text,
    content confidential_text
);
```

## Domain Constraints + Security

### Validation and Security Together

```sql
CREATE DOMAIN password_hash_domain AS TEXT
    -- Ensure strong hashes only
    CHECK (LENGTH(VALUE) >= 60 AND VALUE LIKE '$2%')  -- bcrypt format
    -- Never display
    MASKED WITH (FUNCTION = 'full(VALUE)')
    -- Always encrypt at rest
    ENCRYPTED WITH (ALGORITHM = 'AES-256-GCM');
```

## Domain Encryption

### Automatic Encryption

```sql
-- Create encrypted domain
CREATE DOMAIN encrypted_note AS TEXT
    ENCRYPTED WITH (
        ALGORITHM = 'AES-256-GCM',
        KEY_ID = 'column_encryption_key'
    );

-- Data is encrypted automatically on INSERT/UPDATE
INSERT INTO notes (content) VALUES ('Secret information');
-- Stored as encrypted bytes

-- Decrypted automatically on SELECT (if authorized)
SELECT content FROM notes;
-- Returns: 'Secret information' (decrypted)
```

### Encryption Algorithms

| Algorithm | Description | Use Case |
|-----------|-------------|----------|
| `AES-256-GCM` | Authenticated encryption | General purpose |
| `AES-256-CBC` | Standard encryption | Legacy compatibility |
| `CHACHA20-POLY1305` | Stream cipher | Mobile/low-power |

## Domain Access Control

### USAGE Privilege

```sql
-- Restrict domain usage
CREATE DOMAIN restricted_domain AS INTEGER
    GRANT USAGE TO admin_role, app_role;

-- Other roles cannot create columns with this domain
CREATE TABLE test (col restricted_domain);
-- Error: no USAGE privilege on domain
```

### Role-Specific Domain Masking

```sql
-- Default mask for most users
CREATE DOMAIN salary_domain AS DECIMAL(10,2)
    MASKED WITH (FUNCTION = 'full(VALUE)')
    FOR ROLE PUBLIC;

-- Less restrictive for managers
ALTER DOMAIN salary_domain
    SET MASKED WITH (FUNCTION = 'ROUND(VALUE / 10000) * 10000')
    FOR ROLE manager_role;

-- No mask for HR
ALTER DOMAIN salary_domain
    DROP MASKED FOR ROLE hr_role;
```

## Domain Inheritance

### Creating Sub-Domains

```sql
-- Base domain
CREATE DOMAIN base_email AS TEXT
    CHECK (VALUE LIKE '%@%');

-- Specialized domain with additional masking
CREATE DOMAIN corporate_email AS base_email
    MASKED WITH (FUNCTION = 'email(VALUE)')
    CHECK (VALUE LIKE '%@company.com');
```

## Auditing Domain Access

### Automatic Audit Logging

```sql
-- Domain with audit
CREATE DOMAIN audited_ssn AS TEXT
    MASKED WITH (FUNCTION = 'partial(VALUE, 7)')
    AUDIT ACCESS;

-- All queries accessing this domain are logged
SELECT ssn FROM employees WHERE id = '...';
-- Logs: user, timestamp, query, domain accessed
```

### Audit Log Format

```
audit_time          | user    | table    | column | domain      | operation
--------------------+---------+----------+--------+-------------+-----------
2024-01-15 10:23:45 | analyst | employees| ssn    | audited_ssn | SELECT
2024-01-15 10:24:12 | hr_user | employees| ssn    | audited_ssn | SELECT
```

## Domain vs Column Security

| Aspect | Domain Security | Column Security |
|--------|-----------------|-----------------|
| Scope | All columns of domain | Single column |
| Consistency | Automatic | Manual per column |
| Maintenance | Update domain once | Update each column |
| Flexibility | Same for all columns | Different per column |

### When to Use Each

**Use Domain Security when:**
- Multiple columns need same protection (SSN fields)
- Organization-wide data classification
- Consistent masking across application
- Regulatory compliance requirements

**Use Column Security when:**
- One-off protection needed
- Different masking for same data type
- Legacy table modifications

## Complete Example: Healthcare Application

```sql
-- Create secured domains
CREATE DOMAIN patient_mrn AS TEXT
    CHECK (VALUE ~ '^MRN\d{8}$')
    MASKED WITH (FUNCTION = 'partial(VALUE, 4)')
    ENCRYPTED WITH (ALGORITHM = 'AES-256-GCM')
    AUDIT ACCESS;

CREATE DOMAIN phi_text AS TEXT
    MASKED WITH (FUNCTION = 'full(VALUE)')
    ENCRYPTED WITH (ALGORITHM = 'AES-256-GCM')
    AUDIT ACCESS;

CREATE DOMAIN diagnosis_code AS TEXT
    CHECK (VALUE ~ '^ICD-10-[A-Z]\d{2}(\.\d{1,2})?$')
    MASKED WITH (FUNCTION = 'partial(VALUE, 3)');

-- Use domains in tables
CREATE TABLE patients (
    mrn patient_mrn PRIMARY KEY,
    name TEXT,
    address phi_text,
    diagnosis diagnosis_code,
    notes phi_text
);

-- Access is automatically secured
SELECT * FROM patients;
-- mrn: MRN****5678
-- name: John Doe
-- address: [REDACTED]
-- diagnosis: ICD****A01
-- notes: [REDACTED]
```

## Completion Checklist

- [x] Domain creation with security documented
- [x] Masking at domain level explained
- [x] Encryption at domain level covered
- [x] Access control (USAGE) documented
- [x] Auditing domain access explained
- [x] Domain vs column security comparison
- [x] Complete healthcare example

## See Also

- [Row-Level Security](02_row_level_security_design_and_validation.md)
- [Column-Level Security](03_column_security_and_masking_patterns.md)
- [CREATE DOMAIN](../../language_reference/syntax_guide/ddl/database_and_schema/README.md)
