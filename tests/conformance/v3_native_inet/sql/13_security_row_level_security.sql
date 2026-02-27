
DROP TABLE IF EXISTS v3_sec_rls_table;

CREATE TABLE v3_sec_rls_table (
    id INTEGER PRIMARY KEY,
    owner_name VARCHAR(64),
    payload VARCHAR(32)
);

INSERT INTO v3_sec_rls_table (id, owner_name, payload) VALUES
    (1, CURRENT_USER, 'mine'),
    (2, 'someone_else', 'other');

ALTER TABLE v3_sec_rls_table ENABLE ROW LEVEL SECURITY;
ALTER TABLE v3_sec_rls_table FORCE ROW LEVEL SECURITY;

CREATE POLICY v3_sec_rls_policy ON v3_sec_rls_table
    FOR SELECT
    TO PUBLIC
    USING (owner_name = CURRENT_USER);

SELECT 'ASSERT|sec_rls|with_rls_count|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3_sec_rls_table;

ALTER TABLE v3_sec_rls_table DISABLE ROW LEVEL SECURITY;

SELECT 'ASSERT|sec_rls|without_rls_count|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3_sec_rls_table;
