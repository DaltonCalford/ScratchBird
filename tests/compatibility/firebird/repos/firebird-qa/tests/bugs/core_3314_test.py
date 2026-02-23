#coding:utf-8

"""
ID:          issue-3681
ISSUE:       3681
TITLE:       Dependencies are not removed after dropping the procedure and the table it
  depends on in the same transaction
DESCRIPTION:
JIRA:        CORE-3314
FBTEST:      bugs.core_3314
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

init_script = """
    create table test (a int);
    set term ^;
    create procedure p as
    begin
        delete from test;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set count on;
    select 1 as point_1 from rdb$dependencies where rdb$depended_on_name = upper('test');
    drop procedure p;
    drop table test;
    commit;
    select 1 as point_2 from rdb$dependencies where rdb$depended_on_name = upper('test');
"""

act = isql_act('db', test_script)

expected_stdout = """
    POINT_1                         1
    Records affected: 1
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

