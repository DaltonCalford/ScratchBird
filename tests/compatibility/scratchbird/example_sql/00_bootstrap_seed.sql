-- ScratchBird example database bootstrap + seed data
-- This script is executed during dynamic/static example DB initialization.

DROP USER IF EXISTS SysArch;
DROP USER IF EXISTS postgres;
DROP USER IF EXISTS root;
DROP USER IF EXISTS SYSDBA;
DROP USER IF EXISTS sb_public;
DROP USER IF EXISTS pg_public;
DROP USER IF EXISTS my_public;
DROP USER IF EXISTS fb_public;

CREATE USER SysArch WITH PASSWORD 'replaceme' SUPERUSER;
CREATE USER postgres WITH PASSWORD 'postgres' SUPERUSER;
CREATE USER root WITH PASSWORD 'root' SUPERUSER;
CREATE USER SYSDBA WITH PASSWORD 'masterkey' SUPERUSER;

CREATE USER sb_public WITH PASSWORD 'sb_public';
CREATE USER pg_public WITH PASSWORD 'pg_public';
CREATE USER my_public WITH PASSWORD 'my_public';
CREATE USER fb_public WITH PASSWORD 'fb_public';

-- Ensure scripted user DDL is durable for subsequent seed/login phases.
COMMIT;

-- NOTE:
-- SUPERUSER accounts already have full database access.
-- Keep bootstrap free of GRANT statements until the semantic-bridge
-- closure path for SBLR3_GRANT is fully enabled in all test profiles.
-- Post-bootstrap schema/data seeding runs in:
-- tests/compatibility/scratchbird/example_sql/01_post_bootstrap_seed.sql
