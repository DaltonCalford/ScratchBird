CREATE TABLE vncr_479898_delete_test(
    id INTEGER PRIMARY KEY,
    a INTEGER,
    b TEXT
);
INSERT INTO vncr_479898_delete_test VALUES (1, 10, NULL);
INSERT INTO vncr_479898_delete_test VALUES (2, 50, 'payload');
INSERT INTO vncr_479898_delete_test VALUES (3, 100, NULL);
DELETE FROM vncr_479898_delete_test AS dt WHERE dt.a > 75;
SELECT 'ASSERT|postgresql_delete_alias_visibility|rows|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_479898_delete_test;
SELECT 'ASSERT|postgresql_delete_alias_visibility|max_a|' || CAST(MAX(a) AS VARCHAR(20)) FROM vncr_479898_delete_test;
DROP TABLE vncr_479898_delete_test;
