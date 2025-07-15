## **In-Depth Analysis of Miscellaneous DDL Statements**

This document covers several less common but important DDL statements for managing database-level features like shadowing, blob filters, and other database-wide settings.

### **ALTER DATABASE**

This statement modifies global properties of the current database.

**Syntax:**

ALTER DATABASE  
    alter\_db\_option \[alter\_db\_option ...\]

#### **Common Options (alter\_db\_option):**

* **SET DEFAULT CHARACTER SET charset\_name**: Changes the default character set for the entire database. This does not affect existing columns but applies to new columns created without an explicit character set.  
* **BEGIN BACKUP / END BACKUP**: These commands are part of ScratchBird's nBackup utility. BEGIN BACKUP puts the database into a state where a safe physical backup can be made while the database is still online. END BACKUP returns it to normal operation.  
* **ADD | DROP DIFFERENCE FILE 'path'**: Manages the "delta" file for nBackup, which stores changes made during the backup process.  
* **ENCRYPT WITH plugin\_name \[KEY key\_name\] / DECRYPT**: Manages database-level encryption (ScratchBird 4.0+). It applies or removes encryption using a specified plugin and key.  
* **SET LINGER TO seconds / DROP LINGER**: Configures how long a database connection will "linger" (remain open but inactive) after the client disconnects, which can speed up subsequent connections. DROP LINGER disables the feature.

### **CREATE SHADOW / DROP SHADOW**

A shadow is a complete, real-time physical copy of the database file. The engine automatically writes every change to both the primary database file and all of its shadows. If the primary database file is lost or corrupted, a shadow can be activated to become the new primary, ensuring high availability with minimal data loss.

#### **CREATE SHADOW**

This statement creates and activates a new shadow file for the current database.

* **Syntax**: CREATE SHADOW shadow\_number 'path\_to\_shadow\_file' \[conditional\_auto\_manual\_options\]  
* **shadow\_number**: A unique integer identifier for the shadow.  
* **'path\_to\_shadow\_file'**: The full path and filename for the new shadow file.  
* **Options**:  
  * AUTO/MANUAL: Controls whether the shadow file grows automatically or must be manually extended.  
  * CONDITIONAL: The shadow is only created if the specified file does not already exist.

#### **DROP SHADOW**

This statement deactivates and disassociates a shadow file from the database.

* **Syntax**: DROP SHADOW shadow\_number \[PRESERVE FILE | DELETE FILE\]  
* **PRESERVE FILE**: (Default) The shadow file is left on the disk after being dropped.  
* **DELETE FILE**: The shadow file is deleted from the disk.

### **CREATE FILTER / DROP FILTER**

A blob filter is a user-defined function (from an external library) that can be used to transform BLOB data as it is read from or written to the database. For example, a filter could be used to compress/decompress data on the fly. This is a legacy feature, and modern applications often handle such transformations in the application layer.

#### **CREATE FILTER**

Declares a new blob filter.

* **Syntax**: CREATE FILTER filter\_name INPUT\_TYPE subtype OUTPUT\_TYPE subtype ENTRY\_POINT 'function\_name' MODULE\_NAME 'library\_name';

#### **DROP FILTER**

Removes a blob filter declaration.

* **Syntax**: DROP FILTER filter\_name;

### **DROP DATABASE**

The DROP DATABASE command is **not** a standard DSQL statement that can be executed through a normal connection in ScratchBird. It cannot be used in PSQL or sent via a client API like a SELECT or INSERT.

Instead, dropping a database is an administrative action performed through:

1. The isql command-line utility: DROP DATABASE 'path/to/database.fdb';  
2. The ScratchBird Services API, which is used by administrative tools.

This distinction is important because it prevents a user with regular database access from accidentally or maliciously dropping the entire database with a single SQL command.