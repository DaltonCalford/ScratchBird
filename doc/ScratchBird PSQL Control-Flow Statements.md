## **In-Depth Analysis of PSQL Control-Flow Statements**

Procedural SQL (PSQL) in ScratchBird extends standard SQL with a set of statements for imperative logic, allowing for complex, stateful operations within procedures, functions, and triggers. Control-flow statements are the core of this capability, directing the flow of execution through conditional logic, loops, and branching.

### **1\. IF ... THEN ... ELSE (Conditional Execution)**

The IF statement evaluates a condition and executes a block of code if the condition is true. An optional ELSE clause can specify a block of code to execute if the condition is false.

**Syntax:**

IF (\<search\_condition\>) THEN  
BEGIN  
    \-- statement(s) to execute if condition is true  
END  
\[ELSE  
BEGIN  
    \-- statement(s) to execute if condition is false  
END\]

* **\<search\_condition\>**: Any boolean expression that evaluates to TRUE, FALSE, or UNKNOWN.  
* The BEGIN...END blocks are optional for a single statement but are highly recommended for clarity and are required for multiple statements.

### **2\. WHILE ... DO (Looping with a Pre-condition)**

The WHILE loop repeatedly executes a block of code as long as a specified condition remains true. The condition is checked *before* each iteration.

**Syntax:**

\[label:\]  
WHILE (\<search\_condition\>) DO  
BEGIN  
    \-- statement(s) to execute in the loop  
END

* **label:**: (Optional) A label that can be used with LEAVE to exit the loop.  
* The loop will not execute at all if the \<search\_condition\> is initially false.

### **3\. FOR SELECT ... DO (Cursor Loop)**

This is one of the most common and powerful constructs in PSQL. It iterates over the result set of a SELECT statement, executing a block of code for each row returned.

**Syntax:**

\[label:\]  
FOR  
    SELECT column\_list FROM ... WHERE ...  
    INTO :variable\_list  
    \[AS CURSOR cursor\_name\]  
DO  
BEGIN  
    \-- statement(s) to execute for each row  
END

* **INTO :variable\_list**: This is mandatory. For each row found by the SELECT, the column values are placed into the specified local PSQL variables.  
* **AS CURSOR cursor\_name**: (Optional) This gives the implicit cursor a name, which can be useful for positioned updates or deletes (WHERE CURRENT OF cursor\_name).

### **4\. FOR EXECUTE ... DO (Dynamic Cursor Loop)**

This statement allows you to iterate over the result set of a dynamically constructed SQL query string.

**Syntax:**

\[label:\]  
FOR  
    EXECUTE STATEMENT \<string\_expression\> \[(input\_params)\]  
    INTO :variable\_list  
DO  
BEGIN  
    \-- statement(s) to execute for each row  
END

* **\<string\_expression\>**: An expression that resolves to a string containing a SELECT statement.  
* This provides immense flexibility for building queries on the fly.

### **5\. LEAVE or BREAK (Exiting a Loop)**

The LEAVE statement (and its synonym BREAK) immediately terminates the execution of a WHILE or FOR loop.

**Syntax:**

LEAVE \[label\];  
BREAK; \-- synonym for LEAVE

* Execution continues at the first statement after the loop's END.  
* If loops are nested, LEAVE exits the innermost loop unless a label of an outer loop is specified.

### **6\. CONTINUE (Skipping to the Next Iteration)**

The CONTINUE statement skips the remaining statements in the current loop iteration and proceeds directly to the next iteration.

**Syntax:**

CONTINUE \[label\];

* In a WHILE loop, this jumps to the re-evaluation of the loop's condition.  
* In a FOR loop, this jumps to the fetching of the next row.

### **7\. EXIT (Terminating the Module)**

The EXIT statement immediately terminates the execution of the entire PSQL module (the procedure, trigger, or block). No further statements within the module are executed.

**Syntax:**

EXIT;

### **8\. RETURN (Exiting a Function)**

Used exclusively within a user-defined **function**, the RETURN statement exits the function and provides the value to be returned to the caller.

**Syntax:**

RETURN \<value\_expression\>;

* This also terminates the execution of the function.

### **9\. SUSPEND (Returning a Row)**

Used exclusively within a **selectable procedure** (one with a RETURNS clause), the SUSPEND statement "returns" a row to the caller and pauses the procedure's execution.

**Syntax:**

SUSPEND;

* When the caller requests the next row, execution resumes at the statement immediately following the SUSPEND. The procedure finally terminates when it reaches its END or an EXIT statement.