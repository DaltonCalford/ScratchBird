## **Schema Management Statement Details**

In ScratchBird, a schema is a namespace that contains database objects like tables, views, and procedures. The following statements are used to create, modify, and manage schemas.

### **CREATE SCHEMA**

Creates a new schema within the database.

**Syntax:**

CREATE SCHEMA \[IF NOT EXISTS\] schema\_name  
    \[schema\_option \[schema\_option ...\]\];

* **IF NOT EXISTS**: (Optional) Prevents an error from being thrown if a schema with the same name already exists.  
* **schema\_name**: The name for the new schema. This can be a simple name or a hierarchical name (e.g., parent.child).

#### **Schema Options**

* **DEFAULT CHARACTER SET 'charset\_name'**: Sets the default character set for all objects created within this schema.  
* **DEFAULT SQL SECURITY { DEFINER | INVOKER }**: Sets the default security context for routines (procedures, functions, triggers) within the schema.  
  * DEFINER: The routine executes with the privileges of the user who defined it.  
  * INVOKER: The routine executes with the privileges of the user who is calling it.

### **ALTER SCHEMA**

Modifies the properties of an existing schema.

**Syntax:**

ALTER SCHEMA schema\_name  
    alter\_schema\_option \[alter\_schema\_option ...\];

#### **Alter Schema Options**

* **SET DEFAULT CHARACTER SET 'charset\_name'**: Changes the default character set for the schema.  
* **SET DEFAULT SQL SECURITY { DEFINER | INVOKER }**: Changes the default security context for the schema.  
* **DROP DEFAULT CHARACTER SET**: Removes the schema's specific default character set, causing it to inherit the database's default.  
* **DROP DEFAULT SQL SECURITY**: Removes the schema's specific security context setting.

### **RECREATE SCHEMA**

Atomically drops and recreates a schema. If the schema does not exist, it is simply created. This is useful for development and deployment scripts.

**Syntax:**

RECREATE SCHEMA schema\_name  
    \[schema\_option \[schema\_option ...\]\];

The schema\_options are the same as for CREATE SCHEMA.

### **CREATE OR ALTER SCHEMA**

Modifies a schema if it exists, or creates it if it does not.

**Syntax:**

CREATE OR ALTER SCHEMA schema\_name  
    \[schema\_option \[schema\_option ...\]\];

The schema\_options are the same as for CREATE SCHEMA.

### **DROP SCHEMA**

Deletes an existing schema. The schema must be empty before it can be dropped.

**Syntax:**

DROP SCHEMA \[IF EXISTS\] schema\_name;

* **IF EXISTS**: (Optional) Prevents an error from being thrown if the schema does not exist.

### **Session-Level Schema Statements**

These commands manage the current session's schema context.

#### **SET SCHEMA**

Sets the current default schema for the session. When objects are referenced without a schema name, the database will look for them in the schema set by this command.

**Syntax:**

SET SCHEMA 'schema\_name';

The grammar also defines syntax for navigating hierarchical schemas:

* SET SCHEMA UP \[n\]  
* SET SCHEMA DOWN 'child\_name'  
* SET SCHEMA ROOT  
* SET SCHEMA HOME

#### **SET HOME SCHEMA**

Sets the home schema for the current user or for a specified user. The home schema is the default schema a user is placed in upon connection.

**Syntax:**

SET HOME SCHEMA 'schema\_name';

SET HOME SCHEMA FOR USER 'user\_name' TO 'schema\_name';  
