DROP TABLE IF EXISTS vt_dml_insert_multirow;
CREATE TABLE vt_dml_insert_multirow (
  id INT PRIMARY KEY,
  grp INT NOT NULL,
  payload VARCHAR(64)
);
