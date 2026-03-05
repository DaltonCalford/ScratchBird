-- ScratchBird example database post-bootstrap schema/data seed.
-- This runs after bootstrap user creation using SysArch credentials.

SET SCHEMA users.public;

-- Identity mapping model for compatibility harnesses:
--   - Canonical user identity is tracked in compat_identity_user_map_contract.canonical_userid.
--   - Engine-facing login aliases are tracked per engine_scope.
--   - This is a contract fixture only; runtime auth must not consume it yet.

CREATE TABLE compat_identity_user_map_contract (
    canonical_userid VARCHAR(64) NOT NULL,
    canonical_user VARCHAR(96) NOT NULL,
    engine_scope VARCHAR(24) NOT NULL,
    login_name VARCHAR(128) NOT NULL,
    external_alias VARCHAR(128),
    auth_method VARCHAR(32) NOT NULL,
    password_policy VARCHAR(48) NOT NULL,
    permission_profile VARCHAR(48) NOT NULL,
    is_superuser BOOLEAN NOT NULL,
    PRIMARY KEY (canonical_userid, engine_scope, login_name)
);

INSERT INTO compat_identity_user_map_contract (
    canonical_userid,
    canonical_user,
    engine_scope,
    login_name,
    external_alias,
    auth_method,
    password_policy,
    permission_profile,
    is_superuser
) VALUES ('u_sys_admin', 'sys_admin', 'native', 'SysArch', NULL, 'password', 'native_v3_strict', 'cluster_admin', TRUE);

INSERT INTO compat_identity_user_map_contract (
    canonical_userid,
    canonical_user,
    engine_scope,
    login_name,
    external_alias,
    auth_method,
    password_policy,
    permission_profile,
    is_superuser
) VALUES ('u_sys_admin', 'sys_admin', 'postgresql', 'postgres', NULL, 'scram_sha_256', 'pg_emulated_default', 'engine_admin', TRUE);

INSERT INTO compat_identity_user_map_contract (
    canonical_userid,
    canonical_user,
    engine_scope,
    login_name,
    external_alias,
    auth_method,
    password_policy,
    permission_profile,
    is_superuser
) VALUES ('u_sys_admin', 'sys_admin', 'mysql', 'root', NULL, 'password', 'mysql_emulated_default', 'engine_admin', TRUE);

INSERT INTO compat_identity_user_map_contract (
    canonical_userid,
    canonical_user,
    engine_scope,
    login_name,
    external_alias,
    auth_method,
    password_policy,
    permission_profile,
    is_superuser
) VALUES ('u_sys_admin', 'sys_admin', 'firebird', 'SYSDBA', NULL, 'password', 'firebird_emulated_default', 'engine_admin', TRUE);

INSERT INTO compat_identity_user_map_contract (
    canonical_userid,
    canonical_user,
    engine_scope,
    login_name,
    external_alias,
    auth_method,
    password_policy,
    permission_profile,
    is_superuser
) VALUES ('u_public_user', 'public_user', 'native', 'sb_public', NULL, 'password', 'native_v3_strict', 'public_only', FALSE);

INSERT INTO compat_identity_user_map_contract (
    canonical_userid,
    canonical_user,
    engine_scope,
    login_name,
    external_alias,
    auth_method,
    password_policy,
    permission_profile,
    is_superuser
) VALUES ('u_public_user', 'public_user', 'postgresql', 'pg_public', NULL, 'scram_sha_256', 'pg_emulated_default', 'public_only', FALSE);

INSERT INTO compat_identity_user_map_contract (
    canonical_userid,
    canonical_user,
    engine_scope,
    login_name,
    external_alias,
    auth_method,
    password_policy,
    permission_profile,
    is_superuser
) VALUES ('u_public_user', 'public_user', 'mysql', 'my_public', NULL, 'password', 'mysql_emulated_default', 'public_only', FALSE);

INSERT INTO compat_identity_user_map_contract (
    canonical_userid,
    canonical_user,
    engine_scope,
    login_name,
    external_alias,
    auth_method,
    password_policy,
    permission_profile,
    is_superuser
) VALUES ('u_public_user', 'public_user', 'firebird', 'fb_public', 'public.user', 'password', 'firebird_emulated_default', 'public_only', FALSE);

CREATE TABLE customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(96) NOT NULL,
    customer_tier VARCHAR(16) NOT NULL,
    active BOOLEAN NOT NULL
);

CREATE TABLE orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER NOT NULL,
    order_total DECIMAL(12,2) NOT NULL,
    order_status VARCHAR(24) NOT NULL
);

INSERT INTO customers (customer_id, customer_name, customer_tier, active)
VALUES (1, 'Alice Ng', 'gold', TRUE);

INSERT INTO customers (customer_id, customer_name, customer_tier, active)
VALUES (2, 'Bruno Hale', 'silver', TRUE);

INSERT INTO customers (customer_id, customer_name, customer_tier, active)
VALUES (3, 'Carmen Ives', 'bronze', FALSE);

INSERT INTO customers (customer_id, customer_name, customer_tier, active)
VALUES (4, 'Diego Wu', 'gold', TRUE);

INSERT INTO orders (order_id, customer_id, order_total, order_status)
VALUES (101, 1, 120.50, 'paid');

INSERT INTO orders (order_id, customer_id, order_total, order_status)
VALUES (102, 1, 75.00, 'pending');

INSERT INTO orders (order_id, customer_id, order_total, order_status)
VALUES (103, 2, 225.20, 'paid');

INSERT INTO orders (order_id, customer_id, order_total, order_status)
VALUES (104, 4, 19.99, 'shipped');

COMMIT;
