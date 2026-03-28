DELETE FROM vncr_b9426f_tb;
SELECT 'ASSERT|firebird_delete_all_rows|remaining|' || CAST(COUNT(*) AS VARCHAR(20)) FROM vncr_b9426f_tb;
