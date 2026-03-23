RECREATE TABLE avp_expression_lookup (
  id INTEGER NOT NULL PRIMARY KEY,
  name VARCHAR(32) NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_expression_lookup_lower_name ON avp_expression_lookup COMPUTED BY (LOWER(name));
INSERT INTO avp_expression_lookup (id, name, payload) VALUES (1, 'Alpha', 'alpha-a');
INSERT INTO avp_expression_lookup (id, name, payload) VALUES (2, 'BRAVO', 'bravo-a');
INSERT INTO avp_expression_lookup (id, name, payload) VALUES (3, 'Charlie', 'charlie-a');
INSERT INTO avp_expression_lookup (id, name, payload) VALUES (4, 'charlie', 'charlie-b');
INSERT INTO avp_expression_lookup (id, name, payload) VALUES (5, 'Delta', 'delta-a');
INSERT INTO avp_expression_lookup (id, name, payload) VALUES (6, 'Echo', 'echo-a');
COMMIT;
