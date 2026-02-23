#coding:utf-8

"""
ID:          issue-1328
ISSUE:       1328
TITLE:       Grants don't work for procedures used inside views
DESCRIPTION:
JIRA:        CORE-927
FBTEST:      bugs.core_0927
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set wng off;
    set list on;

    -- Drop old account if it remains from prevoius run:
    set term ^;
    execute block as
    begin
        begin
        execute statement 'drop user tmp$c0927' with autonomous transaction;
            when any do begin end
        end
    end^
    set term ;^
    commit;

    create user tmp$c0927 password '123';
    commit;
    revoke all on all from tmp$c0927;
    commit;

    create or alter view v_test as select 1 id from rdb$database;
    commit;

    set term ^;
    create or alter procedure sp_test returns (result integer) as
    begin
        result = 1;
        suspend;
    end
    ^
    set term ;^
    commit;

    create or alter view v_test as
    select (select result from sp_test) as result from rdb$database;

    grant execute on procedure sp_test to view v_test;
    grant select on v_test to tmp$c0927;
    commit;

    -------------------------------------------------
    connect '$(DSN)' user 'tmp$c0927' password '123';
    -------------------------------------------------
    select current_user as who_am_i, v.* from v_test v;
    commit;

    connect '$(DSN)' user 'SYSDBA' password 'masterkey';

    drop user tmp$c0927;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    WHO_AM_I                        TMP$C0927
    RESULT                          1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

