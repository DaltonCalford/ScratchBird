DROP TABLE IF EXISTS avp_expression_lookup;
CREATE TABLE avp_expression_lookup (
  id INTEGER PRIMARY KEY,
  name VARCHAR(32) NOT NULL,
  payload VARCHAR(32) NOT NULL
);
CREATE INDEX idx_avp_expression_lookup_lower_name ON avp_expression_lookup (LOWER(name));
INSERT INTO avp_expression_lookup (id, name, payload) VALUES
  (1, 'Alpha', 'alpha-a'),
  (2, 'BRAVO', 'bravo-a'),
  (3, 'Charlie', 'charlie-a'),
  (4, 'charlie', 'charlie-b'),
  (5, 'Delta', 'delta-a'),
  (6, 'Echo', 'echo-a');
