-- Native bootstrap: users/roles + base database

CREATE DATABASE sb_grind_native;

-- Roles
CREATE ROLE sb_app_read;
CREATE ROLE sb_app_write;
CREATE ROLE sb_app_admin;

-- Users
CREATE USER sb_reader WITH PASSWORD 'sb_reader_pw';
CREATE USER sb_writer WITH PASSWORD 'sb_writer_pw';
CREATE USER sb_runner WITH PASSWORD 'sb_runner_pw';

-- Role grants
GRANT sb_app_read TO sb_reader;
GRANT sb_app_write TO sb_writer;
GRANT sb_app_admin TO sb_runner;
GRANT sb_app_read, sb_app_write TO sb_runner;

-- Group placeholders (parser support pending)
-- CREATE GROUP sb_ops;
-- CREATE GROUP sb_analytics;
-- ALTER USER sb_runner ADD TO GROUP sb_ops;
-- ALTER USER sb_reader ADD TO GROUP sb_analytics;

-- Database grants
GRANT ALL ON DATABASE sb_grind_native TO sb_app_admin;
GRANT CONNECT ON DATABASE sb_grind_native TO sb_app_read, sb_app_write;

