# V3 Parser: GRANT and REVOKE

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: define parsing, SBLR emission, catalog storage, and executor semantics for
GRANT/REVOKE and role grants. This spec is canonical for ScratchBird V3.

---

## 1) Scope

Covers:
- GRANT/REVOKE privileges on database objects.
- GRANT/REVOKE roles and membership.
- Storage of privileges in the security catalog.
- Locking, error handling, and transactional semantics.

Out of scope:
- External authentication systems (LDAP/Kerberos) and password storage.
- Detailed audit log formats (see security/operations specs).

---

## 2) Grammar (Authoritative)

### 2.1 Privilege Grants

```
GRANT <priv_list> ON [<object_type>] <object_list>
  TO <grantee_list> [WITH GRANT OPTION]

REVOKE [GRANT OPTION FOR] <priv_list> ON [<object_type>] <object_list>
  FROM <grantee_list> [CASCADE | RESTRICT]
```

Notes:
- If `<object_type>` is omitted, it defaults to `TABLE`.
- `<object_list>` is a comma‑separated list of schema paths.
- `<grantee_list>` is a comma‑separated list of grantees or `PUBLIC`.

### 2.2 Role Grants

```
GRANT <role_list> TO <grantee_list> [WITH ADMIN OPTION]
REVOKE [ADMIN OPTION FOR] <role_list> FROM <grantee_list> [CASCADE | RESTRICT]
```

Notes:
- Role grants do **not** use `ON <object_type>`.
- `WITH ADMIN OPTION` grants the right to grant the role to others.

### 2.3 Privilege List

```
<priv_list> := ALL [PRIVILEGES]
             | <priv> (',' <priv>)*

<priv> := SELECT | INSERT | UPDATE | DELETE | TRUNCATE | REFERENCES | TRIGGER
        | EXECUTE | EXECUTE EXTERNAL JOB
        | USAGE | COPY | CREATE JOB | VIEW JOB HISTORY
```

### 2.4 Object Types

```
<object_type> := TABLE | VIEW | SEQUENCE | FUNCTION | PROCEDURE | SCHEMA
               | DATABASE | INDEX | DOMAIN | TYPE | JOB | POLICY
               | SERVER | FOREIGN TABLE | SYNONYM
```

If `<object_type>` is omitted, `TABLE` is assumed.

### 2.5 Grantees

```
<grantee_list> := PUBLIC | <grantee> (',' <grantee>)*
<grantee> := <ident> | USER <ident> | ROLE <ident> | GROUP <ident>
```

If `USER/ROLE/GROUP` keyword is omitted, resolution is performed by lookup order:
`ROLE`, `GROUP`, then `USER`. If ambiguous, the statement is rejected.

---

## 3) Privilege Matrix

Privileges are valid only on compatible object types.

| Privilege | Applies To |
| --- | --- |
| SELECT | TABLE, VIEW, FOREIGN TABLE |
| INSERT | TABLE, FOREIGN TABLE |
| UPDATE | TABLE, FOREIGN TABLE |
| DELETE | TABLE, FOREIGN TABLE |
| TRUNCATE | TABLE |
| REFERENCES | TABLE |
| TRIGGER | TABLE |
| EXECUTE | FUNCTION, PROCEDURE |
| EXECUTE EXTERNAL JOB | JOB |
| USAGE | SCHEMA, SEQUENCE, DOMAIN, TYPE |
| COPY | TABLE, FOREIGN TABLE |
| CREATE JOB | DATABASE |
| VIEW JOB HISTORY | DATABASE |

`ALL [PRIVILEGES]` expands to all privileges valid for the object type.

Invalid combinations MUST be rejected at parse time with `SQLSTATE 0A000`
(feature not supported) or `SQLSTATE 42809` (invalid object type), depending on context.

---

## 4) AST Schema (Logical Fields)

### 4.1 GrantStmt

- `kind`: `GRANT_PRIVILEGE | GRANT_ROLE`
- `privileges`: list of privilege identifiers (empty when `kind=GRANT_ROLE`)
- `roles`: list of role identifiers (empty when `kind=GRANT_PRIVILEGE`)
- `object_type`: enum (or implicit `TABLE` when omitted)
- `objects`: list of schema paths
- `grantees`: list of grantee identifiers or `PUBLIC`
- `with_grant_option`: bool
- `with_admin_option`: bool

### 4.2 RevokeStmt

- `kind`: `REVOKE_PRIVILEGE | REVOKE_ROLE`
- `privileges`: list of privilege identifiers
- `roles`: list of role identifiers
- `object_type`: enum (or implicit `TABLE` when omitted)
- `objects`: list of schema paths
- `grantees`: list of grantee identifiers or `PUBLIC`
- `grant_option_for`: bool
- `admin_option_for`: bool
- `cascade`: bool
- `restrict`: bool

---

## 5) SBLR Emission (Normative)

### 5.1 Opcode Usage

- `SBLR3_GRANT` + `SBLR3_GRANT_PRIVILEGE` for privilege grants
- `SBLR3_REVOKE` + `SBLR3_REVOKE_PRIVILEGE` for privilege revokes
- `SBLR3_GRANT_ROLE` / `SBLR3_REVOKE_ROLE` for role grants/revokes
- `SBLR3_GRANT_OPTION` is emitted only when `WITH GRANT OPTION` is present

### 5.2 Emission Rules

1. Emit `SBLR3_GRANT` / `SBLR3_REVOKE` header with:
   - `object_type`
   - `object_list`
   - `grantee_list`
   - `flags` (`WITH_GRANT_OPTION`, `GRANT_OPTION_FOR`, `CASCADE`, `RESTRICT`)
2. Emit one `SBLR3_GRANT_PRIVILEGE` / `SBLR3_REVOKE_PRIVILEGE` per privilege.
3. For role grants, emit `SBLR3_GRANT_ROLE` / `SBLR3_REVOKE_ROLE` with:
   - `role_list`
   - `grantee_list`
   - `flags` (`WITH_ADMIN_OPTION`, `ADMIN_OPTION_FOR`, `CASCADE`, `RESTRICT`)

All identifiers in SBLR payloads MUST use `string_id` references.

---

## 6) Catalog Storage (Authoritative)

Security metadata is stored under `sys.sec.*` schemas. All IDs are UUID v7.

### 6.1 Required Tables (Logical)

- `sys.sec.privileges`
  - `priv_id` (UUID)
  - `object_type` (u16)
  - `object_id` (UUID)
  - `privilege` (u16 bit or enum)
  - `grantee_type` (USER|ROLE|GROUP|PUBLIC)
  - `grantee_id` (UUID or NULL for PUBLIC)
  - `grantor_id` (UUID)
  - `grant_option` (bool)
  - `granted_at` (timestamp)

- `sys.sec.role_members`
  - `role_id` (UUID)
  - `member_type` (USER|ROLE|GROUP)
  - `member_id` (UUID)
  - `admin_option` (bool)
  - `grantor_id` (UUID)
  - `granted_at` (timestamp)

`SBDB$` domains must be used for all columns; raw types are forbidden.

---

## 7) Executor Semantics (Normative)

### 7.1 GRANT

A GRANT succeeds only if the grantor:
- owns the object, OR
- has `WITH GRANT OPTION` for the privilege, OR
- has a database‑level admin role.

If `ALL PRIVILEGES` is used, it expands to all valid privileges for the object type.

### 7.2 REVOKE

- `GRANT OPTION FOR` removes only the grant option, not the privilege.
- `CASCADE` revokes dependent grants (grants that were made based on this grant).
- `RESTRICT` rejects if dependent grants exist.

### 7.3 Role Grants

- `WITH ADMIN OPTION` allows grantee to grant the role to others.
- `ADMIN OPTION FOR` revokes only admin option.

### 7.4 Transactional Behavior

All GRANT/REVOKE operations are fully transactional and must obey MGA semantics.
Catalog changes must be rolled back if the transaction aborts.

---

## 8) Locking and Ordering

Lock acquisition MUST follow the global order in `EXECUTOR_V3_SBLR.md` and
`EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`:

1. Schema-level metadata lock
2. Object metadata lock
3. Security catalog lock

If multiple objects are referenced, locks are acquired in lexicographic UUID order.

---

## 9) Error Codes / SQLSTATE

- `42501` insufficient_privilege
- `42704` undefined_object (target not found)
- `42809` wrong_object_type
- `0A000` feature_not_supported
- `2F003` invalid_grantor (grantor lacks rights)
- `2BP01` dependent_objects_still_exist (RESTRICT violation)

---

## 10) Determinism

Privilege lists are unordered and MUST be canonicalized (sorted) during emission
per `SBLR_V3_BYTECODE_CANONICALIZATION.md`.

---

## 11) Implementation References (Legacy)

Legacy parsing code references (non‑authoritative):
- `src/parser/parser_v2.cpp:278`
- `src/parser/parser_v2.cpp:9685`
- `src/parser/parser_v2.cpp:9784`
