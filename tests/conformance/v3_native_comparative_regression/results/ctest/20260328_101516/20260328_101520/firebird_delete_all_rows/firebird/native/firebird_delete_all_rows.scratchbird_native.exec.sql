DELETE FROM vncr_347db6_tb;
SELECT 'ASSERT|firebird_delete_all_rows|remaining|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_347db6_tb;
