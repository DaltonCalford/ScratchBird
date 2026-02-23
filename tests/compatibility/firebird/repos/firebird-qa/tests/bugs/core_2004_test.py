#coding:utf-8

"""
ID:          issue-2441
ISSUE:       2441
TITLE:       ALTER USER XXX INACTIVE
DESCRIPTION:
  We create two users ('foo' and 'bar') and make them immediatelly INACTIVE.
  One of them has been granted with RDB$ADMIN role, so he will be able to manage of other user access.
  Then we chek then connect for one of these users (e.g., 'foo') is unable because of his inactive status.
  After this we change state of FOO to active and verify that he can make connect.
  When this user successfully establish connect, he will try to :
  * create and immediatelly drop new user ('rio');
  * change state of other existing user ('bar') to active.
  Finally, we check that user 'bar' really can connect now (after he was allowed to do this by 'foo').
NOTES:
  FB config parameters AuthClient and UserManager must contain 'Srp' plugin in their values.
JIRA:        CORE-2004
FBTEST:      bugs.core_2004
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter view v_check as
    select s.sec$user_name, s.sec$active, s.sec$plugin
    from rdb$database r
    left join sec$users s on lower(s.sec$user_name) in (lower('tmp$c2004_foo'), lower('tmp$c2004_bar'), lower('tmp$c2004_rio'))
    ;
"""

db = db_factory(init=init_script)

user_foo = user_factory('db', name='tmp$c2004_foo', password='123', plugin='Srp', active=False, admin=True)
user_bar = user_factory('db', name='tmp$c2004_bar', password='456', plugin='Srp', active=False)

test_script = """
    set list on;

    -- NB: currently it seems strange that one need to grant rdb$admin to 'foo'
    -- For what reason this role need to be added if 'foo' does his actions only in security_db ?
    -- Sent letter to dimitr and alex, 10-mar-18 16:00
    grant rdb$admin to tmp$c2004_foo;
    commit;

    set count on;
    select 'init_state' as msg, v.* from v_check v;
    set count off;

    select 'try to connect as INACTIVE users' as msg from rdb$database;
    commit;

    connect '$(DSN)' user tmp$c2004_foo password '123'; -- should fail
    select current_user as who_am_i from rdb$database;
    rollback;

    connect '$(DSN)' user tmp$c2004_bar password '456'; -- should fail
    select current_user as who_am_i from rdb$database;
    rollback;

    connect '$(DSN)' user SYSDBA password 'masterkey';


    -- NB: following "alter user" statement must contain "using plugin Srp" clause
    -- otherwise get:
    --    Statement failed, SQLSTATE = HY000
    --    record not found for user: TMP$C2004_BAR

    alter user tmp$c2004_foo active using plugin Srp;
    select 'try to connect as user FOO which was just set as active by SYSDBA.' as msg from rdb$database;
    commit;

    connect '$(DSN)' user tmp$c2004_foo password '123' role 'RDB$ADMIN'; -- should pass
    select current_user as who_am_i, current_role as whats_my_role from rdb$database;


     -- should pass because foo has admin role:
    create or alter user tmp$c2004_rio password '123' using plugin Srp;
    drop user tmp$c2004_rio using plugin Srp;

     -- should pass because foo has admin role:
    alter user tmp$c2004_bar active using plugin Srp;
    select 'try to connect as user BAR which was just set as active by FOO.' as msg from rdb$database;
    commit;

    connect '$(DSN)' user tmp$c2004_bar password '456'; -- should pass
    select current_user as who_am_i from rdb$database;
    commit;
"""

act = isql_act('db', test_script,
               substitutions=[('Use CONNECT or CREATE DATABASE.*', ''), ('.*After line.*', '')])

expected_stdout = """
MSG                             init_state
SEC$USER_NAME                   TMP$C2004_FOO
SEC$ACTIVE                      <false>
SEC$PLUGIN                      Srp
MSG                             init_state
SEC$USER_NAME                   TMP$C2004_BAR
SEC$ACTIVE                      <false>
SEC$PLUGIN                      Srp
Records affected: 2
MSG                             try to connect as INACTIVE users
MSG                             try to connect as user FOO which was just set as active by SYSDBA.
WHO_AM_I                        TMP$C2004_FOO
WHATS_MY_ROLE                   RDB$ADMIN
MSG                             try to connect as user BAR which was just set as active by FOO.
WHO_AM_I                        TMP$C2004_BAR
"""

expected_stderr = """
Statement failed, SQLSTATE = 28000
Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
Statement failed, SQLSTATE = 28000
Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
After line 19 in file tmp_check_2004.sql
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, user_foo: User, user_bar: User):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
