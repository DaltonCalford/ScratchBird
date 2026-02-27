DROP TABLE IF EXISTS vt_ddl_create_table_basic;
CREATE TABLE vt_ddl_create_table_basic (
  id INTEGER PRIMARY KEY,
  amount INTEGER NOT NULL,
  note VARCHAR(64)
);
