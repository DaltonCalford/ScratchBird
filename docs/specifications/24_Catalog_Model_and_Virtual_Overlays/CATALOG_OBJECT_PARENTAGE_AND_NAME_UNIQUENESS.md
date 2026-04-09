# Catalog Object Parentage And Name Uniqueness

## Purpose
Define parent-child ownership and uniqueness constraints for catalog objects.

## Core Rules
1. Engine identity is UUID-only after binding.
2. Friendly names are stored in the name registry and resolved by parsers.
3. A parent may not have two children of the same object type with the same canonical name.
4. Parent scope is mandatory for name uniqueness checks.

## Parentage Matrix

| Child Object Type | Parent Object Type |
| --- | --- |
| schema | database (root) or schema |
| table | schema |
| column | table |
| index | table |
| table_constraint | table |
| fk_constraint | table |
| trigger | table or database (for database-level triggers) |
| sequence | schema |
| view | schema |
| function/procedure | schema or package |
| package_member | package |
| rule | table or view |
| partition | table |
| table_inheritance edge | table (parent) + table (child) |

## Uniqueness Constraint
Unique key for names:
- `(parent_object_uuid, object_type, canonical_name_text, language='default')`

Compatibility alternate key for schema-owned objects:
- `(parent_schema_uuid, object_type, canonical_name_text, language='default')`

## Parent Object UUID Rules
1. Table-owned objects (`index`, `table_constraint`, `trigger`, `column`) must set `parent_object_uuid = table_uuid`.
2. Schema-owned objects must set `parent_object_uuid = schema_uuid`.
3. Name lookups for child objects must use parent object UUID scope first, never global schema scan.

## Parser Lookup Rules
1. Unqualified table name lookup:
- search path scans schema scope.
2. Trigger/index/constraint name lookup:
- uses table parent scope.
3. Fully qualified child object lookup:
- resolve parent table UUID first, then resolve child by parent UUID scope.

## Catalog Requirements
The following tables must include parent ownership fields:
1. `object.parent_object_uuid`
2. `object_name.parent_object_uuid`

## Failure Conditions
1. Duplicate child name/type under same parent -> `NAME_COLLISION`.
2. Child object with missing parent UUID -> `INVALID_PARENT_SCOPE`.
3. Parent type mismatch -> `PARENT_TYPE_MISMATCH`.

## Test Clauses
1. Create two triggers with same name on same table fails.
2. Same trigger name on different tables succeeds.
3. Same index name on same table fails; on different tables succeeds.
4. Table and view with same name under same schema fails when object_type policy disallows collision for that pair.
5. Name-resolution of trigger/index/constraint requires parent table scope.
