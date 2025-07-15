## **In-Depth Analysis of Collation DDL**

A collation is a schema object that defines the rules for sorting and comparing character strings. It specifies properties like case-sensitivity, accent-sensitivity, and character ordering for a specific character set. Custom collations allow for fine-grained control over string comparisons.

### **CREATE COLLATION**

This statement defines a new collation for a specific character set.

**Syntax:**

CREATE \[OR ALTER\] COLLATION \[IF NOT EXISTS\] collation\_name  
    FOR character\_set\_name  
    \[FROM {base\_collation\_name | EXTERNAL ('external\_name')}\]  
    \[attribute\_list\]  
    \['specific\_attributes'\]

#### **Clause Breakdown:**

* **collation\_name**: The name for the new collation.  
* **FOR character\_set\_name**: The mandatory character set to which this collation applies.  
* **FROM ...**: (Optional) Specifies a base for the new collation.  
  * FROM base\_collation\_name: Inherits properties from an existing collation.  
  * FROM EXTERNAL ('external\_name'): For collations based on an external library (e.g., ICU). 'external\_name' is the name recognized by the library.  
* **attribute\_list**: (Optional) A list of attributes that override the base collation's properties.  
  * PAD SPACE / NO PAD: Controls whether strings are padded with spaces for comparison.  
  * CASE SENSITIVE / CASE INSENSITIVE: Controls case sensitivity.  
  * ACCENT SENSITIVE / ACCENT INSENSITIVE: Controls accent sensitivity.  
* **'specific\_attributes'**: (Optional) A string containing provider-specific attributes, often used for ICU-based collations (e.g., 'locale=de@collation=phonebook').

### **ALTER COLLATION**

The parse.y file does not define a specific ALTER COLLATION statement. To modify a collation, you must use CREATE OR ALTER COLLATION or RECREATE COLLATION, which require a full redefinition.

### **DROP COLLATION**

Permanently deletes a collation. A collation cannot be dropped if it is currently in use by any table column, domain, or other database object.

**Syntax:**

DROP COLLATION \[IF EXISTS\] collation\_name;

* **IF EXISTS**: Prevents an error if the collation does not exist.

### **RECREATE COLLATION / CREATE OR ALTER COLLATION**

These are not explicitly defined as standalone commands in the grammar. The standard method is to use CREATE OR ALTER COLLATION, which will modify the collation if it exists or create it if it does not.