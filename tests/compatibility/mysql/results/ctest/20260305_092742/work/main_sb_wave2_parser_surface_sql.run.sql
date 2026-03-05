SET SESSION sql_log_bin = 0;
SET GLOBAL log_bin_trust_function_creators = 1;
-- MySQL emulation parser wave-2 surface coverage
-- Statement families: EPFC-032..EPFC-036 closure vectors

CREATE TABLE IF NOT EXISTS sb_wave2_parser_surface (
  id INT PRIMARY KEY,
  payload VARCHAR(32)
);
INSERT INTO sb_wave2_parser_surface VALUES (1, 'alpha');

CHECK TABLE sb_wave2_parser_surface;
OPTIMIZE TABLE sb_wave2_parser_surface;
START REPLICA;
STOP REPLICA;
SHOW REPLICA STATUS;

XA START 'sbx1';
XA END 'sbx1';
ROLLBACK;
XA RECOVER;

LOAD DATA INFILE '/tmp/sb_dummy.csv' INTO TABLE sb_wave2_parser_surface;
HANDLER sb_wave2_parser_surface OPEN;
HANDLER sb_wave2_parser_surface READ FIRST;
HANDLER sb_wave2_parser_surface CLOSE;

CLONE LOCAL DATA DIRECTORY = '/tmp/sb_clone_target';
CLONE INSTANCE FROM donor:3306 IDENTIFIED BY 'compat_password';
ALTER TABLE sb_wave2_parser_surface IMPORT TABLESPACE;

