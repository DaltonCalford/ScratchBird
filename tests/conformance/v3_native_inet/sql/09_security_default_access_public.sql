
DROP TABLE IF EXISTS sec_default_private.v3_sec_def_private;
DROP TABLE IF EXISTS public.v3_sec_def_public;
DROP SCHEMA IF EXISTS sec_default_private CASCADE;

CREATE ROLE v3_sec_default_user;
CREATE SCHEMA sec_default_private;

CREATE TABLE sec_default_private.v3_sec_def_private (
    id INTEGER PRIMARY KEY,
    payload VARCHAR(32)
);

CREATE TABLE public.v3_sec_def_public (
    id INTEGER PRIMARY KEY,
    payload VARCHAR(32)
);

SELECT 'ASSERT|sec_default|initial_grants|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.table_privileges
WHERE grantee = 'v3_sec_default_user'
  AND table_name IN ('v3_sec_def_private', 'v3_sec_def_public');

GRANT SELECT ON TABLE public.v3_sec_def_public TO v3_sec_default_user;

SELECT 'ASSERT|sec_default|public_select_grant|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.table_privileges
WHERE grantee = 'v3_sec_default_user'
  AND table_name = 'v3_sec_def_public'
  AND privilege_type = 'SELECT';

SELECT 'ASSERT|sec_default|private_select_grant|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.table_privileges
WHERE grantee = 'v3_sec_default_user'
  AND table_name = 'v3_sec_def_private'
  AND privilege_type = 'SELECT';

SELECT 'ASSERT|sec_default|total_grants_two_objects|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.table_privileges
WHERE grantee = 'v3_sec_default_user'
  AND table_name IN ('v3_sec_def_private', 'v3_sec_def_public');
