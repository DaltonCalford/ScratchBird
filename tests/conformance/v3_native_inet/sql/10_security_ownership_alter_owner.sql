
DROP TABLE IF EXISTS sec_owner_schema.v3_sec_owner_table;
DROP SCHEMA IF EXISTS sec_owner_schema CASCADE;

CREATE ROLE v3_sec_owner_target;
CREATE SCHEMA sec_owner_schema;

CREATE TABLE sec_owner_schema.v3_sec_owner_table (
    id INTEGER PRIMARY KEY,
    payload VARCHAR(32)
);

SELECT 'ASSERT|sec_owner|schema_exists|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.schemata
WHERE schema_name = 'sec_owner_schema';

SELECT 'ASSERT|sec_owner|schema_owner_current|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.schemata
WHERE schema_name = 'sec_owner_schema'
  AND schema_owner = CURRENT_USER;

SELECT 'ASSERT|sec_owner|table_owner_current|' || CAST(COUNT(*) AS VARCHAR(20))
FROM pg_catalog.pg_class c
JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace
JOIN pg_catalog.pg_roles r ON r.oid = c.relowner
WHERE n.nspname = 'sec_owner_schema'
  AND c.relname = 'v3_sec_owner_table'
  AND r.rolname = CURRENT_USER;

ALTER SCHEMA sec_owner_schema OWNER TO v3_sec_owner_target;

SELECT 'ASSERT|sec_owner|schema_owner_target|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.schemata
WHERE schema_name = 'sec_owner_schema'
  AND schema_owner = 'v3_sec_owner_target';
