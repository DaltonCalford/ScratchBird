#coding:utf-8

"""
ID:          issue-7476
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7476
TITLE:       Improvement on procedures/triggers to recognize changes in a domain used in COALESCE
DESCRIPTION:
NOTES:
    [02.10.2023] pzotov
    Confirmed bug on 5.0.0.1235

    NB: re-connect is needed, see line marked as "[ !! ]".
    Otherwise error raises: "SQLSTATE = 22001 / ... -string right truncation / -expected length 10, actual 11"
    Checked on 6.0.0.65.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    create domain dm_test as varchar(10);
    create table test1(s dm_test);
    create table test2(s dm_test);
    set term ^;
    create procedure sp_test as
    begin
        insert into test1(s) select coalesce(s, '') from test2;
    end
    ^
    set term ;^
    commit;

    alter domain dm_test type varchar(11);
    commit;

    insert into test2(s) values('1234567890A');
    commit;

    insert into test1(s) select coalesce(s, '') from test2;
    rollback;

    connect '$(DSN)'; ------------------- [ !! ]
    execute procedure sp_test;

    select * from test1;

    rollback;
"""

substitutions = [ ('[ \t]+', ' '), ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    S 1234567890A
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    act.expected_stdout = expected_stdout
    #act.isql(switches=['-q'], input = test_script, combine_output = True)
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
