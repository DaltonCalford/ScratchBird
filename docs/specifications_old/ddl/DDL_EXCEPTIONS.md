# **DDL Specification: Exceptions**

## **Overview**

ScratchBird allows developers to define their own custom, named exceptions for use within the PSQL procedural language. Declaring a formal exception object provides clearer, more maintainable error handling than relying solely on SQLSTATE codes or generic error messages. These user-defined exceptions can be raised explicitly and caught in TRY/EXCEPT or EXCEPTION blocks.

## **CREATE EXCEPTION**

Defines a new, named exception.

### **Syntax**

CREATE EXCEPTION \<exception\_name\>  
    \[ ( \<parameter\_name\> \<data\_type\> \[ , ... \] ) \]  
    \[ WITH MESSAGE TEMPLATE \<string\_literal\> \]  
    \[ WITH MESSAGE BUILDER \<function\_name\> \]  
    \[ WITH OPTIONS (  
        SEVERITY \= '\<level\>',  
        SQLSTATE \= '\<sqlstate\_code\>',  
        HINT \= '\<hint\_string\>',  
        DETAIL \= '\<detail\_string\>'  
    ) \];

### **Parameters**

* \<parameter\_list\>: A list of typed parameters that can be passed when the exception is raised, allowing for dynamic error messages.  
* WITH MESSAGE TEMPLATE: A format string for the error message. Use {parameter\_name} as a placeholder.  
* WITH MESSAGE BUILDER: The name of a function that takes a RECORD of the exception parameters and returns the formatted error message text.  
* SEVERITY: The error level (e.g., ERROR, WARNING, FATAL).  
* SQLSTATE: Associates a custom five-character SQLSTATE code with the exception.

### **Examples**

**1\. Create a simple exception:**

CREATE EXCEPTION insufficient\_inventory;

**Usage:** RAISE insufficient\_inventory;

**2\. Create an exception with a templated message:**

CREATE EXCEPTION payment\_failed (  
    amount MONEY,  
    currency VARCHAR(3),  
    decline\_code VARCHAR(50)  
) WITH MESSAGE TEMPLATE  
    'Payment of {amount} {currency} failed. Decline code: {decline\_code}';

**Usage:** RAISE payment\_failed(amount := 99.99, currency := 'USD', decline\_code := '05');

**3\. Create an exception with a custom SQLSTATE and hint:**

CREATE EXCEPTION validation\_error  
WITH OPTIONS (  
    SQLSTATE \= 'P0001',  
    HINT \= 'Check the input data against business rules.'  
);

## **ALTER EXCEPTION**

ScratchBird does not support ALTER EXCEPTION. To modify an exception, you must DROP and CREATE it again. This is by design to ensure that the contract for an exception (its name, parameters, and codes) remains stable for existing code that may handle it.

To modify an exception, use CREATE OR REPLACE EXCEPTION if available, or a DROP/CREATE sequence within a transaction.

## **DROP EXCEPTION**

Removes a user-defined exception from the database.

### **Syntax**

DROP EXCEPTION \[ IF EXISTS \] \<exception\_name\>;

### **Parameters**

* IF EXISTS: Prevents an error if the exception does not exist.

### **Examples**

**1\. Drop an exception:**

DROP EXCEPTION insufficient\_inventory;

**2\. Safely drop an exception:**

DROP EXCEPTION IF EXISTS old\_validation\_rule;  
