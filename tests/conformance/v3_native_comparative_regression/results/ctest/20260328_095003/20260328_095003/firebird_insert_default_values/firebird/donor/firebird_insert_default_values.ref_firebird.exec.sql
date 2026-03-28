CREATE TABLE vncr_43fec2_employee(
    prenom VARCHAR(20) DEFAULT 'anonymous',
    sex CHAR(1) DEFAULT 'M'
);
INSERT INTO vncr_43fec2_employee DEFAULT VALUES;
SELECT 'ASSERT|firebird_insert_default_values|anon|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM vncr_43fec2_employee
 WHERE prenom = 'anonymous';
SELECT 'ASSERT|firebird_insert_default_values|male|' || CAST(COUNT(*) AS VARCHAR(20))
  FROM vncr_43fec2_employee
 WHERE sex = 'M';
DROP TABLE vncr_43fec2_employee;
