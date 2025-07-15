## **In-Depth Analysis of Sequence/Generator DDL**

A sequence, also known by its older name, generator, is a database object that produces a sequence of unique numbers. They are most commonly used to generate primary key values.

### **CREATE SEQUENCE or CREATE GENERATOR**

This statement defines a new sequence. The keywords SEQUENCE and GENERATOR are interchangeable.

**Syntax:**

CREATE \[OR ALTER\] {SEQUENCE | GENERATOR} \[IF NOT EXISTS\] sequence\_name  
    \[START WITH initial\_value\]  
    \[INCREMENT \[BY\] increment\_value\]

#### **Clause Breakdown:**

* **sequence\_name**: The name of the new sequence.  
* **START WITH initial\_value**: (Optional) Sets the initial value of the sequence. If omitted, the sequence starts at 0\.  
* **INCREMENT \[BY\] increment\_value**: (Optional) Sets the step value by which the sequence will be incremented on each call. The default increment is 1\. The value can be positive or negative.

Usage:  
The value of a sequence is retrieved using NEXT VALUE FOR sequence\_name.

### **ALTER SEQUENCE or ALTER GENERATOR**

Modifies the properties of an existing sequence.

**Syntax:**

ALTER {SEQUENCE | GENERATOR} sequence\_name  
    \[RESTART\]  
    \[RESTART WITH new\_value\]  
    \[INCREMENT \[BY\] new\_increment\]

#### **Clause Breakdown:**

* **RESTART**: Resets the sequence's value to its original starting value (or 0 if none was specified).  
* **RESTART WITH new\_value**: Resets the sequence's value to a new, specified value. This is effectively the same as SET GENERATOR.  
* **INCREMENT \[BY\] new\_increment**: Changes the increment value for future calls to the sequence.

### **SET GENERATOR (Legacy)**

This is the older, still supported syntax for setting the current value of a sequence.

**Syntax:**

SET GENERATOR sequence\_name TO new\_value;

This is functionally equivalent to ALTER SEQUENCE sequence\_name RESTART WITH new\_value;.

### **DROP SEQUENCE or DROP GENERATOR**

Permanently deletes a sequence from the database.

**Syntax:**

DROP {SEQUENCE | GENERATOR} \[IF EXISTS\] sequence\_name;

* **IF EXISTS**: Prevents an error if the sequence does not exist.

### **RECREATE SEQUENCE / CREATE OR ALTER SEQUENCE**

These commands provide convenient ways to manage sequences in deployment scripts.

* **RECREATE SEQUENCE**: Atomically drops the sequence if it exists and then creates it with the new definition.  
* **CREATE OR ALTER SEQUENCE**: Modifies the sequence if it exists, or creates it if it does not.

**Syntax:**

RECREATE {SEQUENCE | GENERATOR} sequence\_name ...  
CREATE OR ALTER {SEQUENCE | GENERATOR} sequence\_name ...

The rest of the syntax is identical to their CREATE counterparts.