## **In-Depth Analysis of the GRANT Statement**

The GRANT statement is used to give specific permissions on database objects to users, roles, or other procedures/triggers. It can also be used to grant membership in a role to a user or another role. The syntax can be broadly divided into two categories: granting object privileges and granting roles.

### **1\. Granting Privileges on Database Objects**

This is the most common form, used to control access to data and routines.

**General Syntax:**

GRANT {privilege\_list | ALL \[PRIVILEGES\]}  
ON \[OBJECT\_TYPE\] object\_name  
TO grantee\_list  
\[WITH GRANT OPTION\]  
\[GRANTED BY grantor\_name\]

#### **Clause Breakdown:**

* **GRANT {privilege\_list | ALL \[PRIVILEGES\]}**:  
  * This specifies which permissions are being granted.  
  * ALL \[PRIVILEGES\]: A shortcut to grant all applicable privileges for the given object type.  
  * privilege\_list: A comma-separated list of specific privileges.  
* **ON \[OBJECT\_TYPE\] object\_name**:  
  * This clause identifies the object to which the privileges apply.  
  * OBJECT\_TYPE: An optional keyword like TABLE, PROCEDURE, VIEW, PACKAGE, etc. It is often optional but improves clarity.  
  * object\_name: The name of the object.  
* **TO grantee\_list**:  
  * Specifies who is receiving the privileges.  
  * grantee\_list: A comma-separated list of one or more grantees.  
* **WITH GRANT OPTION**: (Optional)  
  * Allows the grantee to grant the same privileges they received to other users or roles.  
* **GRANTED BY grantor\_name**: (Optional)  
  * Allows a user with sufficient permissions (like the database owner) to grant privileges on behalf of another user. The grantor\_name is the user recorded as the granter of the privilege.

### **Privilege and Object Types**

The available privileges depend on the type of object.

| Object Type(s) | Available Privileges | ALL Includes | Description |
| :---- | :---- | :---- | :---- |
| TABLE, VIEW | SELECT, INSERT, UPDATE \[(cols)\], DELETE, REFERENCES \[(cols)\] | All five | Controls data access. UPDATE and REFERENCES can be restricted to specific columns. |
| PROCEDURE, FUNCTION, PACKAGE | EXECUTE | EXECUTE | Allows the grantee to call the routine. |
| SEQUENCE / GENERATOR | USAGE | USAGE | Allows the grantee to get the next value using NEXT VALUE FOR. |
| EXCEPTION | USAGE | USAGE | Allows the grantee to reference the exception in PSQL. |
| SCHEMA | USAGE | USAGE | Allows the grantee to create objects within the schema. |
| DOMAIN, CHARACTER SET, COLLATION | USAGE | USAGE | Allows the grantee to use the object in DDL statements (e.g., in a table definition). |

### **2\. Granting Roles**

This form is used to make a user or another role a member of a specific role, thereby inheriting its privileges.

**Syntax:**

GRANT role\_name\_list  
TO grantee\_list  
\[WITH ADMIN OPTION\]  
\[GRANTED BY grantor\_name\]

#### **Clause Breakdown:**

* **GRANT role\_name\_list**:  
  * A comma-separated list of one or more role names to be granted.  
  * DEFAULT role\_name: Grants the role and also makes it one of the user's default active roles upon connection.  
* **TO grantee\_list**:  
  * Specifies who is receiving the role membership. The grantees can be users or other roles.  
* **WITH ADMIN OPTION**: (Optional)  
  * This is the role-equivalent of WITH GRANT OPTION. It allows the grantee to grant membership in the specified role(s) to others.

### **3\. Granting DDL / System Privileges**

ScratchBird also supports granting privileges for creating, altering, or dropping entire classes of objects, either within a specific schema or database-wide.

**Syntax:**

GRANT {ddl\_privilege\_list | ALL}  
ON {schema\_object | schemaless\_object} \[ON SCHEMA schema\_name\]  
TO grantee\_list  
\[WITH GRANT OPTION\]

* **DDL Privileges**: CREATE, ALTER ANY, DROP ANY.  
* **schema\_object**: TABLE, VIEW, PROCEDURE, FUNCTION, PACKAGE, etc.  
* **schemaless\_object**: ROLE, FILTER, SCHEMA.  
* When granting on a schema\_object, you can optionally specify ON SCHEMA schema\_name to limit the privilege to that schema.

A special form exists for database-wide DDL privileges:  
GRANT {CREATE | ALTER | DROP} DATABASE TO grantee\_list

### **Grantee Types**

The TO clause can specify several types of grantees:

* \[USER\] user\_name: A specific user. The USER keyword is optional.  
* ROLE role\_name: A specific role.  
* PROCEDURE procedure\_name: A stored procedure (allowing it to act with the granted privileges).  
* FUNCTION function\_name: A user-defined function.  
* TRIGGER trigger\_name: A trigger.  
* VIEW view\_name: A view.  
* GROUP group\_name: A legacy user group concept.  
* SYSTEM PRIVILEGE privilege\_name: Grants a system-level privilege.