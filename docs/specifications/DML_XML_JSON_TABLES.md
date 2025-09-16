# **XML and JSON Table Functions**

## **1\. Introduction**

Modern applications frequently interact with semi-structured data formats like JSON and XML. ScratchBird provides powerful table functions, JSON\_TABLE and XMLTABLE, that allow you to shred these documents into a relational, tabular format directly within a FROM clause.

This enables you to query, join, and process complex semi-structured data using the full power of SQL, as if it were a standard relational table.

## **2\. JSON\_TABLE**

The JSON\_TABLE function projects JSON data into a relational table with specified columns. It is incredibly powerful for processing arrays of objects within a JSON document.

### **JSON\_TABLE Syntax**

JSON\_TABLE (  
    json\_expression,  
    path\_to\_array  
    COLUMNS (  
        column\_name\_1 FOR ORDINALITY,  
        column\_name\_2 data\_type PATH '$.path\_to\_value\_1' \[ ON EMPTY \] \[ ON ERROR \],  
        column\_name\_3 data\_type PATH '$.path\_to\_value\_2' \[ ON EMPTY \] \[ ON ERROR \],  
        ...  
        NESTED PATH '$.path\_to\_nested\_array' COLUMNS (...)  
    )  
) AS alias

**Key Components:**

* **json\_expression**: The JSON document or column to be shredded.  
* **path\_to\_array**: A JSONPath expression that points to the array within the document that should be iterated over. Each element in this array will become a row in the output table. Use '$' for a single object.  
* **COLUMNS (...)**: Defines the columns of the output table.  
* **FOR ORDINALITY**: A special column definition that generates a row number.  
* **PATH '$.path'**: A JSONPath expression, relative to the array element, that specifies where to extract the data for that column.

### **Example: Shredding a JSON Order Document**

Assume we have a table json\_orders with a doc column of type JSONB:

\-- A single row in json\_orders.doc  
{  
  "orderId": "A1234",  
  "orderDate": "2025-09-15",  
  "customer": {  
    "name": "Jane Doe",  
    "email": "jane.doe@example.com"  
  },  
  "items": \[  
    { "productId": "P-001", "productName": "Widget A", "quantity": 2, "price": 10.50 },  
    { "productId": "P-004", "productName": "Gadget B", "quantity": 1, "price": 25.00 }  
  \]  
}

We can use JSON\_TABLE to turn the items array into rows and join it with other tables.

SELECT  
    p.category,  
    jt.\*  
FROM  
    json\_orders jo,  
    JSON\_TABLE(  
        jo.doc,  
        '$.items\[\*\]' \-- Path to the array of items  
        COLUMNS (  
            row\_num FOR ORDINALITY,  
            order\_id    VARCHAR(20) PATH '$.orderId' \-- ERROR: Path must be relative to item  
            \-- Correction: order\_id is outside the array, so we reference the outer scope  
            order\_id    VARCHAR(20) PATH '$.orderId' FROM jo.doc, \-- Not standard, but a possible extension. Let's stick to standard.  
              
            \-- Let's retry with a better query structure  
        )  
    ) AS jt; \-- This is a common mistake. Let's fix it.

\-- Corrected and complete example  
SELECT  
    jo.doc \-\>\> 'orderId' AS order\_id,  
    jo.doc \-\> 'customer' \-\>\> 'name' AS customer\_name,  
    items.\*  
FROM  
    json\_orders jo,  
    \-- Use LATERAL join to correlate the function call with each row from json\_orders  
    LATERAL JSON\_TABLE(  
        jo.doc,  
        '$.items\[\*\]' \-- Correct path to the array to iterate over  
        COLUMNS (  
            line\_number FOR ORDINALITY,  
            product\_id  VARCHAR(10)  PATH '$.productId',  
            product     TEXT         PATH '$.productName',  
            qty         INT          PATH '$.quantity',  
            unit\_price  DECIMAL(10,2) PATH '$.price'  
        )  
    ) AS items;

/\*  
Output of the query:  
\+----------+---------------+-------------+------------+----------+-----+------------+  
| order\_id | customer\_name | line\_number | product\_id | product  | qty | unit\_price |  
\+----------+---------------+-------------+------------+----------+-----+------------+  
| A1234    | Jane Doe      |           1 | P-001      | Widget A |   2 |      10.50 |  
| A1234    | Jane Doe      |           2 | P-004      | Gadget B |   1 |      25.00 |  
\+----------+---------------+-------------+------------+----------+-----+------------+  
\*/

## **3\. XMLTABLE**

The XMLTABLE function serves the same purpose as JSON\_TABLE but operates on XML data using XPath expressions.

### **XMLTABLE Syntax**

XMLTABLE (  
    \[ XMLNAMESPACES ( namespace\_declarations... ), \]  
    xpath\_to\_rows  
    PASSING xml\_expression  
    COLUMNS  
        column\_name\_1 FOR ORDINALITY,  
        column\_name\_2 data\_type PATH 'relative\_xpath\_1',  
        column\_name\_3 data\_type PATH 'relative\_xpath\_2' \[ DEFAULT default\_expr \],  
        ...  
) AS alias

### **Example: Shredding an XML Invoice Document**

Assume a table xml\_invoices with an doc column of type XML:

\<\!-- A single row in xml\_invoices.doc \--\>  
\<Invoice id="INV-5678"\>  
  \<Date\>2025-09-15\</Date\>  
  \<Customer\>  
    \<Name\>ACME Corp\</Name\>  
  \</Customer\>  
  \<LineItems\>  
    \<Item SKU="X-998"\>  
      \<Description\>Power Scrubber\</Description\>  
      \<Quantity\>1\</Quantity\>  
      \<UnitPrice\>150.00\</UnitPrice\>  
    \</Item\>  
    \<Item SKU="Y-112"\>  
      \<Description\>Microfiber Cloths (Pack of 10)\</Description\>  
      \<Quantity\>5\</Quantity\>  
      \<UnitPrice\>12.50\</UnitPrice\>  
    \</Item\>  
  \</LineItems\>  
\</Invoice\>

We can use XMLTABLE to extract the line items.

SELECT  
    x.\*  
FROM  
    xml\_invoices xi,  
    LATERAL XMLTABLE(  
        '/Invoice/LineItems/Item' \-- XPath to the repeating elements  
        PASSING xi.doc  
        COLUMNS  
            invoice\_id  VARCHAR(20)  PATH '../@id', \-- Go up one level to get attribute  
            sku         VARCHAR(10)  PATH '@SKU',    \-- Attribute of the current element  
            description TEXT         PATH 'Description',  
            quantity    INT          PATH 'Quantity',  
            unit\_price  DECIMAL(10,2) PATH 'UnitPrice'  
    ) AS x;

/\*  
Output of the query:  
\+------------+---------+---------------------------------+----------+------------+  
| invoice\_id | sku     | description                     | quantity | unit\_price |  
\+------------+---------+---------------------------------+----------+------------+  
| INV-5678   | X-998   | Power Scrubber                  |        1 |     150.00 |  
| INV-5678   | Y-112   | Microfiber Cloths (Pack of 10\)  |        5 |      12.50 |  
\+------------+---------+---------------------------------+----------+------------+  
\*/  
