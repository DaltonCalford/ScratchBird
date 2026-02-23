#coding:utf-8

"""
ID:          issue-6998
ISSUE:       6998
TITLE:       Problems with access to RDB$CONFIG table for non-privileged user when he has
  grant on execution of SP which has necessary access rights (created by SYSDBA with SQL DEFINER clause)
DESCRIPTION:
FBTEST:      bugs.gh_6998
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    create or alter user tmp$gh_6998 password '123' revoke admin role;
    commit;

    set term ^;
    create or alter procedure sp_check_rdb$config_avaliable
        returns (cnt int)
        sql security definer
    as
    begin
        select count(*) from rdb$config into cnt;
        suspend;
    end
    ^
    set term ;^
    commit;

    grant execute on procedure sp_check_rdb$config_avaliable to user tmp$gh_6998;
    commit;

    connect '$(DSN)' user tmp$gh_6998 password '123';
    commit;

    select current_user as who_am_i, sign(p.cnt) as is_rdb$config_avaliable
    from sp_check_rdb$config_avaliable p;
    commit;

    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$gh_6998;
    commit;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    WHO_AM_I                        TMP$GH_6998
    IS_RDB$CONFIG_AVALIABLE         1
"""

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
