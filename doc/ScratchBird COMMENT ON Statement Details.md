## **In-Depth Analysis of the COMMENT ON Statement**

The COMMENT ON statement is used to add, change, or remove a descriptive comment for a database object. These comments are stored in the system tables (specifically RDB$DESCRIPTION) and are an excellent way to document your database schema directly within the database itself. This metadata can be invaluable for developers and database administrators to understand the purpose of various objects.

**General Syntax:**

COMMENT ON object\_type object\_name IS {'comment\_text' | NULL}

* 'comment\_text': The string literal containing the description. The maximum length is typically limited by the system table's column size.  
* NULL: This keyword is used to remove an existing comment from the object.

### **Supported Object Types and Syntax**

The COMMENT ON statement can be applied to a wide variety of database objects. The syntax varies slightly depending on the object's scope and whether it's a sub-object (like a column or parameter).

| Statement Syntax | Description |
| :---- | :---- |
| COMMENT ON DATABASE IS ... | Comments on the entire database. |
| COMMENT ON TABLE table\_name IS ... | Comments on a table. |
| COMMENT ON VIEW view\_name IS ... | Comments on a view. |
| COMMENT ON COLUMN table\_name.column\_name IS ... | Comments on a specific table or view column. |
| COMMENT ON PROCEDURE procedure\_name IS ... | Comments on a stored procedure. |
| COMMENT ON FUNCTION function\_name IS ... | Comments on a user-defined function. |
| COMMENT ON TRIGGER trigger\_name IS ... | Comments on a trigger. |
| COMMENT ON PARAMETER procedure\_name.param\_name IS ... | Comments on a specific parameter of a procedure or function. |
| COMMENT ON DOMAIN domain\_name IS ... | Comments on a domain. |
| COMMENT ON EXCEPTION exception\_name IS ... | Comments on a user-defined exception. |
| COMMENT ON {SEQUENCE | GENERATOR} seq\_name IS ... |
| COMMENT ON INDEX index\_name IS ... | Comments on an index. |
| COMMENT ON ROLE role\_name IS ... | Comments on a role. |
| COMMENT ON SCHEMA schema\_name IS ... | Comments on a schema. |
| COMMENT ON PACKAGE package\_name IS ... | Comments on a package header. |
| COMMENT ON CHARACTER SET charset\_name IS ... | Comments on a character set. |
| COMMENT ON COLLATION collation\_name IS ... | Comments on a collation. |
| COMMENT ON FILTER filter\_name IS ... | Comments on a blob filter. |
| COMMENT ON MAPPING mapping\_name IS ... | Comments on a user mapping. |
| COMMENT ON USER user\_name IS ... | Comments on a user account. |

There is no ALTER COMMENT statement; to change an existing comment, you simply execute a new COMMENT ON statement for the same object with the new text.
