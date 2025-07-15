## **In-Depth Analysis of Domain DDL**

A domain in ScratchBird is a user-defined, reusable data type definition. It allows you to centralize the definition of a column's type, CHECK constraints, DEFAULT value, and COLLATE sequence. When you use a domain to define a table column, the column inherits all of these properties. This promotes consistency and simplifies database maintenance.

### **CREATE DOMAIN**

This statement defines a new domain.

**Syntax:**

CREATE \[OR ALTER\] DOMAIN \[IF NOT EXISTS\] domain\_name  
    AS \<data\_type\>  
    \[DEFAULT {literal | context\_variable | NULL}\]  
    \[NOT NULL\]  
    \[CHECK (condition)\]  
    \[COLLATE collation\_name\]

#### **Clause Breakdown:**

* **domain\_name**: The name of the new domain.  
* **AS \<data\_type\>**: The mandatory base data type for the domain (e.g., INTEGER, VARCHAR(100), DECIMAL(18, 4)).  
* **DEFAULT ...**: (Optional) Specifies a default value for columns that use this domain.  
* **NOT NULL**: (Optional) If present, any column defined with this domain will not allow NULL values.  
* **CHECK (condition)**: (Optional) A boolean expression that must evaluate to TRUE for any value being inserted or updated in a column using this domain. The keyword VALUE can be used within the condition to refer to the value being tested.  
* **COLLATE collation\_name**: (Optional) For character-based domains, this specifies the default collation sequence.

### **ALTER DOMAIN**

Modifies an existing domain. Changes made to a domain are **not** automatically propagated to existing table columns that use the domain.

**Syntax:**

ALTER DOMAIN domain\_name  
    { SET DEFAULT {literal | NULL} | DROP DEFAULT }  
  | { ADD \[CONSTRAINT\] CHECK (condition) | DROP CONSTRAINT }  
  | { SET NOT NULL | DROP NOT NULL }  
  | TO new\_domain\_name  
  | TYPE new\_data\_type

#### **Clause Breakdown:**

ALTER DOMAIN allows you to change one property at a time.

* **SET DEFAULT / DROP DEFAULT**: Adds, changes, or removes the domain's default value.  
* **ADD CHECK / DROP CONSTRAINT**: Adds or removes the domain's CHECK constraint. Note that a domain can only have one CHECK constraint.  
* **SET NOT NULL / DROP NOT NULL**: Adds or removes the NOT NULL property.  
* **TO new\_domain\_name**: Renames the domain.  
* **TYPE new\_data\_type**: Changes the underlying base data type of the domain. This is a potentially destructive operation and should be used with caution.

### **DROP DOMAIN**

Permanently deletes a domain. A domain cannot be dropped if it is currently in use by any table column, variable, or other database object.

**Syntax:**

DROP DOMAIN \[IF EXISTS\] domain\_name;

* **IF EXISTS**: Prevents an error if the domain does not exist.

### **RECREATE DOMAIN / CREATE OR ALTER DOMAIN**

These are not explicitly defined in the grammar as standalone commands for domains in the same way as for procedures or tables. The standard way to achieve this is to use CREATE OR ALTER DOMAIN, which will modify the domain if it exists or create it if it does not.