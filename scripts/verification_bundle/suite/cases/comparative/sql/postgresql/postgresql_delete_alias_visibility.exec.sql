CREATE TABLE __VNCR_NS___delete_test(
    id INTEGER PRIMARY KEY,
    a INTEGER,
    b TEXT
);
INSERT INTO __VNCR_NS___delete_test VALUES (1, 10, NULL);
INSERT INTO __VNCR_NS___delete_test VALUES (2, 50, 'payload');
INSERT INTO __VNCR_NS___delete_test VALUES (3, 100, NULL);
DELETE FROM __VNCR_NS___delete_test AS dt WHERE dt.a > 75;
SELECT 'ASSERT|postgresql_delete_alias_visibility|rows|' || CAST(COUNT(*) AS VARCHAR(20)) FROM __VNCR_NS___delete_test;
SELECT 'ASSERT|postgresql_delete_alias_visibility|max_a|' || CAST(MAX(a) AS VARCHAR(20)) FROM __VNCR_NS___delete_test;
DROP TABLE __VNCR_NS___delete_test;
