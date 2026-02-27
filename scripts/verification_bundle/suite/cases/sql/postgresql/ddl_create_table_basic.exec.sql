INSERT INTO vt_ddl_create_table_basic (id, amount, note) VALUES
  (1, 10, 'a'),
  (2, 20, 'b'),
  (3, 30, 'c');
SELECT 'ASSERT|ddl_create_table_basic.rows|' || COUNT(*) FROM vt_ddl_create_table_basic;
SELECT 'ASSERT|ddl_create_table_basic.sum_amount|' || COALESCE(SUM(amount), 0) FROM vt_ddl_create_table_basic;
