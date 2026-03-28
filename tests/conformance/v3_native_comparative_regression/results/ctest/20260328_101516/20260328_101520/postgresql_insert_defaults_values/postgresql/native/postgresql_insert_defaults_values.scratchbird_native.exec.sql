CREATE TABLE vncr_07198b_inserttest(
    col1 INTEGER,
    col2 INTEGER NOT NULL DEFAULT 1,
    col3 TEXT DEFAULT 'testing'
);
INSERT INTO vncr_07198b_inserttest (col1, col2, col3) VALUES (DEFAULT, DEFAULT, DEFAULT);
INSERT INTO vncr_07198b_inserttest (col2, col3) VALUES (3, DEFAULT);
INSERT INTO vncr_07198b_inserttest VALUES (DEFAULT, 5, 'test');
INSERT INTO vncr_07198b_inserttest VALUES (DEFAULT, 7, DEFAULT);
SELECT 'ASSERT|postgresql_insert_defaults_values|rows|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_07198b_inserttest;
SELECT 'ASSERT|postgresql_insert_defaults_values|testing_rows|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_07198b_inserttest WHERE col3 = 'testing';
DROP TABLE vncr_07198b_inserttest;
