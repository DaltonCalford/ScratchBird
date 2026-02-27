
DROP TABLE IF EXISTS public.v3_sec_grant_table;

CREATE ROLE v3_sec_grant_role;

CREATE TABLE public.v3_sec_grant_table (
    id INTEGER PRIMARY KEY,
    payload VARCHAR(32)
);

SET TERM ^;

CREATE FUNCTION v3_sec_grant_fn() RETURNS INTEGER AS
BEGIN
    RETURN 1;
END^

CREATE PROCEDURE v3_sec_grant_proc AS
BEGIN
END^

SET TERM ;^

GRANT SELECT, INSERT, UPDATE, DELETE ON TABLE v3_sec_grant_table TO v3_sec_grant_role;
GRANT EXECUTE ON FUNCTION v3_sec_grant_fn TO v3_sec_grant_role;
GRANT EXECUTE ON PROCEDURE v3_sec_grant_proc TO v3_sec_grant_role;

SHOW FUNCTION v3_sec_grant_fn;
SHOW PROCEDURE v3_sec_grant_proc;
SHOW GRANTS FOR v3_sec_grant_role;

SELECT 'ASSERT|sec_grant|table_dml_count|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.table_privileges
WHERE grantee = 'v3_sec_grant_role'
  AND table_name = 'v3_sec_grant_table'
  AND privilege_type IN ('SELECT','INSERT','UPDATE','DELETE');

SELECT 'ASSERT|sec_grant|fn_execute|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.routine_privileges
WHERE grantee = 'v3_sec_grant_role'
  AND routine_name = 'v3_sec_grant_fn'
  AND privilege_type = 'EXECUTE';

SELECT 'ASSERT|sec_grant|fn_non_execute|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.routine_privileges
WHERE grantee = 'v3_sec_grant_role'
  AND routine_name = 'v3_sec_grant_fn'
  AND privilege_type <> 'EXECUTE';

SELECT 'ASSERT|sec_grant|proc_execute_or_more|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.routine_privileges
WHERE grantee = 'v3_sec_grant_role'
  AND routine_name = 'v3_sec_grant_proc'
  AND privilege_type = 'EXECUTE';
