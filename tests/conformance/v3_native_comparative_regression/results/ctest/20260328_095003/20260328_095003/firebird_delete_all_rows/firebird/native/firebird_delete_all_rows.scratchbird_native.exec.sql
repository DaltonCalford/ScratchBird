CREATE TABLE vncr_95b0db_tb(id INTEGER);
INSERT INTO vncr_95b0db_tb VALUES (10);
DELETE FROM vncr_95b0db_tb;
SELECT 'ASSERT|firebird_delete_all_rows|remaining|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_95b0db_tb;
DROP TABLE vncr_95b0db_tb;
