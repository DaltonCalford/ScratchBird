## **ScratchBird SQL Data Types**

This document lists the data types supported by ScratchBird, as defined by the provided parse.y grammar file. The types are grouped into logical categories.

### **Numeric Data Types**

#### **Exact Numerics (Integer)**

* **SMALLINT**: A 16-bit signed integer.  
* **INTEGER** or **INT**: A 32-bit signed integer.  
* **BIGINT**: A 64-bit signed integer.  
* **INT128**: A 128-bit signed integer.  
* **Unsigned Variants**: SMALLINT UNSIGNED, INTEGER UNSIGNED, BIGINT UNSIGNED, INT128 UNSIGNED are also supported.

#### **Exact Numerics (Decimal/Numeric)**

* **NUMERIC(p, s)**: A number with user-defined precision (p) and scale (s).  
* **DECIMAL(p, s)** or **DEC(p, s)**: Similar to NUMERIC, often with specific implementation details regarding precision.  
  * If precision is 1-4, it's a SMALLINT.  
  * If precision is 5-9, it's a LONG integer.  
  * If precision is 10-18, it's a 64-bit BIGINT.  
  * If precision is 19-38, it's a 128-bit INT128.

#### **Approximate Numerics (Floating-Point)**

* **FLOAT(p)**: A single-precision (if p \<= 24\) or double-precision (if p \> 24\) floating-point number.  
* **REAL**: A single-precision floating-point number (equivalent to FLOAT(24)).  
* **DOUBLE PRECISION**: A double-precision floating-point number.  
* **DECFLOAT(p)**: A decimal floating-point number. Precision (p) can be 16 (64-bit) or 34 (128-bit).

### **Date and Time Data Types**

* **DATE**: Stores a calendar date.  
* **TIME \[WITHOUT TIME ZONE\]**: Stores a time of day.  
* **TIME WITH TIME ZONE**: Stores a time of day with a time zone offset.  
* **TIMESTAMP \[WITHOUT TIME ZONE\]**: Stores both date and time.  
* **TIMESTAMP WITH TIME ZONE**: Stores date and time with a time zone offset.

### **Character String Data Types**

* **CHARACTER(n)** or **CHAR(n)**: A fixed-length character string.  
  * Can specify a CHARACTER SET and a COLLATE clause.  
* **CHARACTER VARYING(n)** or **VARCHAR(n)**: A variable-length character string.  
  * Can specify a CHARACTER SET and a COLLATE clause.  
* **NATIONAL CHARACTER(n)** or **NCHAR(n)**: A fixed-length character string using the national character set.  
* **NATIONAL CHARACTER VARYING(n)** or **NCHAR VARYING(n)**: A variable-length character string using the national character set.  
* **CITEXT**: A case-insensitive text type.

### **Binary String Data Types**

* **BINARY(n)**: A fixed-length binary string.  
* **BINARY VARYING(n)** or **VARBINARY(n)**: A variable-length binary string.  
* **BLOB**: A Binary Large Object for storing large amounts of unstructured binary data.  
  * **SEGMENT SIZE n**: Specifies the segment size.  
  * **SUB\_TYPE n | 'name'**: Specifies the blob subtype (e.g., 0 for binary, 1 for text).  
  * Can have a CHARACTER SET for text blobs.

### **Boolean Data Type**

* **BOOLEAN**: Stores truth values (TRUE, FALSE, or UNKNOWN).

### **Other Data Types**

* **UUID**: Stores a 128-bit Universally Unique Identifier.  
* **JSON**: Stores data in JavaScript Object Notation (JSON) format.  
* **Network Address Types**:  
  * **INET**: Stores an IPv4 or IPv6 address.  
  * **CIDR**: Stores an IPv4 or IPv6 network address in CIDR notation.  
  * **MACADDR**: Stores a MAC address.  
* **Range Types**:  
  * **INT4RANGE**: Range of 32-bit integers.  
  * **INT8RANGE**: Range of 64-bit integers.  
  * **NUMRANGE**: Range of NUMERIC values.  
  * **DATERANGE**: Range of DATE values.  
  * **TSRANGE**: Range of TIMESTAMP values.  
  * **TSTZRANGE**: Range of TIMESTAMP WITH TIME ZONE values.  
* **Full-Text Search Types**:  
  * **TSVECTOR**: For storing documents in a format optimized for text search.  
  * **TSQUERY**: For storing text search queries.

### **Arrays**

* Most non-BLOB data types can be defined as a multi-dimensional array.  
  * **Syntax**: \<type\> \[ \<dimension\_1\>, \<dimension\_2\>, ... \]  
  * **Dimension**: n or m:n (e.g., INTEGER\[10\], VARCHAR(20)\[-5:5, 10\])

### **Domain-Based Types**

* **\<domain\_name\>**: Uses a pre-defined DOMAIN as the data type.  
* **TYPE OF \<domain\_name\>**: Creates a column with the same base type as the domain, but without its constraints or default value.  
* **TYPE OF COLUMN \<table\>.\<column\>**: Creates a column with the same type as another existing column.