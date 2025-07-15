## **In-Depth Analysis of EXECUTE PROCEDURE and CALL**

To execute a stored procedure in ScratchBird, you can use either the traditional, ScratchBird-specific EXECUTE PROCEDURE statement or the SQL-standard CALL statement. Both achieve the same goal but have slightly different syntax and capabilities.

### **1\. EXECUTE PROCEDURE**

This is the original ScratchBird syntax for calling a stored procedure. It is most commonly used within PSQL (BEGIN...END blocks) because of its ability to easily handle output parameters.

**Syntax:**

EXECUTE PROCEDURE procedure\_name  
    \[(input\_parameter\_list)\]  
    \[RETURNING\_VALUES (output\_variable\_list)\]

#### **Clause Breakdown:**

* **EXECUTE PROCEDURE procedure\_name**:  
  * procedure\_name: The name of the stored procedure to execute.  
* **(input\_parameter\_list)**: (Optional)  
  * A comma-separated list of values or expressions to be passed as input parameters to the procedure. The number and data types of the values must match the procedure's declared input parameters.  
  * The parentheses are optional if there are no input parameters.  
* **RETURNING\_VALUES (output\_variable\_list)**: (Optional)  
  * This clause is used to capture the output parameters from a "selectable" procedure.  
  * output\_variable\_list: A comma-separated list of PSQL variable names that will receive the values from the procedure's RETURNS clause. The number and data types must match the declared output parameters.

**Example (PSQL):**

EXECUTE PROCEDURE GET\_EMPLOYEE\_DETAILS(123)  
RETURNING\_VALUES (:EMP\_NAME, :EMP\_SALARY);

### **2\. CALL**

This is the SQL-standard syntax for executing a stored procedure, introduced in later versions of ScratchBird. It is more portable across different database systems.

**Syntax:**

CALL procedure\_name \[(parameter\_list)\]

#### **Clause Breakdown:**

* **CALL procedure\_name**:  
  * procedure\_name: The name of the stored procedure to execute.  
* **(parameter\_list)**: (Optional)  
  * A comma-separated list of parameters. Unlike EXECUTE PROCEDURE, the CALL statement does not syntactically distinguish between input and output parameters in its parameter list.  
  * For DSQL (dynamic SQL executed from an application), you would provide ? placeholders for both input and output parameters.  
  * The CALL statement does not have a RETURNING\_VALUES clause. Output values are retrieved through the parameter binding mechanism of the client API.

**Example (DSQL):**

\-- Assume a procedure P(IN1 INTEGER, OUT OUT1 VARCHAR(20))  
CALL P(?, ?);

### **Key Differences and Usage**

| Feature | EXECUTE PROCEDURE | CALL |
| :---- | :---- | :---- |
| **Standard** | ScratchBird-specific | SQL Standard |
| **Output Handling** | Uses RETURNING\_VALUES clause, ideal for PSQL. | Handles outputs via parameter binding, typical for client applications. |
| **Context** | Can be used anywhere, but most powerful in PSQL. | Primarily intended for DSQL from client applications. |
| **Selectability** | A procedure with SUSPEND is executed like a SELECT statement (SELECT \* FROM my\_proc(...)). | Not used for selectable procedures. |

