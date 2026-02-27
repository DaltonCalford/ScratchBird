DROP TABLE IF EXISTS vt_dml_join_child;
DROP TABLE IF EXISTS vt_dml_join_parent;
CREATE TABLE vt_dml_join_parent (
  id INT PRIMARY KEY,
  name VARCHAR(64)
);
CREATE TABLE vt_dml_join_child (
  id INT PRIMARY KEY,
  parent_id INT NOT NULL,
  metric INT NOT NULL
);
INSERT INTO vt_dml_join_parent (id, name) VALUES (1, 'p1'), (2, 'p2');
INSERT INTO vt_dml_join_child (id, parent_id, metric) VALUES
  (1, 1, 5),
  (2, 1, 7),
  (3, 2, 11);
