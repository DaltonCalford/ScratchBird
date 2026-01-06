# **DDL Specification: Domains**

## **Overview**

A domain in ScratchBird is a user-defined data type with a name, an underlying primitive type, and a set of optional constraints, rules, and behaviors. Domains are a powerful feature for centralizing data validation and business logic. When a domain is used for a column or variable, it inherits all the properties of the domain, ensuring consistency and integrity throughout the database.

### **Storage and Encoding Notes (Alpha)**
- Domain values are stored using the **base type's canonical encoding** (see `DATA_TYPE_PERSISTENCE_AND_CASTS.md`). Domains do not introduce a new on-disk type tag.
- Domain constraints, normalization, masking, and encryption are enforced by the DomainManager at write/read time.
- Encrypted domain values store the same canonical plaintext payload inside the encrypted record.

## **CREATE DOMAIN**

Defines a new domain.

### **Syntax**

CREATE DOMAIN \<domain\_name\> AS \<base\_data\_type\>  
    \[ DEFAULT \<default\_value\> \]  
    \[ \[ CONSTRAINT \<constraint\_name\> \] CHECK ( \<expression\> ) \]  
    \[ COLLATE \<collation\_name\> \]  
    \[ WITH \<feature\> ( \<options\> ) \] \[ ... \];

### **Advanced WITH Clauses (ScratchBird Enhanced)**

* WITH SECURITY (...): Defines masking, auditing, permissions, and encryption.  
* WITH INTEGRITY (...): Defines global uniqueness and normalization rules.  
* WITH VALIDATION (...): Defines custom validation functions and error messages.  
* WITH QUALITY (...): Defines data parsing, standardization, and enrichment rules.  
* WITH OPTIONS (WRAP \= TRUE): For ENUM domains to enable cyclical arithmetic.

### **Examples**

**1\. Create a simple domain for a postal code:**

CREATE DOMAIN postal\_code AS VARCHAR(10)  
    CHECK (VALUE \~ '^\\d{5}(-\\d{4})?$');

**2\. Create a domain for positive monetary values:**

CREATE DOMAIN money\_positive AS DECIMAL(12, 2\)  
    DEFAULT 0.00  
    CHECK (VALUE \>= 0);

**3\. Create an advanced ENUM domain for order status:**

CREATE DOMAIN order\_state AS ENUM (  
    'Draft', 'Submitted', 'Processing', 'Shipped', 'Delivered', 'Cancelled'  
) WITH OPTIONS (WRAP \= FALSE);

**4\. Create a complex RECORD domain for addresses:**

CREATE DOMAIN mailing\_address AS RECORD (  
    street1 VARCHAR(100) NOT NULL,  
    street2 VARCHAR(100),  
    city VARCHAR(50) NOT NULL,  
    state CHAR(2),  
    postal\_code postal\_code \-- Using another domain  
);

**5\. Create a secure domain for sensitive information:**

CREATE DOMAIN ssn AS VARCHAR(11)  
    CHECK (VALUE \~ '^\\d{3}-\\d{2}-\\d{4}$')  
    WITH SECURITY (  
        MASK\_FUNCTION \= 'mask\_ssn\_function',  
        AUDIT\_ACCESS \= TRUE,  
        ENCRYPTION \= 'AES256'  
    );

## **ALTER DOMAIN**

Modifies the properties of an existing domain.

### **Syntax**

ALTER DOMAIN \<domain\_name\>  
    { RENAME TO \<new\_domain\_name\>  
    | OWNER TO \<new\_owner\>  
    | SET DEFAULT \<expression\>  
    | DROP DEFAULT  
    | ADD \[ CONSTRAINT \<constraint\_name\> \] CHECK ( \<expression\> )  
    | DROP CONSTRAINT \[ IF EXISTS \] \<constraint\_name\>  
    | RENAME CONSTRAINT \<constraint\_name\> TO \<new\_constraint\_name\> };

### **Examples**

**1\. Add a new constraint to a domain:**

ALTER DOMAIN money\_positive ADD CONSTRAINT positive\_check  
    CHECK (VALUE \> 0);

**2\. Remove a constraint from a domain:**

ALTER DOMAIN money\_positive DROP CONSTRAINT positive\_check;

**3\. Set a new default value:**

ALTER DOMAIN order\_state SET DEFAULT 'Draft';

**4\. Rename a domain:**

ALTER DOMAIN postal\_code RENAME TO zip\_code;

## **DROP DOMAIN**

Removes a domain from the database.

### **Syntax**

DROP DOMAIN \[ IF EXISTS \] \<domain\_name\> \[ , ... \] \[ CASCADE | RESTRICT \];

### **Parameters**

* IF EXISTS: Prevents an error if the domain does not exist.  
* CASCADE: Automatically drops any objects that depend on the domain (e.g., table columns, variables in routines).  
* RESTRICT: (Default) Prevents dropping the domain if any object depends on it.

### **Examples**

**1\. Drop a domain that is not in use:**

DROP DOMAIN old\_code\_format;

**2\. Safely drop a domain:**

DROP DOMAIN IF EXISTS temp\_domain;

**3\. Drop a domain and automatically alter columns that use it:**

\-- WARNING: This will convert any column using 'money\_positive' to its base  
\-- type (DECIMAL(12, 2)) and remove the domain constraints from it.  
DROP DOMAIN money\_positive CASCADE;  
