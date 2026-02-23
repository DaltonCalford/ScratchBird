#coding:utf-8

"""
ID:          issue-7391
ISSUE:       7391
TITLE:       AV when compiling stored procedure
NOTES:
    [16.02.2023] pzotov
    Confirmed bug on 5.0.0.843
    Checked on 5.0.0.938 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;

    set autoddl off;
    commit;

    set term ^;
    create or alter procedure list_to_rows(i_list varchar(1024)) returns (o_id int) as
    begin
        suspend;
    end
    ^
    commit
    ^
    create or alter procedure sp_bug (i_list varchar(1024)) as
        declare id int;
    begin
        rdb$set_context('USER_SESSION','PROC_COMPLETED',1);
        if (not (:id in (select o_id from list_to_rows(:i_list)))) then
            exit;
    end
    ^
    commit
    ^
    set term ;^
    execute procedure sp_bug('foo,list,bar');
    select rdb$get_context('USER_SESSION','PROC_COMPLETED') as PROC_COMPLETED from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    PROC_COMPLETED                  1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
