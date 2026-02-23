#coding:utf-8

"""
ID:          issue-6033
ISSUE:       6033
TITLE:       User who is allowed to manage other users must have this ability WITHOUT need to grant him RDB$ADMIN role
DESCRIPTION:
JIRA:        CORE-5770
FBTEST:      bugs.core_5770
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set wng off;

    recreate view v_sec as
    select
        current_user as who_am_i
        ,s.sec$user_name
        ,s.sec$active
        ,s.sec$admin
        ,s.sec$plugin
    from sec$users s
    where upper(sec$user_name) = upper('tmp$c5770_bar');
    commit;
    grant select on v_sec to public;
    commit;

    create or alter user tmp$c5770_foo password '123' using plugin Srp grant admin role;
    create or alter user tmp$c5770_bar password '456' inactive using plugin Srp;
    commit;
    revoke all on all from tmp$c5770_foo;
    revoke all on all from tmp$c5770_bar;
    commit;

    connect '$(DSN)' user tmp$c5770_foo password '123';
    --select current_user as who_am_i from rdb$database;
    commit;

    -- check that sub-admin user 'foo' can make common user 'bar' ACTIVE:
    alter user tmp$c5770_bar active using plugin Srp;
    commit;
    select * from v_sec;
    commit;

    connect '$(DSN)' user tmp$c5770_bar password '456'; -- should PASS because he is ACTIVE now
    select current_user as who_am_i from rdb$database;
    commit;

    connect '$(DSN)' user tmp$c5770_foo password '123';

    -- check that sub-admin user 'foo' can make common user 'bar' INACTIVE:
    alter user tmp$c5770_bar inactive using plugin Srp;
    commit;
    select * from v_sec;
    commit;

    -- this should raise SQLSTATE = 28000: user 'tmp$c5770_bar' again become inactive:
    connect '$(DSN)' user tmp$c5770_bar password '456';
    select current_user as who_am_i from rdb$database;
    commit;

    -- cleanup: drop foo and bar.
    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c5770_foo using plugin Srp;
    drop user tmp$c5770_bar using plugin Srp;
    commit;
"""

act = isql_act('db', test_script, substitutions=[('Use CONNECT or CREATE DATABASE.*', '')])

expected_stdout = """
    WHO_AM_I                        TMP$C5770_FOO
    SEC$USER_NAME                   TMP$C5770_BAR
    SEC$ACTIVE                      <true>
    SEC$ADMIN                       <false>
    SEC$PLUGIN                      Srp

    WHO_AM_I                        TMP$C5770_BAR

    WHO_AM_I                        TMP$C5770_FOO
    SEC$USER_NAME                   TMP$C5770_BAR
    SEC$ACTIVE                      <false>
    SEC$ADMIN                       <false>
    SEC$PLUGIN                      Srp
"""

expected_stderr = """
    Statement failed, SQLSTATE = 28000
    Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
