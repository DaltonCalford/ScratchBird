#coding:utf-8

"""
ID:          issue-5368
ISSUE:       5368
TITLE:       Server does not validate correctness of user/password pair provided in EXECUTE STATEMENT operator
DESCRIPTION:
  Confirmed on build 32136 (RC1), build 32181: wrongly displayed names of non-existent user 'TMP$C5082'
  and user SYDSDBA that login was provided with wrong (but NOT empty) password.
  NOTE: check does NOT perform inside ES when password is empty string or any number of ascii_char(32)
  and 'new' user specified is equal to user name that was used to login - see examples here when password
  for SYSDBA is '' or ascii_char(32).
NOTES:
[06.02.2019] added separate code for 4.0 because of new error message ("Missing security context for ...")
JIRA:        CORE-5082
FBTEST:      bugs.core_5082
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Make sure that user 'TMP$C5082' does NOT exist at the point just before this test start:
    create or alter user tmp$c5082 password '123';
    commit;
    drop user tmp$c5082;
    commit;

    set list on;
    set term ^;
    execute block returns (whoami varchar(32)) as
    begin
        execute statement 'select current_user from rdb$database' as user 'TMP$C5082' password 'qwecXzasd' into whoami;
        suspend;
    end
    ^

    execute block returns (whoami varchar(32)) as
    begin
        execute statement 'select current_user from rdb$database' as user 'TMP$C5082' password '' into whoami;
        suspend;
    end
    ^

    execute block returns (whoami varchar(32)) as
    begin
        execute statement 'select current_user from rdb$database' as user 'SYSDBA' password 'fullyWrong' into whoami;
        suspend;
    end
    ^

    execute block returns (whoami varchar(32)) as
    begin
        execute statement 'select current_user from rdb$database' as user 'SYSDBA' password '' into whoami;
        suspend;
    end
    ^

    execute block returns (whoami varchar(32)) as
    begin
        execute statement 'select current_user from rdb$database' as user 'SYSDBA' password ' ' into whoami;
        suspend;
    end
    ^

    execute block returns (whoami varchar(32)) as
    begin
        execute statement 'select current_user from rdb$database' as user 'SYSDBA' password '	' into whoami;
        --                                                                                   ^- Tab character here!
        suspend;
    end
    ^

"""

expected_stdout = """
    WHOAMI                          SYSDBA
    WHOAMI                          SYSDBA
"""

# version: 3.0

act_1 = isql_act('db', test_script, substitutions=[('-At block line: [\\d]+, col: [\\d]+', '-At block line')])

expected_stderr_1 = """
    Statement failed, SQLSTATE = 28000
    Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
    -At block line: 3, col: 9

    Statement failed, SQLSTATE = 28000
    Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
    -At block line: 3, col: 9

    Statement failed, SQLSTATE = 28000
    Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
    -At block line: 3, col: 9

    Statement failed, SQLSTATE = 28000
    Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
    -At block line: 3, col: 9
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout
    act_1.expected_stderr = expected_stderr_1
    act_1.execute()
    assert (act_1.clean_stderr == act_1.clean_expected_stderr and
            act_1.clean_stdout == act_1.clean_expected_stdout)

# version: 4.0

act_2 = isql_act('db', test_script,
                 substitutions=[('-At block line: [\\d]+, col: [\\d]+', ''),
                                ('Missing security context.*', ''),
                                ('Your user name and password are not defined.*', '')])

expected_stderr_2 = """
    Statement failed, SQLSTATE = 28000

    Statement failed, SQLSTATE = 28000

    Statement failed, SQLSTATE = 28000

    Statement failed, SQLSTATE = 28000
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout
    act_2.expected_stderr = expected_stderr_2
    act_2.execute()
    assert (act_2.clean_stderr == act_2.clean_expected_stderr and
            act_2.clean_stdout == act_2.clean_expected_stdout)

