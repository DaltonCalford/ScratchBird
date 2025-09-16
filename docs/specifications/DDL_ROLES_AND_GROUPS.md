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
