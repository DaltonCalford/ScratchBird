# **Event System Specification**

## **1\. Introduction**

The ScratchBird Event System provides a mechanism for scheduling the execution of SQL code at a specific time, or on a recurring basis. This is a powerful tool for automating routine database maintenance, generating periodic reports, or running data processing tasks without the need for an external scheduler like cron.

Events are first-class database objects that are managed via DDL commands. Each event is associated with a block of SQL code that will be executed when the event's schedule is triggered.

## **2\. Creating Events**

The CREATE EVENT statement defines a new scheduled event.

### **CREATE EVENT**

**Syntax:**

CREATE EVENT \[ IF NOT EXISTS \] event\_name  
    ON SCHEDULE schedule\_definition  
    \[ ON COMPLETION \[ NOT \] PRESERVE \]  
    \[ ENABLE | DISABLE \]  
    \[ COMMENT 'comment\_string' \]  
    DO event\_body;

**Key Clauses:**

* **event\_name**: A unique name for the event.  
* **ON SCHEDULE**: Defines when the event should run. (See section 2.1)  
* **ON COMPLETION \[NOT\] PRESERVE**:  
  * NOT PRESERVE (Default): The event is dropped automatically after it executes once (for AT schedules).  
  * PRESERVE: The event definition is kept after it executes. This is the default and required for recurring (EVERY) events.  
* **ENABLE | DISABLE**:  
  * ENABLE (Default): The event is active and will run according to its schedule.  
  * DISABLE: The event is created but is inactive until explicitly enabled.  
* **DO event\_body**: The SQL statement or block of code to be executed. This can be a simple DML statement or a complex BEGIN...END block.

### **2.1. Schedule Definitions**

The ON SCHEDULE clause is the core of the event's definition.

#### **AT timestamp**

Executes the event once at a specific, absolute point in time.

**Example:**

\-- Run a year-end closing procedure at the end of 2025  
CREATE EVENT year\_end\_close  
    ON SCHEDULE AT '2025-12-31 23:59:00'  
    DO CALL run\_year\_end\_procedures();

#### **EVERY interval**

Executes the event repeatedly at a specified interval.

**Syntax:**

EVERY interval  
    \[ STARTS 'timestamp' \]  
    \[ ENDS 'timestamp' \]

**Example:**

\-- Truncate a log table every night at 1:00 AM  
CREATE EVENT cleanup\_logs  
    ON SCHEDULE EVERY 1 DAY  
    STARTS (CURRENT\_DATE \+ INTERVAL '1 day' \+ INTERVAL '1 hour') \-- Start tomorrow at 1 AM  
    DO TRUNCATE TABLE daily\_action\_logs;

\-- Generate a summary report every 15 minutes, starting now, for the next 8 hours  
CREATE EVENT generate\_hourly\_summaries  
    ON SCHEDULE EVERY 15 MINUTE  
    STARTS CURRENT\_TIMESTAMP  
    ENDS CURRENT\_TIMESTAMP \+ INTERVAL '8 hour'  
    ON COMPLETION PRESERVE  
    DO CALL update\_dashboard\_summary();

## **3\. Managing Events**

Existing events can be modified or removed.

### **ALTER EVENT**

Modifies the definition of an existing event. You can change its schedule, body, status, or other properties.

**Syntax:**

ALTER EVENT event\_name  
    \[ ON SCHEDULE schedule\_definition \]  
    \[ RENAME TO new\_event\_name \]  
    \[ ENABLE | DISABLE \]  
    \[ COMMENT 'new\_comment' \]  
    \[ DO event\_body \];

**Example:**

\-- Change the daily log cleanup to run at 2:30 AM instead of 1:00 AM  
ALTER EVENT cleanup\_logs  
    ON SCHEDULE EVERY 1 DAY  
    STARTS (CURRENT\_DATE \+ INTERVAL '1 day' \+ INTERVAL '2 hour' \+ INTERVAL '30 minute');

\-- Disable an event temporarily  
ALTER EVENT generate\_hourly\_summaries DISABLE;

### **DROP EVENT**

Removes an event from the database.

**Syntax:**

DROP EVENT \[ IF EXISTS \] event\_name;

## **4\. System Integration**

### **Event Scheduler Thread**

The ScratchBird server runs a dedicated event scheduler thread that is responsible for monitoring the event queue and executing events at their scheduled times. This thread is managed internally by the database.

### **System Views**

You can monitor the status of events through the information\_schema.

\-- View all defined events and their properties  
SELECT \* FROM information\_schema.events;

\-- Example Output:  
\-- | event\_name                | schedule\_body                                      | status  |  
\-- |---------------------------|----------------------------------------------------|---------|  
\-- | cleanup\_logs              | EVERY 1 DAY STARTS ...                             | ENABLED |  
\-- | generate\_hourly\_summaries | EVERY 15 MINUTE STARTS ... ENDS ...                | DISABLED|  
