#coding:utf-8

"""
ID:          issue-6121
ISSUE:       6121
TITLE:       GRANT OPTION is not checked for new object
DESCRIPTION:
JIRA:        CORE-5861
FBTEST:      bugs.core_5861
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

user_1 = user_factory('db', name='tmp$c5861_u1', password='pass')
user_2 = user_factory('db', name='tmp$c5861_u2', password='pass')
user_3 = user_factory('db', name='tmp$c5861_u3', password='pass')

role_1 = role_factory('db', name='role1')
role_2 = role_factory('db', name='role2')
role_3 = role_factory('db', name='role3')

test_script = """
    set bail on;
    set list on;
/*
    create or alter user tmp$c5861_u1 password 'pass';
    create or alter user tmp$c5861_u2 password 'pass';
    create or alter user tmp$c5861_u3 password 'pass';

    create role role1; -- Has privileges with grant option
    create role role2; -- Has privileges without errors and without grant option
    create role role3; -- Must get errors in granting privileges from role2
*/
    grant role1 to tmp$c5861_u1;
    grant role2 to tmp$c5861_u2;
    grant role3 to tmp$c5861_u3;

    create procedure p as begin end;
    create function f returns int as begin end;
    create generator g;
    create exception e 'ex';
    create table tab(id int);
    create package pak as begin end;

    grant create table to role1 with grant option;
    grant create procedure to role1 with grant option;
    grant execute on procedure p to role1 with grant option;
    grant execute on function f to role1 with grant option;
    grant usage on generator g to role1 with grant option;
    grant usage on exception e to role1 with grant option;
    grant select on tab to role1 with grant option;
    grant update(id) on tab to role1 with grant option;
    grant execute on package pak to role1 with grant option;

    commit;

    connect '$(DSN)' user 'tmp$c5861_u1' password 'pass' role 'role1';

    select rdb$role_name from rdb$roles where rdb$role_in_use(rdb$role_name);

    grant create table to role2;
    grant execute on procedure p to role2;
    grant execute on function f to role2;
    grant usage on generator g to role2;
    grant usage on exception e to role2;
    grant select on tab to role2;
    grant update(id) on tab to role2;
    grant execute on package pak to role2;

    commit;

    -- create own objects
    create table tab_of_tmp$c5861_u1(i int);
    create procedure proc_of_tmp$c5861_u1 as begin end;

    commit;

    -- try to grant privileges for owned objects
    grant select on table tab_of_tmp$c5861_u1 to role2;
    grant execute on procedure proc_of_tmp$c5861_u1 to role2;

    commit;

    connect '$(DSN)' user 'tmp$c5861_u2' password 'pass' role 'role2';

    -- check every privilege
    create table t(i integer);
    execute procedure p;
    select f() from rdb$database;
    select gen_id(g, 1) from rdb$database;
    select * from tab;

    -- try to grant every privilege to role3 and sure this causes an error
    set bail off;

    ------------------------------------------------
    grant create table to role3;
    ------------------------------------------------
    grant execute on procedure p to role3;
    ------------------------------------------------
    grant execute on function f to role3;
    ------------------------------------------------
    grant usage on generator g to role3;
    ------------------------------------------------
    grant usage on exception e to role3;
    ------------------------------------------------
    grant select on tab to role3;
    ------------------------------------------------
    grant update(id) on tab to role3;
    ------------------------------------------------
    grant execute on package pak to role3;

    --- cleanup:
    commit;
/*
    connect '$(DSN)' user 'SYSDBA' password 'masterkey';

    drop user tmp$c5861_u1;
    drop user tmp$c5861_u2;
    drop user tmp$c5861_u3;
    commit;*/
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    RDB$ROLE_NAME                   ROLE1
    F                               <null>
    GEN_ID                          1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no CREATE privilege with grant option on DDL SQL$TABLES
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no EXECUTE privilege with grant option on object P
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no EXECUTE privilege with grant option on object F
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object G
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object E
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no grant option for privilege SELECT on table/view TAB
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no grant option for privilege UPDATE on column ID of table/view TAB
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no EXECUTE privilege with grant option on object PAK
"""

expected_stdout_6x = """
    RDB$ROLE_NAME                   ROLE1
    F                               <null>
    GEN_ID                          1
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no CREATE privilege with grant option on DDL "PUBLIC"."SQL$TABLES"
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no EXECUTE privilege with grant option on object "PUBLIC"."P"
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no EXECUTE privilege with grant option on object "PUBLIC"."F"
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object "PUBLIC"."G"
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no USAGE privilege with grant option on object "PUBLIC"."E"
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no grant option for privilege SELECT on table/view "PUBLIC"."TAB"
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no grant option for privilege UPDATE on column "ID" of table/view "PUBLIC"."TAB"
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -GRANT failed
    -no EXECUTE privilege with grant option on object "PUBLIC"."PAK"
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, user_1, user_2, user_3, role_1, role_2, role_3):

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
