## **In-Depth Analysis of Built-in Functions and Expressions**

This document provides a detailed look at the various built-in functions and expressions supported by ScratchBird SQL. These are categorized for clarity.

### **1\. Conditional Expressions**

These expressions allow for conditional logic within a SQL statement, similar to if-then-else constructs in other languages.

#### **CASE**

The CASE expression is the SQL-standard way to handle conditional logic. It comes in two forms:

* **Simple CASE**: Compares an expression against a series of literal values.  
  CASE \<case\_operand\>  
      WHEN \<when\_value\_1\> THEN \<result\_1\>  
      WHEN \<when\_value\_2\> THEN \<result\_2\>  
      ...  
      \[ELSE \<else\_result\>\]  
  END

  **Example**: SELECT CASE status WHEN 'A' THEN 'Active' WHEN 'I' THEN 'Inactive' ELSE 'Unknown' END FROM T;  
* **Searched CASE**: Evaluates a series of independent boolean conditions.  
  CASE  
      WHEN \<condition\_1\> THEN \<result\_1\>  
      WHEN \<condition\_2\> THEN \<result\_2\>  
      ...  
      \[ELSE \<else\_result\>\]  
  END

  **Example**: SELECT CASE WHEN salary \> 100000 THEN 'High' WHEN salary \> 50000 THEN 'Medium' ELSE 'Low' END FROM T;

#### **IIF**

A non-standard, compact equivalent of a searched CASE statement.

* **Syntax**: IIF(\<condition\>, \<result\_if\_true\>, \<result\_if\_false\>)  
* **Example**: SELECT IIF(is\_active \= 1, 'Active', 'Inactive') FROM T;

#### **COALESCE**

Returns the first non-NULL expression in a list of arguments.

* **Syntax**: COALESCE(value\_1, value\_2, ...)  
* **Example**: SELECT COALESCE(nickname, firstname, 'N/A') FROM USERS;

#### **NULLIF**

Returns NULL if two expressions are equal; otherwise, it returns the first expression.

* **Syntax**: NULLIF(expression\_1, expression\_2)  
* **Example**: SELECT NULLIF(salary, 0\) FROM EMPLOYEES; (Returns NULL if salary is 0\)

#### **DECODE**

A non-standard function that compares an expression against a series of pairs (search, result) and returns the corresponding result.

* **Syntax**: DECODE(expression, search\_1, result\_1, search\_2, result\_2, ..., \[default\_result\])  
* **Example**: SELECT DECODE(status, 'A', 'Active', 'I', 'Inactive', 'Unknown') FROM T;

### **2\. String and Character Functions**

* **SUBSTRING(string FROM start \[FOR length\])**: Extracts a substring.  
* **SUBSTRING(string SIMILAR pattern ESCAPE escape\_char)**: Extracts a substring using SQL regular expression matching.  
* **UPPER(string)**: Converts a string to uppercase.  
* **LOWER(string)**: Converts a string to lowercase.  
* **TRIM(\[ {BOTH | LEADING | TRAILING} \[trim\_char\] FROM \] string)**: Removes leading, trailing, or both kinds of characters from a string.  
* **BTRIM(string \[, trim\_chars\])**: A non-standard trim function.  
* **LTRIM(string \[, trim\_chars\])**: A non-standard left-trim function.  
* **RTRIM(string \[, trim\_chars\])**: A non-standard right-trim function.  
* **CHAR\_LENGTH(string)** or **CHARACTER\_LENGTH(string)**: Returns the number of characters in a string.  
* **BIT\_LENGTH(string)**: Returns the number of bits in a string.  
* **OCTET\_LENGTH(string)**: Returns the number of bytes in a string.  
* **POSITION(substring IN string)**: Returns the starting position of a substring within another string.  
* **OVERLAY(string PLACING replacement FROM start \[FOR length\])**: Replaces a part of a string with another string.  
* **LPAD(string, length \[, pad\_char\])**: Left-pads a string to a certain length.  
* **RPAD(string, length \[, pad\_char\])**: Right-pads a string to a certain length.  
* **REPLACE(string, search\_for, replace\_with)**: Replaces all occurrences of a substring.  
* **REVERSE(string)**: Reverses the characters of a string.  
* **LEFT(string, length)**: Returns the leftmost length characters.  
* **RIGHT(string, length)**: Returns the rightmost length characters.  
* **ASCII\_CHAR(code)**: Returns the ASCII character for a given code.  
* **ASCII\_VAL(char)**: Returns the ASCII code for a given character.  
* **UNICODE\_CHAR(code)**: Returns the Unicode character for a given code point.  
* **UNICODE\_VAL(char)**: Returns the Unicode code point for a given character.

### **3\. Numeric and Mathematical Functions**

* **ABS(number)**: Returns the absolute value.  
* **ROUND(number, scale)**: Rounds a number to a specified number of decimal places.  
* **TRUNC(number, scale)**: Truncates a number to a specified number of decimal places.  
* **FLOOR(number)**: Returns the largest integer value less than or equal to the number.  
* **CEIL(number)** or **CEILING(number)**: Returns the smallest integer value greater than or equal to the number.  
* **MOD(dividend, divisor)**: Returns the remainder of a division.  
* **POWER(base, exponent)**: Raises a number to the power of another.  
* **SQRT(number)**: Returns the square root.  
* **SIGN(number)**: Returns \-1, 0, or 1 for negative, zero, or positive numbers, respectively.  
* **Trigonometric Functions**: SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2, SINH, COSH, TANH, ASINH, ACOSH, ATANH, COT.  
* **Logarithmic Functions**: LN (natural log), LOG (base-B log), LOG10 (base-10 log), EXP (e^number).  
* **PI()**: Returns the value of Pi.  
* **RAND()**: Returns a random floating-point number between 0 and 1\.  
* **Binary Functions**: BIN\_AND, BIN\_OR, BIN\_XOR, BIN\_NOT, BIN\_SHL (shift left), BIN\_SHR (shift right).

### **4\. Date and Time Functions**

* **EXTRACT(part FROM datetime\_value)**: Extracts a part from a date/time value. Parts include YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, MILLISECOND, WEEKDAY, YEARDAY, QUARTER, WEEK, etc.  
* **DATEADD(part, number, datetime\_value)**: Adds a specified number of date/time parts to a value.  
* **DATEDIFF(part, start\_datetime, end\_datetime)**: Returns the number of date/time parts between two values.  
* **CURRENT\_DATE**: Returns the current date of the server.  
* **CURRENT\_TIME**: Returns the current time with time zone.  
* **CURRENT\_TIMESTAMP**: Returns the current timestamp with time zone.  
* **LOCALTIME**: Returns the current time without time zone.  
* **LOCALTIMESTAMP**: Returns the current timestamp without time zone.  
* **FIRST\_DAY(OF {YEAR|QUARTER|MONTH|WEEK} FROM date)**: Returns the first day of the specified period.  
* **LAST\_DAY(OF {YEAR|QUARTER|MONTH|WEEK} FROM date)**: Returns the last day of the specified period.

### **5\. Type Casting and Conversion**

* **CAST(expression AS data\_type)**: Converts an expression from one data type to another.  
* **CAST(expression AS char\_type FORMAT format\_string)**: Converts a date/time/numeric type to a string with a specific format.  
* **CHAR\_TO\_UUID(hex\_string)**: Converts a 32-character hex string to a UUID.  
* **UUID\_TO\_CHAR(uuid\_value)**: Converts a UUID to its 32-character hex string representation.  
* **BASE64\_ENCODE(binary\_value) / BASE64\_DECODE(string)**: Encodes/decodes using Base64.  
* **HEX\_ENCODE(binary\_value) / HEX\_DECODE(string)**: Encodes/decodes using hexadecimal representation.

### **6\. System, Context, and Sequence Functions**

* **GEN\_ID(generator\_name, increment)** or **NEXT VALUE FOR sequence\_name**: Retrieves the next value from a sequence/generator.  
* **CURRENT\_USER**: Returns the name of the current user.  
* **CURRENT\_ROLE**: Returns the name of the current role.  
* **RDB$GET\_CONTEXT('namespace', 'variable')**: Retrieves a value from a context namespace.  
* **RDB$SET\_CONTEXT('namespace', 'variable', 'value')**: Sets a value in a context namespace.  
* **GEN\_UUID()**: Generates a new universally unique identifier (UUID).

### **7\. Aggregate and Window Functions**

#### **Standard Aggregates**

These functions operate on a group of rows and return a single summary value. They can be used with a GROUP BY clause.

* COUNT(\*), COUNT(\[DISTINCT\] expression)  
* SUM(\[DISTINCT\] expression)  
* AVG(\[DISTINCT\] expression)  
* MIN(expression)  
* MAX(expression)  
* LIST(\[DISTINCT\] expression \[, separator\]): Concatenates values into a single string.  
* Statistical Aggregates: STDDEV\_POP, STDDEV\_SAMP, VAR\_POP, VAR\_SAMP, COVAR\_POP, COVAR\_SAMP, CORR.

#### **Window Functions**

These functions operate on a "window" of rows related to the current row, without collapsing the result set. They are used with an OVER clause.

* **Ranking**: RANK(), DENSE\_RANK(), PERCENT\_RANK(), CUME\_DIST(), NTILE(num\_buckets).  
* **Row Numbering**: ROW\_NUMBER().  
* **Offset**: LAG(expr, offset, default), LEAD(expr, offset, default).  
* **Value**: FIRST\_VALUE(expr), LAST\_VALUE(expr), NTH\_VALUE(expr, n).  
* Standard aggregate functions can also be used as window functions (e.g., SUM(salary) OVER (PARTITION BY department)).

### **8\. JSON Functions**

* **JSON\_OBJECT(KEY key1 VALUE val1, ...)**: Creates a JSON object.  
* **JSON\_ARRAY(val1, val2, ...)**: Creates a JSON array.  
* **JSON\_EXTRACT(json\_doc, path)**: Extracts a value from a JSON document using a path expression.  
* **JSON\_MERGE(json\_doc1, json\_doc2, ...)**: Merges multiple JSON documents.  
* **JSON\_SET(json\_doc, path, value, ...)**: Inserts or updates values in a JSON document.  
* **JSON\_VALID(expression)**: Checks if an expression is valid JSON.

### **9\. Cryptographic and Hashing Functions**

* **HASH(expression \[USING algorithm\])**: Computes a hash of a value. The default algorithm is SHA-1.  
* **CRYPT\_HASH(expression USING algorithm)**: An alternative syntax for HASH.  
* **ENCRYPT(...) / DECRYPT(...)**: Performs symmetric encryption/decryption using various algorithms (AES, etc.), keys, modes, and initialization vectors (IV).  
* **RSA\_ENCRYPT(...) / RSA\_DECRYPT(...)**: Performs asymmetric encryption/decryption using RSA public/private keys.  
* **RSA\_SIGN\_HASH(...) / RSA\_VERIFY\_HASH(...)**: Creates and verifies digital signatures using RSA.
