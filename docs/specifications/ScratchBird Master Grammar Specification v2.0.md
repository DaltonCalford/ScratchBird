# ScratchBird Master Grammar Specification v2.0

Architecture: Recursive Descent with State-Aware Dispatch ("Smart Parser, Dumb Lexer").

Design Goal: < 50 Reserved Words.

Key Feature: Recursive Schema Navigation (Filesystem Metaphor).

Parser Model:
- ScratchBird V2 is the primary/core parser and supports ALL engine functionality.
- Emulated parsers (Firebird, PostgreSQL, MySQL) are separate front-ends that only implement the
  subset supported by their native engines.
- The engine executes only SBLR; parsers emit SBLR and remain isolated.

---

## Related Specifications
- `docs/specifications/SCHEMA_PATH_RESOLUTION.md`

---

## 1. Lexical Specification

### 1.1 The Gatekeepers (Strictly Reserved)

These are the **only** words that cannot be used as unquoted identifiers at the start of a statement. They define the initial parsing state.

| **Category**    | **Keywords**                                                    |
| --------------- | --------------------------------------------------------------- |
| **Control**     | `CREATE`, `ALTER`, `DROP`, `TRUNCATE`, `COMMENT`                |
| **DML**         | `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `MERGE`, `WITH`         |
| **Transaction** | `START`, `COMMIT`, `ROLLBACK`, `SAVEPOINT`, `RELEASE`           |
| **Session**     | `SET`, `RESET`, `SHOW`, `DESCRIBE`                              |
| **Flow**        | `CALL`, `BEGIN`, `END`, `IF`, `CASE`, `WHILE`, `LOOP`, `RETURN` |
| **Logic**       | `AND`, `OR`, `NOT`, `NULL`, `TRUE`, `FALSE`                     |
| **Security**    | `GRANT`, `REVOKE`                                               |

### 1.2 The Contextual List (Demoted to Identifiers)

The following words are **NO LONGER RESERVED**. They are recognized as keywords *only* when the parser is in a specific state.

- **Structure:** `TABLE`, `INDEX`, `VIEW`, `PROCEDURE`, `FUNCTION`, `TRIGGER`, `SEQUENCE`, `DOMAIN`, `TYPE`

- **Attributes:** `VALUE`, `DATA`, `name`, `type`, `date`, `user`, `role`, `path`

- **Transaction:** `ISOLATION`, `LEVEL`, `SNAPSHOT`, `WAIT`, `READ`, `WRITE`

- **Options:** `CASCADE`, `RESTRICT`, `ASYNC`, `SYNC`, `FORCE`

---

## 2. Statement Dispatch (The Gatekeeper)

The `parseStatement()` function acts as the root state machine. It peeks at the first token and dispatches to a specific subsystem.

C++

```
// C++ Pseudo-code for Gatekeeper Logic
Statement* parseStatement() {
    if (match(KW_SELECT)) return parseSelect();
    if (match(KW_INSERT)) return parseInsert();
    if (match(KW_SET))    return parseSet();    // Enters Session State
    if (match(KW_CREATE)) return parseCreate(); // Enters DDL State
    // ... etc

    // If token is not a Gatekeeper, it is an Identifier (e.g., assignment in PL/SQL)
    return parseAssignmentOrCall();
}
```

---

## 3. Schema Navigation System (`SET`)

This subsystem replaces the static schema model with a stateful "filesystem" navigation model.

### 3.1 Grammar (BNF)

BNF

```
<set_statement> ::=
    SET <set_target>

<set_target> ::=
    /* Recursive Schema Navigation */
    CURRENT SCHEMA <schema_navigation>
  | SEARCH PATH <path_list>

    /* Transaction/Session settings */
  | TRANSACTION <transaction_options>
  | ROLE <identifier>
  | NAMES <charset>

<schema_navigation> ::=
    UP                          /* Equivalent to: cd .. */
  | DEFAULT                     /* Equivalent to: cd ~ (home) */
  | <schema_path>               /* Equivalent to: cd path */

<schema_path> ::=
    <identifier>                /* Relative:  cd subschema */
  | / <identifier_path>         /* Absolute:  cd /root/subschema */

<path_list> ::=
    <schema_path> [ , <schema_path> ]*
```

### 3.2 Contextual Logic

- After consuming `SET`, the parser enters **Session State**.

- It checks `current().text` (case-insensitive).

- If text is "CURRENT", it expects "SCHEMA".

- It does **not** fail if the user has a table named `current`. It only parses it as a command if `SET` preceded it.

---

## 4. DDL State Machine (`CREATE`, `ALTER`, `DROP`)

This handles the creation of objects without reserving object names globally.

### 4.1 Grammar (BNF)

BNF

```
<create_statement> ::=
    CREATE [ OR REPLACE ] <object_type> <object_def>

<object_type> ::=
    /* Contextual matching on Identifier text */
    TABLE | VIEW | INDEX | SEQUENCE | PROCEDURE | FUNCTION | TRIGGER | USER

<object_def> ::=
    /* If TABLE */ <identifier> ( <column_def_list> ) ...
    /* If INDEX */ <identifier> ON <table_ref> ...
    /* If PROCEDURE */ <identifier> ( <params> ) AS <block>
```

### 4.2 Example Resolution

Query: `CREATE TABLE procedure (value INT)`

1. **Gatekeeper:** `CREATE` triggers DDL State.

2. **Context Check:** Next token is identifier `TABLE`. Matches `checkContextual("TABLE")`.

3. **Identifier Parsing:** Next token is identifier `procedure`.
   
   - Is it a Gatekeeper? No.
   
   - Is parsing in "Expect Identifier" mode? Yes.
   
   - **Valid Name.**

4. **Column Parsing:** Inside `( ... )`.
   
   - Next token is identifier `value`.
   
   - **Valid Column Name.**

---

## 5. Procedural Logic (`IF`, `LOOP`)

Control flow keywords are only reserved within the context of a Procedural Block (PSQL).

### 5.1 Grammar (BNF)

BNF

```
<psql_block> ::=
    [ DECLARE <declarations> ]
    BEGIN
        <statement_list>
    END

<statement_list> ::=
    <statement> [ ; <statement> ]*

<if_statement> ::=
    IF <condition> THEN
        <statement_list>
    [ ELSIF <condition> THEN <statement_list> ]*
    [ ELSE <statement_list> ]
    END IF
```

### 5.2 Contextual Logic

- `ELSE`, `ELSIF`, `THEN` are **not** Gatekeepers. They are effectively identifiers.

- However, `parseIfStatement()` specifically looks for them.

- If `parseIfStatement` finishes the `THEN` block, it peeks at the next token.
  
  - If token text is "ELSE", it consumes it as control flow.
  
  - If token text is "x", it assumes a new statement `x := ...`.

---

## 6. Firebird-Style Transaction Syntax

The transaction syntax is formalized to use contextual keywords instead of the `KW_NOT + KW_WAIT` hack.

### 6.1 Grammar (BNF)

BNF

```
<start_transaction> ::=
    START TRANSACTION <tx_options>

<tx_options> ::=
    [ <access_mode> ]
    [ <wait_mode> ]
    [ <isolation_level> ]
    [ <reservation> ]

<wait_mode> ::=
    WAIT
  | NO WAIT             /* Parsed as two contextual identifiers */

<isolation_level> ::=
    ISOLATION LEVEL <iso_type>

<iso_type> ::=
    SNAPSHOT [ TABLE STABILITY ]
  | READ COMMITTED

<reservation> ::=
    RESERVING <table_list> FOR <lock_mode>
```

---

## 7. DML with Recursive Paths (`SELECT`, `INSERT`)

Queries must support the recursive schema paths natively.

### 7.1 Grammar (BNF)

BNF

```
<table_reference> ::=
    <path_identifier> [ [ AS ] <alias> ]

<path_identifier> ::=
    <identifier>
  | <identifier> . <identifier>       /* Standard SQL dot notation */
  | / <identifier> [ / <identifier> ]* /* Absolute Path */
  | .. / <identifier>                 /* Relative Parent Path */
```

### 7.2 Example

SQL

```
SELECT * FROM /production/sales/orders AS o
JOIN ../hr/employees AS e ON o.emp_id = e.id
```

---

## 8. Implementation Roadmap

1. **Lexer Refactor:** Remove 450+ keywords from `lexer_v2.h`. Keep only the ~30 Gatekeepers.

2. **ParserState Class:** Create a class to track the current mode (`SESSION`, `DDL`, `DML`, `EXPRESSION`).

3. **Contextual Helper:** Implement `expectContextual(string)` which checks `token.type == IDENTIFIER && token.text == string`.

4. **Rewrite `parseStatement`:** Switch from checking `KW_CREATE` to dispatching based on the new Gatekeeper list.

5. **Path Logic:** Implement `parseSchemaPath` to handle `/` and `..` syntax tokens.
