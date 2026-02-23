#coding:utf-8

"""
ID:          issue-5859
ISSUE:       5859
TITLE:       System function RDB$ROLE_IN_USE cannot take long role names
DESCRIPTION:
  Test creates several roles and grants them to user.
  Role #1 is common ascii string without spaces.
  Role #2 is ascii string with spaces, all character are written in UPPERCASE.
  Role #3 is ascii string with spaces, all character are written in lowercase.

  After make connection to database using role #3 we have to check that this role
  indeed was applied to connection and that rdb$role_in_use *does* return this role name also.
  See also: github.com/FirebirdSQL/firebird/commit/61b4bf0d276884a0ba0f2edef3b41fa2c58c87ee
  Comments there:
     - roleStr.upper();
     + //roleStr.upper();		// sorry - but this breaks role names containing lower case letters


  ::: NB CAUTION ::: TEMPLY DEFERRED :::
  We also have to check cases when role contains NON-ASCII characters but currently this is impossible.
  Exception like:
  ===
   Statement failed, SQLSTATE = 22001
   arithmetic exception, numeric overflow, or string truncation
   -string right truncation
   -expected length 63, actual 79
  ===
  - is raised when non-ascii role contains only ~40 *characters* and connection charset = UTF8.
  Sent letter to Alex, 23.06.2018 09:21, waiting for reply.
JIRA:        CORE-5593
FBTEST:      bugs.core_5593
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set names utf8;
    set wng off;
    set list on;
    set bail on;

    create or alter user tmp$c5593 password '123' revoke admin role;
    commit;

    --revoke all on all from tmp$c5593; --> led to crash on 4.0.0.1036
    --commit;

    create or alter view v_role_info as
    select
        current_user as who_am_i,
        current_role as my_current_role,
        rdb$role_name as r_name,
        rdb$owner_name r_owner,
        rdb$role_in_use(rdb$role_name) r_in_use
    from rdb$roles
    where coalesce(rdb$system_flag,0) = 0
    ;
    commit;

    create role chief;
    create role "CHIEF OF TRANSPORT AND LOGISTICS DEPARTMENT";
    create role "chief of financial and accounting department";
    commit;

    grant chief to user tmp$c5593;
    grant "CHIEF OF TRANSPORT AND LOGISTICS DEPARTMENT" to user tmp$c5593;
    grant "chief of financial and accounting department" to user tmp$c5593;
    commit;
    --show grants;
    grant select on v_role_info to user tmp$c5593;
    commit;

    --set echo on;

    connect '$(DSN)'
        user tmp$c5593
        password '123'
        role chief
    ;
    select * from v_role_info where my_current_role = r_name;
    commit;

    connect '$(DSN)'
        user tmp$c5593
        password '123'
        role "CHIEF OF TRANSPORT AND LOGISTICS DEPARTMENT"
    ;
    select * from v_role_info where my_current_role = r_name;
    commit;

    connect '$(DSN)'
        user tmp$c5593
        password '123'
        role "chief of financial and accounting department"
    ;

    -- ###########################################
    -- Before fix this statement returned FALSE as
    -- result of rdb$role_in_use(rdb$role_name)
    -- ###########################################
    select * from v_role_info where my_current_role = r_name;
    commit;

    -- cleanup
    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c5593;
    commit;

"""

act = isql_act('db', test_script)

expected_stdout = """
    WHO_AM_I                        TMP$C5593
    MY_CURRENT_ROLE                 CHIEF
    R_NAME                          CHIEF
    R_OWNER                         SYSDBA
    R_IN_USE                        <true>

    WHO_AM_I                        TMP$C5593
    MY_CURRENT_ROLE                 CHIEF OF TRANSPORT AND LOGISTICS DEPARTMENT
    R_NAME                          CHIEF OF TRANSPORT AND LOGISTICS DEPARTMENT
    R_OWNER                         SYSDBA
    R_IN_USE                        <true>

    WHO_AM_I                        TMP$C5593
    MY_CURRENT_ROLE                 chief of financial and accounting department
    R_NAME                          chief of financial and accounting department
    R_OWNER                         SYSDBA
    R_IN_USE                        <true>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

