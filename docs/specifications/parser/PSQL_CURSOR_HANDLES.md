# **PSQL Cursor Handle Passing**

## **1. Purpose**

This specification defines how cursor handles are passed into and out of
procedures and functions without copying row data. A cursor handle is a
session-local reference to a live cursor state (result set, position, and
snapshot). Passing the handle allows multiple routines to operate on the
same cursor instance.

Status: Planned.

## **2. Definitions**

- **Cursor handle**: A lightweight token referencing a cursor state owned by
  the current session. It is not a copied rowset.
- **Typed cursor handle**: A cursor handle that carries a declared row shape
  (`CURSOR OF <rowtype>`), enabling static type checks for FETCH targets.
- **Owner**: The routine or statement that created a cursor handle. Ownership
  controls who may close the cursor by default.

## **3. Syntax**

### **3.1. Parameter and return types**

```
CREATE PROCEDURE proc_name(
    IN      cur CURSOR,
    IN OUT  cur2 CURSOR OF some_table%ROWTYPE,
    OUT     cur3 CURSOR
)
AS BEGIN
    ...
END;
```

```
CREATE FUNCTION fn_name(...)
RETURNS CURSOR [OF <rowtype>]
AS BEGIN
    ...
END;
```

Notes:
- `RETURNS CURSOR` is allowed only for PSQL invocation contexts (EXECUTE,
  CALL). It is not valid inside SQL expressions or SELECT lists.
- For SQL integration, continue to use `RETURNS SETOF` or `RETURNS TABLE`.

### **3.2. Cursor handle variables**

Cursor handles can be stored in variables:

```
DECLARE
    @c CURSOR;
    @c2 CURSOR OF orders%ROWTYPE;
BEGIN
    ...
END;
```

`DECLARE <name> CURSOR FOR <query>` creates a cursor and assigns the handle
to `<name>`. It is equivalent to allocating a new handle and binding it to
the query source.

## **4. Semantics**

### **4.1. Pass-by-reference behavior**

All cursor handle parameters are reference semantics:
- `IN`: the callee may FETCH and move position, but must not CLOSE.
- `IN OUT`: the callee may FETCH and CLOSE; position changes are visible
  to the caller.
- `OUT`: the callee must create and assign a new cursor handle.

### **4.2. Lifetime and scope**

- Cursor handles are valid only within the same session and transaction.
- Handles become invalid when:
  - the cursor is explicitly CLOSED,
  - the owning transaction COMMITs or ROLLBACKs,
  - the session ends.
- A handle may not be stored in tables, serialized, or passed to other
  sessions.

### **4.3. Snapshot and visibility**

- The cursor snapshot is established at OPEN time.
- Subsequent DML in the same transaction does not change the cursor's
  current result set unless the cursor is re-opened.

### **4.4. Security**

- Permission checks occur at cursor creation (OPEN).
- For SECURITY DEFINER routines, cursor creation uses definer privileges.
- FETCH does not re-check privileges unless the cursor is re-opened.

## **5. Trigger constraints**

- Triggers may declare and use cursor handles inside the trigger body.
- Triggers cannot accept cursor parameters or return cursor handles.
- Any cursor handle created in a trigger must be closed before the trigger
  completes, or it will be closed implicitly at trigger end.

## **6. SBLR Mapping (Planned)**

- Introduce a value type tag `CURSOR_HANDLE` in TypedValue payloads.
- Handle payload holds a session-local cursor id (64-bit integer).
- Cursor opcodes operate on the handle id and resolve to cursor state
  from the session cursor registry.
- Parameter passing and variable assignment carry the handle id unchanged
  (no row data copy).

## **7. Errors**

- `CURSOR_INVALID`: handle is NULL or not found.
- `CURSOR_CLOSED`: handle refers to a closed cursor.
- `CURSOR_TYPE_MISMATCH`: typed handle does not match FETCH target shape.
- `CURSOR_OWNERSHIP`: disallowed CLOSE on `IN` parameter.

## **8. Examples**

### **8.1. Returning a cursor handle**

```
CREATE FUNCTION get_recent_orders(p_customer UUID)
RETURNS CURSOR OF orders%ROWTYPE
AS
DECLARE
    @c CURSOR OF orders%ROWTYPE;
BEGIN
    DECLARE @c CURSOR FOR
        SELECT * FROM orders
        WHERE customer_id = p_customer
        ORDER BY created_at DESC;
    OPEN @c;
    RETURN @c;
END;
```

### **8.2. Passing a cursor handle between procedures**

```
CREATE PROCEDURE walk_orders(IN OUT c CURSOR OF orders%ROWTYPE)
AS
DECLARE
    @row orders%ROWTYPE;
BEGIN
    FETCH NEXT FROM c INTO @row;
    -- caller sees the advanced cursor position
END;
```
