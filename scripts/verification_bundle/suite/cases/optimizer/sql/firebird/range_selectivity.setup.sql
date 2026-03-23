RECREATE TABLE avp_range_selectivity (
  id INTEGER NOT NULL PRIMARY KEY,
  k INTEGER NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_range_selectivity_k ON avp_range_selectivity (k);
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (1, 5, 'r005');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (2, 10, 'r010');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (3, 15, 'r015');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (4, 20, 'r020');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (5, 25, 'r025');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (6, 30, 'r030');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (7, 35, 'r035');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (8, 40, 'r040');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (9, 45, 'r045');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (10, 50, 'r050');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (11, 55, 'r055');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (12, 60, 'r060');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (13, 65, 'r065');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (14, 70, 'r070');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (15, 75, 'r075');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (16, 80, 'r080');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (17, 85, 'r085');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (18, 90, 'r090');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (19, 95, 'r095');
INSERT INTO avp_range_selectivity (id, k, payload) VALUES (20, 100, 'r100');
COMMIT;
