<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# Column-Level Security (CLS) and Masking

[Prev](./02_row_level_security_design_and_validation.md) | [Next](./04_domain_visibility_and_policy_enforcement.md) | [Topic README](./README.md) | [Security Hardening README](../../README.md)

## Coverage and Evidence Status

Status: Complete

## Synopsis

Column-Level Security (CLS) controls access to specific columns, with optional masking to show partial or obfuscated data to unauthorized users.

## Column Privileges

### GRANT/REVOKE Column Access

```sql
-- Grant access to specific columns
GRANT SELECT (id, name, email) ON employees TO hr_role;

-- Revoke column access
REVOKE SELECT (salary) ON employees FROM employee_role;

-- Grant update on specific column
GRANT UPDATE (status) ON orders TO support_role;
```

### Column Privilege Behavior

```sql
-- User with column privileges
GRANT SELECT (id, name) ON employees TO limited_user;

-- This works:
SELECT id, name FROM employees;

-- This fails:
SELECT * FROM employees;  -- Error: no privilege on other columns
SELECT salary FROM employees;  -- Error: no privilege on salary
```

## Column Masking

### Basic Masking

```sql
-- Create masked column
ALTER TABLE employees 
    ALTER COLUMN ssn SET MASKED WITH (FUNCTION = 'partial(ssn, 0, 4, "***-**-")');

-- Result: 123-45-6789 becomes ***-**-6789
```

### Built-in Masking Functions

| Function | Description | Example |
|----------|-------------|---------|
| `partial(col, start, end, mask)` | Partial masking | `partial(ssn, 0, 5, 'XXX-XX-')` |
| `full(col)` | Full mask (NULL or ****) | `full(password)` |
| `email(col)` | Mask email local part | `john@email.com` → `***@email.com` |
| `credit_card(col)` | Show last 4 digits | `****-****-****-1234` |
| `phone(col)` | Mask phone number | `(***) ***-1234` |
| `random(col)` | Random value same type | Random number, string, etc. |
| `fixed(col, value)` | Fixed replacement | Always returns 'REDACTED' |
| `custom(col, expr)` | Custom expression | Any SQL expression |

### Masking Examples

```sql
-- Credit card: show last 4
ALTER TABLE payments
    ALTER COLUMN card_number SET MASKED 
    WITH (FUNCTION = 'credit_card(card_number)');

-- Email: show domain only
ALTER TABLE users
    ALTER COLUMN email SET MASKED
    WITH (FUNCTION = 'email(email)');

-- Phone: mask all but last 4
ALTER TABLE customers
    ALTER COLUMN phone SET MASKED
    WITH (FUNCTION = 'partial(phone, 0, NULL, "(***) ***-")');

-- Full mask for sensitive data
ALTER TABLE users
    ALTER COLUMN password_hash SET MASKED
    WITH (FUNCTION = 'full(password_hash)');

-- Random masking for analytics
ALTER TABLE employees
    ALTER COLUMN salary SET MASKED
    WITH (FUNCTION = 'random(salary)');
```

### Custom Masking Functions

```sql
-- Create custom masking function
CREATE OR REPLACE FUNCTION mask_salary(salary DECIMAL) RETURNS DECIMAL AS $$
BEGIN
    -- Round to nearest 10K
    RETURN ROUND(salary / 10000) * 10000;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- Apply custom mask
ALTER TABLE employees
    ALTER COLUMN salary SET MASKED
    WITH (FUNCTION = 'mask_salary(salary)');
```

## Role-Based Masking

### Different Masks for Different Roles

```sql
-- Full mask for general users
ALTER TABLE employees
    ALTER COLUMN salary SET MASKED
    WITH (FUNCTION = 'full(salary)')
    FOR ROLE app_user;

-- Partial mask for managers
ALTER TABLE employees
    ALTER COLUMN salary SET MASKED
    WITH (FUNCTION = 'mask_salary(salary)')
    FOR ROLE manager_role;

-- No mask for HR
ALTER TABLE employees
    ALTER COLUMN salary DROP MASKED
    FOR ROLE hr_role;
```

### Masking Policy Priority

```sql
-- More specific roles take precedence
ALTER TABLE employees
    ALTER COLUMN ssn SET MASKED WITH (FUNCTION = 'full(ssn)')
    FOR ROLE PUBLIC;  -- Default for everyone

ALTER TABLE employees  
    ALTER COLUMN ssn SET MASKED WITH (FUNCTION = 'partial(ssn, 7)')
    FOR ROLE support_role;  -- Less restrictive for support
```

## Combined RLS and CLS

```sql
-- Table with both RLS and CLS
CREATE TABLE employee_records (
    id UUID PRIMARY KEY,
    name TEXT,
    department TEXT,
    salary DECIMAL(10,2),
    ssn TEXT,
    performance_review TEXT
);

-- Enable RLS
ALTER TABLE employee_records ENABLE ROW LEVEL SECURITY;

-- RLS: See only your department
CREATE POLICY dept_isolation ON employee_records
    FOR SELECT
    USING (department = get_user_department());

-- CLS: Mask salary for non-managers
ALTER TABLE employee_records
    ALTER COLUMN salary SET MASKED
    WITH (FUNCTION = 'full(salary)')
    FOR ROLE employee_role;

-- CLS: Mask SSN for everyone except HR
ALTER TABLE employee_records
    ALTER COLUMN ssn SET MASKED
    WITH (FUNCTION = 'partial(ssn, 0, 5, "XXX-XX-")')
    FOR ROLE PUBLIC;
```

## View Security Isolation

### View-Based Column Security

```sql
-- Base table with all columns
CREATE TABLE employee_private (
    id UUID PRIMARY KEY,
    name TEXT,
    ssn TEXT,
    salary DECIMAL,
    bank_account TEXT
);

-- Public view (masked/excluded columns)
CREATE VIEW employee_public AS
SELECT id, name
FROM employee_private;

-- HR view (more columns)
CREATE VIEW employee_hr AS
SELECT id, name, ssn, salary
FROM employee_private;

-- Grant on views, not base table
GRANT SELECT ON employee_public TO PUBLIC;
GRANT SELECT ON employee_hr TO hr_role;

-- Base table inaccessible
REVOKE ALL ON employee_private FROM PUBLIC;
```

### View Definition Security

```sql
-- Users can't see view definition (unless granted)
-- Implementation detail: Views have their own RLS/CLS context
```

## Masking Patterns

### PII Protection

```sql
-- Standard PII masking
ALTER TABLE customers
    ALTER COLUMN ssn SET MASKED WITH (FUNCTION = 'partial(ssn, 7)'),
    ALTER COLUMN dob SET MASKED WITH (FUNCTION = 'partial(dob::TEXT, 0, 4, "XXXX-")'),
    ALTER COLUMN email SET MASKED WITH (FUNCTION = 'email(email)'),
    ALTER COLUMN phone SET MASKED WITH (FUNCTION = 'phone(phone)');
```

### Financial Data

```sql
-- Financial masking
ALTER TABLE accounts
    ALTER COLUMN balance SET MASKED 
        WITH (FUNCTION = 'fixed(balance, -999)'),  -- Obscured value
    ALTER COLUMN account_number SET MASKED 
        WITH (FUNCTION = 'partial(account_number, 12, 4, "************")');
```

### Healthcare Data

```sql
-- HIPAA-style masking
ALTER TABLE patients
    ALTER COLUMN mrn SET MASKED WITH (FUNCTION = 'partial(mrn, 4)'),
    ALTER COLUMN diagnosis SET MASKED WITH (FUNCTION = 'fixed(diagnosis, '[REDACTED]')'),
    ALTER COLUMN notes SET MASKED WITH (FUNCTION = 'full(notes)');
```

## Dynamic Masking

### Context-Aware Masking

```sql
-- Mask based on session variables
CREATE OR REPLACE FUNCTION dynamic_mask(val TEXT) RETURNS TEXT AS $$
BEGIN
    IF current_setting('app.masking_level') = 'high' THEN
        RETURN '***REDACTED***';
    ELSIF current_setting('app.masking_level') = 'medium' THEN
        RETURN partial(val, 4);
    ELSE
        RETURN val;
    END IF;
END;
$$ LANGUAGE plpgsql;

-- Apply dynamic mask
ALTER TABLE sensitive_data
    ALTER COLUMN content SET MASKED
    WITH (FUNCTION = 'dynamic_mask(content)');
```

## Auditing Masked Access

```sql
-- Log when masked columns are accessed
CREATE OR REPLACE FUNCTION audit_masked_access() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO audit_log (user_id, table_name, column_name, access_time)
    VALUES (current_user_id(), TG_TABLE_NAME, TG_COLUMN_NAME, NOW());
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Apply to masked columns
ALTER TABLE employees
    ALTER COLUMN ssn SET MASKED
    WITH (FUNCTION = 'partial(ssn, 7)', AUDIT = TRUE);
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `insufficient_column_privilege` | No SELECT on column |
| `masking_error` | Mask function failed |

## Completion Checklist

- [x] Column privileges explained
- [x] Masking functions documented
- [x] Role-based masking covered
- [x] Combined RLS/CLS patterns
- [x] View security isolation
- [x] Industry patterns (PII, financial, healthcare)
- [x] Dynamic masking examples

## See Also

- [Row-Level Security](02_row_level_security_design_and_validation.md)
- [Domain Security](04_domain_visibility_and_policy_enforcement.md)
- [GRANT Column Privileges](../../language_reference/syntax_guide/security/01_grant_syntax.md)
