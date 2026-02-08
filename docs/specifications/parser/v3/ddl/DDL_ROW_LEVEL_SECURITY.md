# **Row-Level Security Specification**

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## **Implementation Status**

| Feature | Status | Notes |
|---------|--------|-------|
| `ALTER TABLE ... ENABLE/DISABLE ROW LEVEL SECURITY` | ✅ Implemented | V2 Parser + Executor |
| `CREATE POLICY` | ✅ Implemented | V2 Parser + Executor + CatalogManager |
| `DROP POLICY` | ✅ Implemented | V2 Parser + Executor + CatalogManager |
| `ALTER POLICY` | ✅ Implemented | V2 Parser + Executor + CatalogManager (new) |
| Policy Enforcement (USING/WITH CHECK) | ✅ Implemented | DML operations respect policies |
| Policy Enable/Disable | ✅ Implemented | Via `alterPolicy()` API |
| PERMISSIVE/RESTRICTIVE policies | 🚧 Partial | Parsed but all treated as PERMISSIVE |
| Role-based policy targeting | ✅ Implemented | PUBLIC and role-specific policies |

## **1\. Introduction**

Row-Level Security (RLS) is an advanced security feature that controls data access at the row level. While standard SQL permissions operate at the table or column level (GRANT SELECT ON ...), RLS allows you to define fine-grained policies that determine which specific rows a user is allowed to view or modify within a single table.

This is essential for applications where different users or roles should see different subsets of data within the same table, such as in multi-tenant applications, or when sales representatives should only see their own customers.

### **How It Works**

RLS is implemented through **security policies** that are attached to a table. A policy is essentially a function that returns a boolean TRUE or FALSE for each row.

* When a user queries the table, the policy's expression is evaluated for every row.  
* Only rows for which the expression evaluates to TRUE are visible to the user or can be operated on.

This filtering is applied automatically and transparently for all queries against the table.

## **2\. Enabling and Disabling RLS**

Before policies can be applied, Row-Level Security must be enabled on the table.

### **ALTER TABLE ... ENABLE ROW LEVEL SECURITY**

This command activates RLS for a specific table. Once enabled, access to the table is restricted by default (unless a permissive policy exists).

**Syntax:**

ALTER TABLE table\_name  
    ENABLE ROW LEVEL SECURITY;

### **ALTER TABLE ... DISABLE ROW LEVEL SECURITY**

This command deactivates RLS for the table. All policies attached to the table are ignored, and access reverts to the standard grant/revoke permission model.

**Syntax:**

ALTER TABLE table\_name  
    DISABLE ROW LEVEL SECURITY;

## **3\. Creating Security Policies**

A policy defines the rules for accessing rows.

### **CREATE POLICY**

**Syntax:**

CREATE POLICY policy\_name ON table\_name  
    \[ AS { PERMISSIVE | RESTRICTIVE } \]  
    \[ FOR { ALL | SELECT | INSERT | UPDATE | DELETE } \]  
    \[ TO { role\_name | PUBLIC } \[, ...\] \]  
    \[ USING ( using\_expression ) \]  
    \[ WITH CHECK ( check\_expression ) \];

**Key Clauses:**

* **policy\_name**: A unique name for the policy.  
* **AS { PERMISSIVE | RESTRICTIVE }**:  
  * PERMISSIVE (Default): Policies are combined with OR. A row is accessible if *any* permissive policy passes.  
  * RESTRICTIVE: Policies are combined with AND. A row is accessible only if *all* restrictive policies pass.  
* **FOR command**: Specifies which DML commands the policy applies to (SELECT, INSERT, UPDATE, DELETE, or ALL).  
* **TO role**: Specifies which user roles the policy applies to.  
* **USING ( expression )**: The core of the policy. This expression is evaluated for existing rows to determine if they are visible (SELECT) or can be modified (UPDATE, DELETE).  
* **WITH CHECK ( expression )**: This expression is evaluated for new or modified rows to ensure they comply with the policy (INSERT, UPDATE).

**Example: A Simple "Users can only see their own data" Policy**

\-- Assume a function current\_user\_id() exists that returns the ID of the logged-in user.  
\-- And a table 'documents' with a 'owner\_id' column.

\-- 1\. Enable RLS on the table  
ALTER TABLE documents ENABLE ROW LEVEL SECURITY;

\-- 2\. Create the policy  
CREATE POLICY user\_access\_policy ON documents  
    FOR ALL                        \-- Applies to SELECT, INSERT, UPDATE, DELETE  
    TO PUBLIC                      \-- Applies to all users/roles  
    USING ( owner\_id \= current\_user\_id() ) \-- Users can only see/modify rows they own  
    WITH CHECK ( owner\_id \= current\_user\_id() ); \-- Users can only create/update rows for themselves

## **4\. Managing Policies**

### **ALTER POLICY**

Modifies an existing policy, for example, to change its expression or the roles it applies to.

**Syntax:**

ALTER POLICY policy\_name ON table\_name  
    \[ TO { role\_name | PUBLIC } \[, ...\] \]  
    \[ USING ( using\_expression ) \]  
    \[ WITH CHECK ( check\_expression ) \];

**Example:**

\-- Change the policy to also allow managers to see documents  
\-- Assume a function user\_is\_manager() exists.  
ALTER POLICY user\_access\_policy ON documents  
    USING ( owner\_id \= current\_user\_id() OR user\_is\_manager() );

### **DROP POLICY**

Removes a security policy from a table.

**Syntax:**

DROP POLICY \[ IF EXISTS \] policy\_name ON table\_name;

## **5\. Advanced Examples**

### **Multi-Tenant Application Policy**

\-- 'tenants' table: tenant\_id, name  
\-- 'users' table: user\_id, tenant\_id  
\-- 'invoices' table: invoice\_id, tenant\_id, amount

\-- A session variable @app.current\_tenant\_id is set upon user login

CREATE POLICY tenant\_isolation\_policy ON invoices  
    FOR ALL  
    TO PUBLIC  
    USING ( tenant\_id \= current\_setting('app.current\_tenant\_id')::UUID )  
    WITH CHECK ( tenant\_id \= current\_setting('app.current\_tenant\_id')::UUID );

This single policy ensures that users can *only* ever interact with invoices belonging to their own tenant, providing strong data isolation.

### **Restrictive Policy for Sensitive Data**

\-- A table with a 'sensitivity\_level' column (e.g., 1=Public, 5=Secret)  
\-- A function user\_clearance\_level() returns the clearance of the current user

\-- First, a permissive policy allows access to non-sensitive data  
CREATE POLICY public\_access ON sensitive\_documents AS PERMISSIVE  
    FOR SELECT  
    TO PUBLIC  
    USING ( sensitivity\_level \<= 1 );

\-- Second, a restrictive policy that must ALSO be true for sensitive data  
CREATE POLICY secret\_access ON sensitive\_documents AS RESTRICTIVE  
    FOR SELECT  
    TO PUBLIC  
    USING ( sensitivity\_level \<= user\_clearance\_level() );

In this scenario, a user can see a row if (sensitivity\_level \<= 1\) OR (sensitivity\_level \<= user\_clearance\_level()). However, because the second policy is RESTRICTIVE, if a row has sensitivity\_level \= 5, the user *must* have a clearance of 5 or higher. The permissive policy alone is not enough.
---

## **Implementation Details**

### **Parser Support (V2)**

The V2 parser (the main parser for ScratchBird) now supports:

1. **CREATE POLICY** - Full syntax including:
   - `AS { PERMISSIVE | RESTRICTIVE }` (parsed, treated as PERMISSIVE)
   - `FOR { ALL | SELECT | INSERT | UPDATE | DELETE }`
   - `TO { role_name | PUBLIC }`
   - `USING ( expression )`
   - `WITH CHECK ( expression )`

2. **ALTER POLICY** - Supports:
   - `TO { role_name | PUBLIC }` (role changes)
   - `USING ( expression )` (expression changes)
   - `WITH CHECK ( expression )` (check expression changes)

3. **DROP POLICY** - Supports:
   - `IF EXISTS`
   - Policy name and table reference

### **CatalogManager API**

```cpp
// Create a new policy
Status createPolicy(const ID& table_id, 
                    const std::string& policy_name,
                    PolicyType type,
                    const std::vector<std::string>& roles,
                    const std::string& using_expr,
                    const std::string& with_check_expr,
                    ID& policy_id_out,
                    ErrorContext* ctx);

// Alter an existing policy
Status alterPolicy(const ID& table_id,
                   const std::string& policy_name,
                   int is_enabled,  // -1 = no change, 0 = disable, 1 = enable
                   const std::string& using_expr,      // empty = no change
                   const std::string& with_check_expr, // empty = no change
                   ErrorContext* ctx);

// Drop a policy
Status dropPolicy(const ID& table_id,
                  const std::string& policy_name,
                  ErrorContext* ctx);
```

### **SBLR Opcodes**

| Opcode | Value | Description |
|--------|-------|-------------|
| `EXT_CREATE_POLICY` | 0xD7 | CREATE POLICY statement |
| `EXT_DROP_POLICY` | 0xD8 | DROP POLICY statement |
| `EXT_ALTER_TABLE_RLS` | 0xD9 | ALTER TABLE ... ROW LEVEL SECURITY |
| `EXT_ALTER_POLICY` | 0xE5 | ALTER POLICY statement (new) |

### **Security Enforcement**

Policy enforcement happens during DML operations:
- **SELECT**: Rows are filtered by the USING expression
- **INSERT**: New rows must satisfy the WITH CHECK expression
- **UPDATE**: Both USING (for visibility) and WITH CHECK (for new values) are checked
- **DELETE**: USING expression determines which rows can be deleted

### **Policy Cache**

Policies are cached in memory by the CatalogManager for fast lookup during query execution. The cache is invalidated when:
- A policy is created, altered, or dropped
- RLS is enabled/disabled on a table
- The security policy epoch is bumped

