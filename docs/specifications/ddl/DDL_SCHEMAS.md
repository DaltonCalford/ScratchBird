# **DDL Specification: Schemas**

## **Overview**

A schema is a namespace within a database that contains a collection of objects such as tables, views, functions, and indexes. Schemas are used to organize database objects logically, manage permissions, and avoid naming conflicts.

## **CREATE SCHEMA**

Creates a new schema in the current database.

### **Syntax**

CREATE SCHEMA \[ IF NOT EXISTS \] \<schema\_name\>  
    \[ AUTHORIZATION \<owner\_name\> \]  
    \[ PATH \<schema\_path\> \];

### **Parameters**

* IF NOT EXISTS: Prevents an error if the schema already exists.  
* AUTHORIZATION: Specifies the user who will own the schema. By default, it's the user executing the command.  
* PATH: Defines a search path within the schema, useful for hierarchical schema structures.

### **Examples**

**1\. Create a basic schema:**

CREATE SCHEMA sales;

**2\. Create a schema for a different user:**

CREATE SCHEMA IF NOT EXISTS reporting AUTHORIZATION 'reporting\_user';

**3\. Create a schema with a nested structure (path-based):**

CREATE SCHEMA sales.international;  
\-- This logically groups 'international' under 'sales'.

## **ALTER SCHEMA**

Modifies the properties of an existing schema.

### **Syntax**

ALTER SCHEMA \<schema\_name\>  
    { RENAME TO \<new\_schema\_name\>  
    | OWNER TO \<new\_owner\>  
    | SET PATH \<new\_schema\_path\> };

### **Parameters**

* RENAME TO: Changes the name of the schema.  
* OWNER TO: Transfers ownership to a different user.  
* SET PATH: Modifies the schema's search path.

### **Examples**

**1\. Rename a schema:**

ALTER SCHEMA orders RENAME TO sales\_orders;

**2\. Change the owner of a schema:**

ALTER SCHEMA reporting OWNER TO 'bi\_team\_role';

## **DROP SCHEMA**

Removes a schema and all the objects it contains.

### **Syntax**

DROP SCHEMA \[ IF EXISTS \] \<schema\_name\> \[ CASCADE | RESTRICT \];

### **Parameters**

* IF EXISTS: Prevents an error if the schema does not exist.  
* CASCADE: Automatically drops all objects within the schema. This is a destructive operation.  
* RESTRICT: (Default) Prevents dropping the schema if it contains any objects.

### **Examples**

**1\. Drop an empty schema:**

\-- This will fail if 'temp' contains any objects.  
DROP SCHEMA temp;

**2\. Drop a schema and all its contents:**

\-- WARNING: This will delete all tables, views, etc., inside 'old\_project'.  
DROP SCHEMA IF EXISTS old\_project CASCADE;  
