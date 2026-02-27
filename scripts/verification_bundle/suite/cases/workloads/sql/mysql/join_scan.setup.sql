DROP TABLE IF EXISTS vw_join_scan_b;
DROP TABLE IF EXISTS vw_join_scan_a;
CREATE TABLE vw_join_scan_a (id INT PRIMARY KEY, grp INT NOT NULL, payload VARCHAR(32));
CREATE TABLE vw_join_scan_b (id INT PRIMARY KEY, a_id INT NOT NULL, metric INT NOT NULL);
INSERT INTO vw_join_scan_a (id, grp, payload) VALUES
  (1, 1, 'a1'),(2, 1, 'a2'),(3, 2, 'a3'),(4, 2, 'a4');
INSERT INTO vw_join_scan_b (id, a_id, metric) VALUES
  (1, 1, 11),(2, 1, 13),(3, 2, 17),(4, 3, 19),(5, 4, 23);
