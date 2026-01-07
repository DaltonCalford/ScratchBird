# **DDL Specification: Views**

## **Overview**

A view is a virtual table based on the result-set of an SQL statement. It contains rows and columns, just like a real table, but the data is generated dynamically when the view is queried. Views are used to simplify complex queries, provide a stable interface to changing table structures, and enforce security by exposing only certain columns or rows.

## **CREATE VIEW**

Defines a new view.

### **Syntax**

CREATE \[ OR REPLACE \] \[ TEMPORARY \] VIEW \<view\_name\>  
    \[ ( \<column\_name\_list\> ) \]  
    AS \<select\_statement\>  
    \[ WITH \[ CASCADED | LOCAL \] CHECK OPTION \];

### **Parameters**

* OR REPLACE: Replaces the view if it already exists.  
* TEMPORARY: The view exists only for the duration of the current session.  
* \<column\_name\_list\>: Explicitly names the columns of the view.  
* WITH CHECK OPTION: For updatable views, ensures that any INSERT or UPDATE statements satisfy the view's WHERE clause.

### **Examples**

**1\. Create a simple view to hide column complexity:**

CREATE VIEW customer\_details AS  
SELECT  
    customer\_id,  
    first\_name || ' ' || last\_name AS full\_name,  
    email,  
    city,  
    state  
FROM customers;

**2\. Create a view to restrict row access:**

CREATE VIEW my\_orders AS  
SELECT order\_id, order\_date, total\_amount  
FROM orders  
WHERE employee\_id \= CURRENT\_USER\_ID();

**3\. Create an updatable view with a check option:**

CREATE VIEW active\_products AS  
SELECT product\_id, product\_name, price  
FROM products  
WHERE is\_active \= TRUE  
WITH LOCAL CHECK OPTION;

\-- This INSERT would fail if is\_active was set to FALSE.  
\-- INSERT INTO active\_products (product\_name, price) VALUES ('New Gizmo', 99.99);

## **ALTER VIEW**

Modifies the definition or properties of an existing view.

### **Syntax**

ALTER VIEW \[ IF EXISTS \] \<view\_name\>  
    { RENAME TO \<new\_view\_name\>  
    | OWNER TO \<new\_owner\>  
    | SET SCHEMA \<new\_schema\>  
    | SET ( \<view\_option\> \= \<value\> \[ , ... \] ) }; \-- e.g., check\_option \= 'cascaded'

### **Examples**

**1\. Rename a view:**

ALTER VIEW customer\_info RENAME TO customer\_summary;

**2\. Change the owner of a view:**

ALTER VIEW sales\_report OWNER TO 'reporting\_role';

## **DROP VIEW**

Removes a view from the database.

### **Syntax**

DROP VIEW \[ IF EXISTS \] \<view\_name\> \[ , ... \] \[ CASCADE | RESTRICT \];

### **Parameters**

* IF EXISTS: Prevents an error if the view does not exist.  
* CASCADE: Automatically drops objects that depend on the view.  
* RESTRICT: (Default) Prevents dropping if other objects depend on it.

### **Examples**

**1\. Drop a single view:**

DROP VIEW old\_report\_view;

**2\. Safely drop multiple views:**

DROP VIEW IF EXISTS temp\_v1, temp\_v2;

## **Materialized Views**

A materialized view is a database object that contains the results of a query, physically stored on disk. Unlike a regular view, its data is pre-computed and can be indexed, making queries against it much faster.

### **CREATE MATERIALIZED VIEW**

CREATE MATERIALIZED VIEW \<view\_name\>  
AS \<select\_statement\>  
\[ WITH \[NO\] DATA \];

**Example:**

CREATE MATERIALIZED VIEW daily\_sales\_summary AS  
SELECT  
    order\_date,  
    COUNT(\*) as num\_orders,  
    SUM(total\_amount) as total\_sales  
FROM orders  
GROUP BY order\_date  
WITH DATA; \-- Populate immediately

### **REFRESH MATERIALIZED VIEW**

Updates the data in a materialized view to reflect the current state of the underlying tables.

REFRESH MATERIALIZED VIEW \[ CONCURRENTLY \] \<view\_name\>;

**Example:**

\-- Recalculates and replaces the data in the view  
REFRESH MATERIALIZED VIEW daily\_sales\_summary;  
