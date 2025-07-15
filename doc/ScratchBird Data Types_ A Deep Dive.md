## **In-Depth Analysis of ScratchBird Data Types**

This document provides a detailed look at the syntax, options, and usage of ScratchBird's data types.

### **1\. Numeric Types**

#### **Exact Numerics (Integers)**

* **SMALLINT**: 16-bit signed integer. Range: \-32,768 to 32,767.  
* **INTEGER** or **INT**: 32-bit signed integer. Range: \-2,147,483,648 to 2,147,483,647.  
* **BIGINT**: 64-bit signed integer.  
* **INT128**: 128-bit signed integer (available in ScratchBird 4.0+).  
* **USMALLINT**: 16-bit unsigned integer. Range: 0 to 65,535.  
* **UINTEGER**: 32-bit unsigned integer. Range: 0 to 4,294,967,295.  
* **UBIGINT**: 64-bit unsigned integer.

#### **Exact Numerics (Decimal)**

* **NUMERIC(p, s)** and **DECIMAL(p, s)**: Fixed-point numbers.  
  * **p (precision)**: The total number of digits (1 to 38).  
  * **s (scale)**: The number of digits to the right of the decimal point (0 to p).  
  * The underlying storage type (SMALLINT, INTEGER, BIGINT, INT128) is chosen by the engine based on the required precision.  
  * **Example**: DECIMAL(10, 2\) can store numbers like 12345678.99.

#### **Approximate Numerics (Floating-Point)**

* **FLOAT** or **REAL**: Single-precision 32-bit floating-point number.  
* **DOUBLE PRECISION**: Double-precision 64-bit floating-point number.  
* **DECFLOAT(p)**: Decimal floating-point type (ScratchBird 4.0+).  
  * **p (precision)**: Can be 16 (for 64-bit storage) or 34 (for 128-bit storage). Adheres to IEEE 754 standard for decimal arithmetic, avoiding binary floating-point rounding issues.

### **2\. Character and String Types**

* **CHAR(n)** or **CHARACTER(n)**: Fixed-length string.  
  * n: The exact number of characters. Strings shorter than n are padded with spaces. Max size depends on page size.  
* **VARCHAR(n)** or **CHARACTER VARYING(n)**: Variable-length string.  
  * n: The maximum number of characters.  
* **BLOB SUB\_TYPE TEXT**: For text of virtually unlimited length.  
* **Options for String Types**:  
  * **CHARACTER SET charset\_name**: Specifies the character encoding (e.g., UTF8, WIN1252). If omitted, the database or schema default is used.  
  * **COLLATE collation\_name**: Specifies the collation (sorting and comparison rules) for the string.

### **3\. Binary Types**

* **BINARY(n)**: Fixed-length binary data.  
* **VARBINARY(n)**: Variable-length binary data.  
* **BLOB \[SUB\_TYPE BINARY\]** or **BLOB SUB\_TYPE 0**: For binary data of virtually unlimited length.  
  * **SEGMENT SIZE n**: (Optional) A hint for storage allocation, typically left to the default.

### **4\. Date and Time Types**

* **DATE**: Stores only the date (year, month, day).  
* **TIME**: Stores only the time (hour, minute, second, fraction).  
* **TIMESTAMP**: Stores both date and time.  
* **Time Zone Variants**:  
  * **TIME WITH TIME ZONE**  
  * **TIMESTAMP WITH TIME ZONE**  
  * These types store the value along with a time zone offset. They are stored internally in UTC and converted to the session's time zone for display.

### **5\. Boolean Type**

* **BOOLEAN**: Stores truth values. Can be TRUE, FALSE, or UNKNOWN (NULL).

### **6\. Other Data Types**

* **UUID**: Stores a 128-bit Universally Unique Identifier.  
* **JSON**: Stores a JSON object or array as text, with validation (ScratchBird 4.0+).

### **7\. Arrays**

Most data types can be defined as multi-dimensional arrays.

* **Syntax**: \<data\_type\>\[d1\_start:d1\_end, d2\_start:d2\_end, ...\]  
* If only one number is provided, it is the upper bound, and the lower bound defaults to 1 (e.g., INTEGER\[10\] is an array of 10 integers indexed 1 to 10).  
* **Example**: VARCHAR(20)\[-5:5\] defines an array of 11 strings, indexed from \-5 to 5\.
