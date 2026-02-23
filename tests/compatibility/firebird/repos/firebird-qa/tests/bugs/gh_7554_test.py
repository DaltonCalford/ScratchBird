#coding:utf-8

"""
ID:          issue-7554
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7554
TITLE:       Firebird 5 partial index creation causes server hang up
DESCRIPTION:
NOTES:
    [04.06.2023] pzotov
    Confirmed bugcheck on 5.0.0.1049: second execution of test SQL leads to:
    internal Firebird consistency check (invalid SEND request (167), file: Statement.cpp line: 405)
    Checked on 5.0.0.1050: all OK.
"""

import pytest
from firebird.qa import *

N_ROWS = 100000
init_sql = """
    create table tbl(num int);
    set term ^;
    create procedure sp_add_rows(a_cnt int not null) as
        declare i int = 0;
    begin
        while (:i < :a_cnt) do
        begin
            insert into tbl(num) values (0);
            i = i + 1;
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init = init_sql)

test_script = f"""
    set autoddl off;
    delete from tbl;
    execute procedure sp_add_rows( {N_ROWS} );
    create index tbl_p1 on tbl (num) where num > 0;
    commit;
    drop index tbl_p1;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    act.execute(combine_output = True) # <<< THIS CAUSED BUGCHECK BEFORE FIX
    assert act.clean_stdout == act.clean_expected_stdout

