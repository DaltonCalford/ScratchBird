## **Updates for Existing Documentation**

This document contains the necessary modifications for previously generated documents to align them with the new features found in the updated parse.y file.

### **Document: Data Types: A Deep Dive (firebird\_data\_types\_deep\_dive)**

The grammar now includes explicit tokens for unsigned integer types.

**In section: 1\. Numeric Types \-\> Exact Numerics (Integers)**

\* \*\*\`BIGINT\`\*\*: 64-bit signed integer.  
\* \*\*\`INT128\`\*\*: 128-bit signed integer (available in ScratchBird 4.0+).  
\-\* \*\*Unsigned variants\*\*: \`SMALLINT UNSIGNED\`, \`INTEGER UNSIGNED\`, \`BIGINT UNSIGNED\`, \`INT128 UNSIGNED\` are also supported.  
\+\* \*\*\`USMALLINT\`\*\*: 16-bit unsigned integer. Range: 0 to 65,535.  
\+\* \*\*\`UINTEGER\`\*\*: 32-bit unsigned integer. Range: 0 to 4,294,967,295.  
\+\* \*\*\`UBIGINT\`\*\*: 64-bit unsigned integer.

### **Document: COMMENT ON Statement Details (firebird\_comment\_on\_details)**

The COMMENT ON statement now supports SCHEMA objects.

**In section: Supported Object Types and Syntax \-\> Table**

| \`COMMENT ON ROLE role\_name IS ...\` | Comments on a role. |  
\+| \`COMMENT ON SCHEMA schema\_name IS ...\` | Comments on a schema. |  
| \`COMMENT ON PACKAGE package\_name IS ...\` | Comments on a package header. |

### **Updates for DML Document Syntax**

The following documents need to be updated to include the AT database\_link\_name syntax for referencing tables.

**Document: ScratchBird SELECT Statement In-Depth (firebird\_select\_details\_1)**

**In section: The SELECT Block: query\_spec \-\> FROM from\_list \-\> Table Reference**

\* \*\*Table Reference\*\*:  
\-   \* \`table\_name \[AS\] alias\`  
\+   \* \`table\_name \[AS\] alias \[AT database\_link\_name\]\`  
    \* \*\*Derived Table\*\*: \`(select\_expression) \[AS\] alias \[(column\_aliases)\]\`

**Document: ScratchBird INSERT Statement In-Depth (**firebird\_insert\_details**)**

**In section:** 1\. INSERT ... VALUES (Single Row) **\-\>** Clause Breakdown

\* \*\*\`INSERT INTO table\_name \[(column\_list)\]\`\*\*:  
\-   \* \`table\_name\`: The mandatory name of the table to insert into.  
\+   \* \`table\_name\`: The mandatory name of the table to insert into. Can be specified as \`table\_name AT link\_name\` to insert into a remote table.  
    \* \`(column\_list)\`: (Optional) A comma-separated list of columns for which you are providing values.

**Document: ScratchBird UPDATE Statement In-Depth (**firebird\_update\_details\_1**)**

**In section:** 1\. Searched UPDATE **\-\>** Clause Breakdown

\* \*\*\`UPDATE table\_name \[AS\] alias\`\*\*:  
\-   \* \`table\_name\`: The mandatory name of the table to be updated.  
\+   \* \`table\_name\`: The mandatory name of the table to be updated. Can be specified as \`table\_name AT link\_name\` to update a remote table.  
    \* \`alias\`: (Optional) A temporary alias for the table name...

**Document: ScratchBird DELETE Statement In-Depth (**firebird\_delete\_details\_1**)**

**In section:** 1\. Searched DELETE **\-\>** Clause Breakdown

\* \*\*\`DELETE FROM table\_name \[AS\] alias\`\*\*:  
\-   \* \`table\_name\`: The mandatory name of the table from which to delete rows.  
\+   \* \`table\_name\`: The mandatory name of the table from which to delete rows. Can be specified as \`table\_name AT link\_name\` to delete from a remote table.  
    \* \`alias\`: (Optional) A temporary alias for the table name.

**Document: ScratchBird MERGE Statement In-Depth (**firebird\_merge\_details**)**

**In section:** Main Clause Breakdown

\* \*\*\`MERGE INTO target\_table\`\*\*:   
\- The table to be modified.  
\+ The table to be modified. Can be specified as \`target\_table AT link\_name\` to merge into a remote table.  
\* \*\*\`USING source\_table\`\*\*: The data source containing the new or updated data.

**Document: ScratchBird UPDATE OR INSERT Statement In-Depth (**firebird\_update\_insert\_details**)**

**In section:** Clause Breakdown

\* \*\*\`UPDATE OR INSERT INTO table\_name \[(insert\_column\_list)\]\`\*\*:  
\-   \* \`table\_name\`: The mandatory name of the table to be modified.  
\+   \* \`table\_name\`: The mandatory name of the table to be modified. Can be specified as \`table\_name AT link\_name\` to operate on a remote table.  
    \* \`(insert\_column\_list)\`: (Optional) A comma-separated list of columns.  
