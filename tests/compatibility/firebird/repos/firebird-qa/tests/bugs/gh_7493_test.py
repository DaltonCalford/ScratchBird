#coding:utf-8

"""
ID:          issue-7493
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7493
TITLE:       Firebird 5.0 snapshot blob not found error after first commit / rollback
NOTES:
    [04.03.2023] pzotov
    Checked on 5.0.0.970.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain xszovegn as blob sub_type 1 segment size 80 not null;
    create global temporary table test (txt xszovegn) on commit delete rows;

    set term ^;
    create or alter trigger test_bi1 for test active before insert position 1 as
    begin
        if (char_length(new.txt) > 8000) then
        begin
            -- nop --
        end
    end
    ^
    set term ;^
    commit;

    insert into test(txt) values ('Something');
    insert into test(txt) values ('Something');
    insert into test(txt) values ('Something');
    insert into test(txt) values ('Something');
    rollback; -- or commit

    insert into test(txt) values ('Something');
"""

act = isql_act('db', test_script)

expected_stdout = """
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
