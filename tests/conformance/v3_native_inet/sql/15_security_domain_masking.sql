
DROP VIEW IF EXISTS v3_sec_domain_masked_view;
DROP TABLE IF EXISTS v3_sec_domain_data;
DROP DOMAIN IF EXISTS v3_sec_mask_domain;

CREATE DOMAIN v3_sec_mask_domain AS VARCHAR(16)
    WITH SECURITY (MASKING='PARTIAL', MASK_PATTERN='***-**-####', REQUIRE_PRIVILEGE='VIEW_SENSITIVE');

CREATE TABLE v3_sec_domain_data (
    id INTEGER PRIMARY KEY,
    masked_value v3_sec_mask_domain,
    owner_name VARCHAR(64)
);

INSERT INTO v3_sec_domain_data (id, masked_value, owner_name) VALUES
    (1, '123-45-6789', CURRENT_USER),
    (2, '***-**-****', 'other_user');

CREATE VIEW v3_sec_domain_masked_view AS
SELECT id,
       masked_value AS masked_projection
FROM v3_sec_domain_data;

SHOW DOMAIN v3_sec_mask_domain;

SELECT 'ASSERT|sec_domain|domain_exists|' || CAST(COUNT(*) AS VARCHAR(20))
FROM information_schema.domains
WHERE LOWER(domain_name) = 'v3_sec_mask_domain';

SELECT 'ASSERT|sec_domain|masked_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3_sec_domain_masked_view
WHERE masked_projection = '***-**-****';

SELECT 'ASSERT|sec_domain|clear_rows|' || CAST(COUNT(*) AS VARCHAR(20))
FROM v3_sec_domain_masked_view
WHERE masked_projection <> '***-**-****';
