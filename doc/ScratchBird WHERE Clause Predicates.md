## **In-Depth Analysis of WHERE Clause Predicates**

The WHERE clause in a SQL statement filters rows by applying a set of conditions, known as predicates. A predicate is an expression that evaluates to TRUE, FALSE, or UNKNOWN. This document details the full range of predicates available in ScratchBird.

### **1\. Comparison Predicates**

These are the most basic predicates, used to compare two values.

* **Syntax**: expression operator expression  
* **Operators**:  
  * \= : Equal to  
  * \<\> or \!= : Not equal to  
  * \> : Greater than  
  * \>= : Greater than or equal to  
  * \< : Less than  
  * \<= : Less than or equal to  
* **Example**: WHERE SALARY \> 50000

### **2\. Range Predicate (BETWEEN)**

Checks if a value falls within a specified inclusive range.

* **Syntax**: expression \[NOT\] BETWEEN start\_expression AND end\_expression  
* **Behavior**: It is equivalent to expression \>= start\_expression AND expression \<= end\_expression.  
* **Example**: WHERE HIRE\_DATE BETWEEN '2023-01-01' AND '2023-12-31'

### **3\. Set Membership Predicate (IN)**

Checks if a value exists within a specified set of values or the result of a subquery.

* **Syntax**: expression \[NOT\] IN (value\_list | subquery)  
* **value\_list**: A comma-separated list of literal values (e.g., (1, 2, 3\) or ('A', 'B', 'C')).  
* **subquery**: A SELECT statement that must return a single column.  
* **Example (Value List)**: WHERE STATUS IN ('ACTIVE', 'PENDING')  
* **Example (Subquery)**: WHERE CUSTOMER\_ID IN (SELECT ID FROM INACTIVE\_CUSTOMERS)

### **4\. Pattern Matching Predicates**

These predicates are used for string matching.

#### **LIKE**

Performs string matching using standard SQL wildcards.

* **Syntax**: expression \[NOT\] LIKE pattern\_string \[ESCAPE escape\_character\]  
* **Wildcards**:  
  * \_ (underscore): Matches any single character.  
  * % (percent): Matches any sequence of zero or more characters.  
* **ESCAPE**: Defines a character to "escape" a wildcard, allowing you to search for the wildcard character itself (e.g., LIKE '50\!%' ESCAPE '\!' searches for strings starting with "50%").  
* **Example**: WHERE LAST\_NAME LIKE 'Sm\_th%'

#### **CONTAINING**

A ScratchBird-specific, non-standard predicate that checks for the presence of a substring anywhere within a string.

* **Syntax**: expression \[NOT\] CONTAINING substring  
* **Behavior**: It is roughly equivalent to LIKE '%' || substring || '%', but it can be optimized better by the engine using certain types of indices.  
* **Example**: WHERE NOTES CONTAINING 'urgent'

#### **STARTING WITH**

A ScratchBird-specific predicate that checks if a string begins with a specified substring.

* **Syntax**: expression \[NOT\] STARTING WITH substring  
* **Behavior**: It is equivalent to LIKE substring || '%'.  
* **Example**: WHERE SKU STARTING WITH 'FB-01-'

#### **SIMILAR TO**

Performs string matching using SQL standard regular expressions, which are more powerful than LIKE's wildcards.

* **Syntax**: expression \[NOT\] SIMILAR TO sql\_regex\_pattern \[ESCAPE escape\_character\]  
* **Behavior**: Supports a richer set of pattern matching operators, including character classes \[...\], alternations |, grouping (...), and quantifiers \*, \+, ?.  
* **Example**: WHERE PART\_CODE SIMILAR TO '(A|B)\_\[0-9\]{3}' (Matches codes like A\_123 or B\_987).

### **5\. NULL Predicates**

These predicates are used to specifically check for the presence or absence of a NULL value.

* **Syntax**: expression IS \[NOT\] {NULL | UNKNOWN}  
* **IS NULL**: Evaluates to TRUE if the expression's value is NULL.  
* **IS NOT NULL**: Evaluates to TRUE if the expression's value is not NULL.  
* **IS UNKNOWN**: For boolean values, this is equivalent to IS NULL.  
* **IS NOT UNKNOWN**: For boolean values, this is equivalent to IS NOT NULL.  
* **Note**: You cannot use standard comparison operators like \= NULL or \<\> NULL. These will always evaluate to UNKNOWN.  
* **Example**: WHERE MANAGER\_ID IS NULL

### **6\. Existence Predicate (EXISTS)**

Checks if a subquery returns one or more rows.

* **Syntax**: \[NOT\] EXISTS (subquery)  
* **Behavior**: It returns TRUE if the subquery produces at least one row, and FALSE otherwise. It does not matter what the subquery's columns or values are. Because of this, the SELECT list in the subquery is often written as SELECT 1 or SELECT \* for convention.  
* **Example**: SELECT C.NAME FROM CUSTOMERS C WHERE EXISTS (SELECT 1 FROM ORDERS O WHERE O.CUST\_ID \= C.ID) (Selects customers who have placed at least one order).

### **7\. Uniqueness and Distinction Predicates**

#### **SINGULAR**

A ScratchBird-specific predicate that checks if a subquery returns exactly one row.

* **Syntax**: \[NOT\] SINGULAR (subquery)  
* **Behavior**: Returns TRUE if the subquery produces exactly one row.  
* **Example**: WHERE SINGULAR (SELECT 1 FROM USER\_SETTINGS S WHERE S.USER\_ID \= U.ID)

#### **IS \[NOT\] DISTINCT FROM**

This predicate compares two expressions just like the \= and \<\> operators, but it treats NULL as a comparable value.

* **Syntax**: expression\_1 IS \[NOT\] DISTINCT FROM expression\_2  
* **Behavior**:  
  * IS DISTINCT FROM is like \<\>, but NULL IS DISTINCT FROM NULL is FALSE.  
  * IS NOT DISTINCT FROM is like \=, but NULL IS NOT DISTINCT FROM NULL is TRUE.  
* **Example**: WHERE OLD\_VALUE IS DISTINCT FROM NEW\_VALUE (This will be true if one is NULL and the other is not, unlike the \<\> operator).

### **8\. Quantified Predicates**

These predicates compare a value to a set of values returned by a subquery.

* **Syntax**: expression operator {ALL | ANY | SOME} (subquery)  
* **ALL**: The comparison must be true for **all** values returned by the subquery. \> ALL (...) means greater than the maximum value.  
* **ANY / SOME**: The comparison must be true for **at least one** value returned by the subquery. \> ANY (...) means greater than the minimum value. IN is equivalent to \= ANY (...).  
* **Example**: WHERE SALARY \> ALL (SELECT MIN\_WAGE FROM JOB\_GRADES)

### **9\. PSQL Context Predicates**

These are special boolean predicates available only within PSQL (triggers, procedures) to check the context of the current operation.

* **INSERTING**: TRUE if the trigger is firing due to an INSERT.  
* **UPDATING**: TRUE if the trigger is firing due to an UPDATE.  
* **DELETING**: TRUE if the trigger is firing due to a DELETE.  
* **RESETTING**: TRUE if the ON CONNECT trigger is firing because the session was just reset via ALTER SESSION RESET.  
* **Example (in a trigger)**: IF (UPDATING AND NEW.SALARY \> OLD.SALARY \* 1.5) THEN ...