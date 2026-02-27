DROP TABLE IF EXISTS vt_dml_update_delete;
CREATE TABLE vt_dml_update_delete (
  id INTEGER PRIMARY KEY,
  score INTEGER NOT NULL,
  flag INTEGER NOT NULL
);
INSERT INTO vt_dml_update_delete (id, score, flag) VALUES
  (1, 10, 0),
  (2, 20, 0),
  (3, 30, 1),
  (4, 40, 1);
