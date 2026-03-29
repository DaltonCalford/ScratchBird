CREATE TABLE vncr_940f11_t1 (
    a INT NOT NULL AUTO_INCREMENT,
    t TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    c CHAR(10) DEFAULT 'hello',
    i INT,
    PRIMARY KEY (a)
);
INSERT INTO vncr_940f11_t1 VALUES
    (DEFAULT, DEFAULT, DEFAULT, DEFAULT),
    (DEFAULT, DEFAULT, DEFAULT, DEFAULT);
INSERT INTO vncr_940f11_t1 (a, c, i) VALUES (4, 'a', 5);
SELECT CONCAT('ASSERT|mysql_insert_defaults_identity|rows|', COUNT(*)) FROM vncr_940f11_t1;
SELECT CONCAT('ASSERT|mysql_insert_defaults_identity|hello_rows|', COUNT(*)) FROM vncr_940f11_t1 WHERE c = 'hello';
SELECT CONCAT('ASSERT|mysql_insert_defaults_identity|explicit_id|', COUNT(*)) FROM vncr_940f11_t1 WHERE a = 4;
DROP TABLE vncr_940f11_t1;
