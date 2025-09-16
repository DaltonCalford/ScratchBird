# **Security Model Specification**

## **1\. Introduction**

The ScratchBird security model is designed to be comprehensive, granular, and flexible, providing database administrators with precise control over data access and manipulation. The model is built upon three distinct but complementary pillars that work together to create a robust, multi-layered security environment.

### **The Three Pillars of Security**

1. **Access Control (Roles & Schemas)**: This is the traditional layer of security that defines *who* can perform *what actions* on *which database objects*. It is implemented through a powerful combination of hierarchical roles (which function as groups) and cascading schema-level permissions.  
2. **Data-Centric Security (Domains)**: This is an advanced, modern layer of security that attaches rules directly to the *data's type*. By using a DOMAIN, security policies for sensitive data (like masking, auditing, and encryption) travel with the data itself, ensuring consistent enforcement regardless of how it is accessed.  
3. **Procedural Security (Definer vs. Invoker Rights)**: This layer defines the privilege context for executing stored procedures and functions. It controls whether a routine runs with the permissions of the user who defined it (DEFINER) or the user who is calling it (INVOKER).

This document details each of these pillars.

## **2\. Pillar 1: Access Control**

### **2.1. Hierarchical Roles (Groups)**

In ScratchBird, the standard SQL ROLE is used to implement a powerful system of permission inheritance, functioning identically to "Groups" in other systems (e.g., Microsoft Active Directory).

**Key Concept**: When a role is granted to another role, the grantee role permanently inherits all the privileges of the granted role. When a user is made a member of a role, they permanently inherit all privileges of that role and any roles it inherits from, recursively. This creates a powerful hierarchy for managing permissions.

**Example: Composing Roles into Groups**

\-- 1\. Create base roles with specific, granular permissions  
CREATE ROLE data\_readers;  
GRANT SELECT ON ALL TABLES IN SCHEMA public TO data\_readers;

CREATE ROLE data\_writers;  
GRANT INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO data\_writers;

CREATE ROLE schema\_designers;  
GRANT CREATE\_TABLE, ALTER\_TABLE, CREATE\_INDEX ON SCHEMA public TO schema\_designers;

\-- 2\. Compose base roles into a functional "Group"  
CREATE ROLE developers;  
GRANT data\_readers TO developers;   \-- Developers can read data  
GRANT data\_writers TO developers;   \-- Developers can also write data  
GRANT schema\_designers TO developers; \-- And they can modify the schema

\-- 3\. Grant the "Group" to a user  
CREATE USER jsmith PASSWORD '...';  
GRANT developers TO jsmith; \-- jsmith now has all read, write, and design privileges.

**Dynamic Inheritance**: If a new permission is granted to a base role, it automatically propagates up the hierarchy to all members.

\-- Grant a new privilege to the base role  
GRANT TRUNCATE ON ALL TABLES IN SCHEMA public TO data\_writers;

\-- User 'jsmith' automatically and immediately gains the TRUNCATE privilege  
\-- because they are a member of the 'developers' group.

### **2.2. Hierarchical Schema Permissions**

Beyond object-level GRANT/REVOKE, ScratchBird allows permissions to be granted on a SCHEMA. These permissions can then automatically cascade down the schema hierarchy.

**Permission Categories on Schemas:**

* **DML Rights**: SELECT, INSERT, UPDATE, DELETE, TRUNCATE  
* **DDL Rights**: CREATE\_TABLE, ALTER\_TABLE, CREATE\_VIEW, CREATE\_FUNCTION, etc.  
* **Schema Management**: CREATE\_SCHEMA, GRANT\_RIGHTS, REVOKE\_RIGHTS

**Inheritance Rules (WITH INHERITANCE)**

* **CASCADE (Default)**: Permissions apply to the specified schema and all of its current and future sub-schemas, recursively.  
* **LOCAL**: Permissions apply to the specified schema and only its direct children (one level down).  
* **NONE**: Permissions apply only to the specified schema and do not cascade.

Example: Schema Permission Inheritance  
Consider a schema structure: app \-\> sales \-\> reports.  
\-- Grant read access that cascades all the way down  
GRANT SELECT ON SCHEMA app TO reporting\_users WITH INHERITANCE CASCADE;  
\-- A member of reporting\_users can now SELECT from tables in 'app', 'app.sales', and 'app.sales.reports'.

\-- Grant table creation rights, but only in the top-level application schemas  
GRANT CREATE\_TABLE ON SCHEMA app TO app\_developers WITH INHERITANCE LOCAL;  
\-- An app\_developer can create tables in 'app.sales', but NOT in 'app.sales.reports'.

## **3\. Pillar 2: Data-Centric Security (Domains)**

This is a powerful feature where security rules are embedded within a data type (DOMAIN) itself. This ensures that protection policies for sensitive data are always enforced, no matter who accesses it or from where.

### **WITH SECURITY Clause**

When creating a DOMAIN, you can specify a SECURITY clause to attach rules.

**Example: A Self-Protecting SSN Domain**

CREATE DOMAIN social\_security\_number AS VARCHAR(11)  
    CHECK (VALUE \~ '^\\d{3}-\\d{2}-\\d{4}$')  
    WITH SECURITY (  
        \-- For users without special permissions, data is automatically masked  
        MASK\_FUNCTION \= 'mask\_ssn\_function', \-- e.g., returns '\*\*\*-\*\*-1234'

        \-- Log every single time this data is read  
        AUDIT\_ACCESS \= TRUE,

        \-- A user must have this specific permission to see the unmasked value  
        REQUIRE\_PERMISSION \= 'view\_pii\_data',

        \-- The data is automatically encrypted when stored on disk  
        ENCRYPTION \= 'AES256'  
    );

\-- Now, any table column or variable of this type automatically has these protections.  
CREATE TABLE employees (  
    id UUID PRIMARY KEY,  
    ssn social\_security\_number \-- This column is now self-protecting  
);

\-- A user with the 'view\_pii\_data' permission sees '123-45-6789'.  
\-- Any other user sees '\*\*\*-\*\*-6789'.  
\-- All access is logged, and the value is always encrypted at rest.

## **4\. Pillar 3: Procedural Security**

This pillar governs the execution context of stored procedures and functions.

### **SQL SECURITY DEFINER vs. INVOKER**

* **DEFINER (Default)**: The routine executes with the permissions of the user who **created** it. This is useful for creating controlled APIs where you want to grant users access to perform a specific, well-defined action on underlying tables without granting them direct access to the tables themselves.  
* **INVOKER**: The routine executes with the permissions of the user who is **calling** it. This is useful for creating general-purpose utility functions that operate on behalf of the current user.

**Example: Using DEFINER for Controlled Access**

\-- The 'app\_owner' owns the 'employees' table and has full rights.  
\-- Regular users have NO rights on the 'employees' table.

CREATE FUNCTION change\_employee\_phone(p\_employee\_id INT, p\_new\_phone TEXT)  
RETURNS VOID  
LANGUAGE plpgsql  
SQL SECURITY DEFINER \-- This is the key part\!  
AS $$  
BEGIN  
    \-- This UPDATE runs with the permissions of 'app\_owner', not the caller.  
    UPDATE employees SET phone\_number \= p\_new\_phone WHERE employee\_id \= p\_employee\_id;  
END;  
$$;

\-- Grant EXECUTE on the function to a regular user  
GRANT EXECUTE ON FUNCTION change\_employee\_phone TO regular\_user;

\-- 'regular\_user' can now successfully call the function to change a phone number,  
\-- even though they cannot directly UPDATE the 'employees' table.

## **5\. Permission Resolution Precedence**

When determining if a user has permission for an action, ScratchBird evaluates privileges in the following order of precedence (from highest to lowest):

1. **Superuser / Object Owner**: A superuser or the owner of an object can always perform any action on it.  
2. **Explicit DENY**: An explicit DENY on the specific object will always override any GRANT.  
3. **Explicit GRANT**: A direct GRANT of the privilege on the specific object to the user or a role they are a member of.  
4. **Inherited Schema Permissions**: A GRANT on a parent schema that cascades down to the object.  
5. **Default Privileges**: Privileges automatically granted at the time of object creation.  
6. **PUBLIC Role**: Any privileges granted to the special PUBLIC role.  
7. **Implicit Deny**: If no permissions are found through the checks above, access is denied.