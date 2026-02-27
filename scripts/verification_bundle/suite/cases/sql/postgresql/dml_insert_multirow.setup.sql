DROP TABLE IF EXISTS vt_dml_insert_multirow;
CREATE TABLE vt_dml_insert_multirow (
  id INTEGER PRIMARY KEY,
  grp INTEGER NOT NULL,
  payload VARCHAR(64)
);
