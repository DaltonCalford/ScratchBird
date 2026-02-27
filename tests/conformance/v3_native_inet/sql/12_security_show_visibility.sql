
DROP TABLE IF EXISTS sec_show_private.v3_sec_show_private_obj;
DROP TABLE IF EXISTS public.v3_sec_show_public_obj;
DROP SCHEMA IF EXISTS sec_show_private CASCADE;

CREATE ROLE v3_sec_show_user;
CREATE SCHEMA sec_show_private;

CREATE TABLE public.v3_sec_show_public_obj (
    id INTEGER PRIMARY KEY,
    payload VARCHAR(32)
);

CREATE TABLE sec_show_private.v3_sec_show_private_obj (
    id INTEGER PRIMARY KEY,
    payload VARCHAR(32)
);

GRANT SELECT ON TABLE public.v3_sec_show_public_obj TO v3_sec_show_user;

SHOW TABLES FROM public;
SHOW TABLES FROM sec_show_private;
SHOW GRANTS FOR v3_sec_show_user;
SHOW TABLE v3_sec_show_public_obj;
