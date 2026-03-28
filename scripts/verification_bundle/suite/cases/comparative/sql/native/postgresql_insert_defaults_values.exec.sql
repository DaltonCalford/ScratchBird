CREATE TABLE __VNCR_NS___inserttest(
    col1 INTEGER,
    col2 INTEGER NOT NULL DEFAULT 1,
    col3 TEXT DEFAULT 'testing'
);
INSERT INTO __VNCR_NS___inserttest (col1, col2, col3) VALUES (DEFAULT, DEFAULT, DEFAULT);
INSERT INTO __VNCR_NS___inserttest (col2, col3) VALUES (3, DEFAULT);
INSERT INTO __VNCR_NS___inserttest VALUES (DEFAULT, 5, 'test');
INSERT INTO __VNCR_NS___inserttest VALUES (DEFAULT, 7, DEFAULT);
SELECT 'ASSERT|postgresql_insert_defaults_values|rows|' || CAST(COUNT(*) AS VARCHAR(20)) FROM __VNCR_NS___inserttest;
SELECT 'ASSERT|postgresql_insert_defaults_values|testing_rows|' || CAST(COUNT(*) AS VARCHAR(20)) FROM __VNCR_NS___inserttest WHERE col3 = 'testing';
DROP TABLE __VNCR_NS___inserttest;
