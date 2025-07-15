## **In-Depth Analysis of the REVOKE Statement**

The REVOKE statement is the counterpart to GRANT. It is used to remove permissions or role memberships from users, roles, and other database objects. Its syntax closely mirrors the GRANT statement.

### **1\. Revoking Privileges on Database Objects**

This form removes specific permissions (SELECT, EXECUTE, etc.) on an object from a grantee.

**General Syntax:**

REVOKE \[GRANT OPTION FOR\] {privilege\_list | ALL \[PRIVILEGES\]}  
ON \[OBJECT\_TYPE\] object\_name  
FROM grantee\_list  
\[GRANTED BY grantor\_name\]

#### **Clause Breakdown:**

* **REVOKE \[GRANT OPTION FOR\]**:  
  * If GRANT OPTION FOR is specified, the statement only revokes the grantee's ability to grant the privilege to others (WITH GRANT OPTION). It does not revoke the underlying privilege itself.  
  * If GRANT OPTION FOR is omitted, the statement revokes the actual privilege.  
* **{privilege\_list | ALL \[PRIVILEGES\]}**:  
  * Specifies which permissions are being revoked. This must match the privileges that were originally granted.  
  * privilege\_list: A comma-separated list like SELECT, DELETE.  
  * ALL \[PRIVILEGES\]: Revokes all applicable privileges for that object type.  
* **ON \[OBJECT\_TYPE\] object\_name**:  
  * Identifies the object from which privileges are being revoked.  
* **FROM grantee\_list**:  
  * Specifies who is losing the privileges. This is a comma-separated list of users, roles, procedures, etc.  
* **GRANTED BY grantor\_name**: (Optional)  
  * This clause is used by administrators to revoke a privilege that was granted by a specific user (grantor\_name).

### **2\. Revoking Roles**

This form removes a user's or another role's membership in a specific role.

**Syntax:**

REVOKE \[ADMIN OPTION FOR\] role\_name\_list  
FROM grantee\_list  
\[GRANTED BY grantor\_name\]

#### **Clause Breakdown:**

* **REVOKE \[ADMIN OPTION FOR\]**:  
  * If ADMIN OPTION FOR is specified, only the grantee's ability to grant the role to others is revoked. The grantee remains a member of the role.  
  * If omitted, the role membership itself is revoked.  
* **role\_name\_list**:  
  * A comma-separated list of one or more role names to be revoked.  
* **FROM grantee\_list**:  
  * Specifies the user or role that is losing membership in the role\_name\_list.

### **3\. Revoking DDL / System Privileges**

This form revokes permissions to perform DDL operations like CREATE TABLE or DROP PROCEDURE.

**Syntax:**

REVOKE \[GRANT OPTION FOR\] {ddl\_privilege\_list | ALL}  
ON {schema\_object | schemaless\_object} \[ON SCHEMA schema\_name\]  
FROM grantee\_list

The syntax directly mirrors the GRANT equivalent for DDL privileges, removing the CREATE, ALTER ANY, or DROP ANY permissions from the specified grantees.