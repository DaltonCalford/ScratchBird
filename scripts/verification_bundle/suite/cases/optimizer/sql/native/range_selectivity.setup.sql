DROP TABLE IF EXISTS avp_range_selectivity;
CREATE TABLE avp_range_selectivity (
  id INTEGER PRIMARY KEY,
  k INTEGER NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_range_selectivity_k ON avp_range_selectivity (k);
INSERT INTO avp_range_selectivity (id, k, payload) VALUES
  (1, 5, 'r005'),
  (2, 10, 'r010'),
  (3, 15, 'r015'),
  (4, 20, 'r020'),
  (5, 25, 'r025'),
  (6, 30, 'r030'),
  (7, 35, 'r035'),
  (8, 40, 'r040'),
  (9, 45, 'r045'),
  (10, 50, 'r050'),
  (11, 55, 'r055'),
  (12, 60, 'r060'),
  (13, 65, 'r065'),
  (14, 70, 'r070'),
  (15, 75, 'r075'),
  (16, 80, 'r080'),
  (17, 85, 'r085'),
  (18, 90, 'r090'),
  (19, 95, 'r095'),
  (20, 100, 'r100');
