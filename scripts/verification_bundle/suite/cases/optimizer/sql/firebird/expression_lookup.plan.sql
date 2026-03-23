SET PLAN ON;
SELECT id, payload FROM avp_expression_lookup WHERE LOWER(name) = 'charlie';
SET PLAN OFF;
