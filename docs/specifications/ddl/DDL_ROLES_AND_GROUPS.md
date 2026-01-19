# **DDL Specification: Roles and Groups**

## **Overview**

In ScratchBird, security principals are managed through **Roles**. A role is an entity that can own database objects and have database privileges. A role can be considered a "user" (if it has the LOGIN privilege) or a "group" (if it doesn't have LOGIN and is used to bundle permissions).

The key feature is **Role Composition**: roles can be granted membership in other roles, creating a powerful, hierarchical system of permission inheritance that functions exactly like traditional user groups.

## **Roles vs. Groups: A Clarification**

* **Role:** The SQL-standard term used in ScratchBird. A role is a collection of permissions.  
* **Group:** A conceptual term. In ScratchBird, a "group" is simply a role that other roles (or users) are members of.  
* **User:** A role with the LOGIN privilege.

**Inheritance Model:** When a user logs in, their security token contains the sum of all permissions from all roles they are a member of, recursively. If user\_jdoe is a member of the developers role, and the developers role is a member of the employees role, then user\_jdoe inherits the permissions of both developers AND employees permanently for their session. This is the "permanent inheritance" group model.

## **CREATE ROLE**

Creates a new role, which can function as a user or a group.

### **Syntax**

CREATE ROLE \<role\_name\>  
    \[ WITH  
        ADMIN \<role\_list\>  
      | LOGIN | NOLOGIN  
      | \[ENCRYPTED\] PASSWORD { '\<password\>' | NULL }  
      | CREATEDB | NOCREATEDB  
      | CREATEROLE | NOCREATEROLE  
      | INHERIT | NOINHERIT  
      | IN ROLE \<role\_list\>  
      | ROLE \<role\_list\>  
      ...  
    \];

### **Key Parameters**

* LOGIN: Allows the role to log in, making it a "user".  
* NOLOGIN: (Default) Prevents login, making it a "group".  
* PASSWORD: Sets the password for login-enabled roles.  
* IN ROLE \<role\_list\>: Makes the new role a member of the specified existing role(s) at creation time.

### **Examples**

**1\. Create a "group" role:**

\-- This role cannot log in but can be used to group permissions.  
CREATE ROLE developers NOLOGIN;  
GRANT SELECT, INSERT, UPDATE, DELETE ON SCHEMA dev\_schema TO developers;

**2\. Create a "user" role:**

CREATE ROLE jdoe WITH  
    LOGIN  
    PASSWORD 'a\_secure\_password';

**3\. Create a user and immediately add them to a group:**

CREATE ROLE ssmith WITH  
    LOGIN  
    PASSWORD 'another\_password'  
    IN ROLE developers; \-- ssmith is now a member of the developers group

## **ALTER ROLE**

Modifies the attributes of a role.

### **Syntax**

ALTER ROLE \<role\_name\>  
    \[ WITH \<options\> \] \-- Same options as CREATE ROLE  
    | RENAME TO \<new\_role\_name\>;

### **Examples**

**1\. Grant a user the ability to log in:**

ALTER ROLE service\_account WITH LOGIN;

**2\. Change a user's password:**

ALTER ROLE jdoe WITH PASSWORD 'new\_stronger\_password';

**3\. Add a connection limit:**

ALTER ROLE api\_user WITH CONNECTION LIMIT 10;

## **DROP ROLE**

Removes a role. The role cannot be dropped if it owns any database objects or if other roles depend on it.

### **Syntax**

DROP ROLE \[ IF EXISTS \] \<role\_name\> \[ , ... \];

### **Example**

DROP ROLE IF EXISTS former\_employee;

## **CREATE USER / ALTER USER / DROP USER**

ScratchBird treats **users as roles with LOGIN**. The USER statements are convenience aliases for
role management with login enabled.

### **CREATE USER**

Alias for `CREATE ROLE <name> WITH LOGIN ...`.

#### **Syntax**

CREATE USER \<user\_name\>  
    \[ WITH  
        PASSWORD { '\<password\>' | NULL }  
      | CREATEDB | NOCREATEDB  
      | CREATEROLE | NOCREATEROLE  
      | INHERIT | NOINHERIT  
      | IN ROLE \<role\_list\>  
      | ROLE \<role\_list\>  
      ...  
    \];

#### **Notes**
- `LOGIN` is implied and cannot be disabled in CREATE USER.
- `CREATE USER` MUST create a role record with `can_login = true`.

### **ALTER USER**

Alias for `ALTER ROLE <name> WITH ...` and restricted to roles with `LOGIN`.

#### **Syntax**

ALTER USER \<user\_name\>  
    \[ WITH \<options\> \]  
    | RENAME TO \<new\_user\_name\>;

### **DROP USER**

Alias for `DROP ROLE <name>` and restricted to roles with `LOGIN`.

#### **Syntax**

DROP USER \[ IF EXISTS \] \<user\_name\> \[ , ... \];

## **Managing Membership (GRANT/REVOKE ROLE)**

This is the core of the group functionality, where you make roles members of other roles.

### **Syntax**

\-- Add a user/role to a group/role  
GRANT \<group\_role\_list\> TO \<member\_role\_list\> \[ WITH ADMIN OPTION \];

\-- Remove a user/role from a group/role  
REVOKE \<group\_role\_list\> FROM \<member\_role\_list\>;

### **Examples**

**1\. Add a user to the developers group:**

GRANT developers TO jdoe;

**2\. Create a hierarchy of roles (groups):**

\-- Create base roles  
CREATE ROLE employees NOLOGIN;  
CREATE ROLE engineering NOLOGIN;

\-- Create a hierarchy: engineering is a subgroup of employees  
GRANT employees TO engineering;

\-- Add the developers group to the engineering group  
GRANT engineering TO developers;

\-- Now, any member of 'developers' (like jdoe and ssmith)  
\-- automatically inherits permissions from 'engineering' AND 'employees'.

**3\. Remove a user from a group:**

REVOKE developers FROM ssmith;  

## **System Catalog Exposure**

Users and roles are exposed through read-only system views for administration tools:

### **sys.users**

Lists roles that can login.

| Column | Type | Description |
| --- | --- | --- |
| user_id | UUID | User UUID |
| user_name | TEXT | User name |
| can_login | BOOLEAN | Always true for users |
| is_superuser | BOOLEAN | Superuser flag |
| default_schema | TEXT | Default schema (nullable) |
| created_at | TIMESTAMPTZ | Creation time |
| last_login_at | TIMESTAMPTZ | Last login time (nullable) |
| auth_provider | TEXT | Auth provider name (nullable) |
| password_state | TEXT | ok / expired / must_change / unknown |

### **sys.roles**

Lists all roles (including users).

| Column | Type | Description |
| --- | --- | --- |
| role_id | UUID | Role UUID |
| role_name | TEXT | Role name |
| can_login | BOOLEAN | True if role can login |
| is_superuser | BOOLEAN | Superuser flag |
| is_system_role | BOOLEAN | System-defined role |
| default_schema | TEXT | Default schema (nullable) |
| created_at | TIMESTAMPTZ | Creation time |

### **sys.role_members**

Role membership and inheritance.

| Column | Type | Description |
| --- | --- | --- |
| role_id | UUID | Role or group being granted |
| role_name | TEXT | Role name |
| member_id | UUID | User/role receiving membership |
| member_name | TEXT | Member name |
| admin_option | BOOLEAN | WITH ADMIN OPTION |
| is_default | BOOLEAN | Default role on login |
| grantor_name | TEXT | Grantor name |
| granted_at | TIMESTAMPTZ | Grant time |

Access control:
- Superuser can read all rows.
- Regular users can read their own user row and effective role memberships.
