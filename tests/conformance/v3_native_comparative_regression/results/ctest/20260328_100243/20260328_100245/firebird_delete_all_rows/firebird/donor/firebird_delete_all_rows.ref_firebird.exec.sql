DELETE FROM vncr_03c1c6_tb;
SELECT 'ASSERT|firebird_delete_all_rows|remaining|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_03c1c6_tb;
