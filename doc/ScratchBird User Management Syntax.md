## **In-Depth Analysis of User Management Syntax**

User management in ScratchBird is handled through a set of SQL statements that allow for the creation, modification, and deletion of user accounts. These statements control user identity, credentials, and other properties.

### **CREATE USER**

This statement creates a new user account in the security database.

**Syntax:**

\* \*\*\`ACTIVE\` / \`INACTIVE\`\*\*:  
\-   \* \`ACTIVE\`: The user account is enabled and can be used to connect (default).  
\-   \* \`INACTIVE\`: The user account is disabled.  
\+   \* \`ACTIVE\`: The user account is enabled and can be used to connect (default).  
\+   \* \`INACTIVE\`: The user account is disabled.  
\+\* \*\*\`HOME SCHEMA 'schema\_name'\`\*\*: Sets the default schema for the user upon connection.  
\* \*\*\`USING PLUGIN plugin\_name\`\*\*: Specifies the authentication plugin...


CREATE \[OR ALTER\] USER \[IF NOT EXISTS\] user\_name  
    \[user\_option \[user\_option ...\]\];

* **OR ALTER**: If a user with user\_name already exists, the statement will modify the existing user instead of failing.  
* **IF NOT EXISTS**: (Used with CREATE only) Prevents an error if the user already exists; the statement will do nothing.  
* **user\_name**: The name of the new user.

#### **User Options (user\_option):**

This is a list of one or more clauses that define the user's properties.

* **PASSWORD 'password'**: Sets the user's password.  
* **FIRSTNAME 'firstname'**: Sets the user's first name.  
* **MIDDLENAME 'middlename'**: Sets the user's middle name.  
* **LASTNAME 'lastname'**: Sets the user's last name.  

* **ACTIVE / INACTIVE**:  
  * ACTIVE: The user account is enabled and can be used to connect (default).  
  * INACTIVE: The user account is disabled.  
  * ACTIVE: The user account is enabled and can be used to connect (default).  
  * INACTIVE: The user account is disabled.  
  * HOME SCHEMA 'schema name': Sets the default schema for the user upon connection.  
  * **USING PLUGIN plugin\_name**: Specifies the authentication plugin to be used for this user (e.g., Srp, Legacy\_Auth).  
  * **TAGS ( tag\_list )**: A flexible key-value store for associating custom metadata with the user account, often used by authentication plugins.  
  * tag\_list: A comma-separated list of key \= 'value' or DROP key.

### **ALTER USER**

This statement modifies the properties of an existing user account.

**Syntax:**

ALTER USER user\_name  
    \[SET\]  
    \[user\_option \[user\_option ...\]\];

or for the current user:

ALTER CURRENT USER  
    \[SET\]  
    \[user\_option \[user\_option ...\]\];

* **user\_name**: The name of the user to modify.  
* **CURRENT USER**: A special form to modify the currently connected user.  
* **SET**: An optional keyword for clarity.  
* The user\_option clauses are the same as for CREATE USER. You can change the password, names, active status, plugin, and tags.

### **RECREATE USER**

This statement atomically drops and recreates a user. If the user does not exist, it is simply created. This is useful for ensuring a user account is in a known state in deployment scripts.

**Syntax:**

RECREATE USER user\_name  
    \[user\_option \[user\_option ...\]\];

The options are the same as for CREATE USER.

### **DROP USER**

This statement permanently deletes a user account from the security database.

**Syntax:**

DROP USER \[IF EXISTS\] user\_name \[USING PLUGIN plugin\_name\];

* **IF EXISTS**: Prevents an error if the user does not exist.  
* **USING PLUGIN plugin\_name**: This is required if the user was created with a specific authentication plugin and the server configuration requires it for dropping users.

### **COMMENT ON USER**

This statement adds or changes the descriptive comment for a user account.

**Syntax:**

COMMENT ON USER user\_name \[USING PLUGIN plugin\_name\] IS {'comment' | NULL};

* 'comment': The text to associate with the user.  
* NULL: Removes the existing comment.
