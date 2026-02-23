#coding:utf-8

"""
ID:          issue-6807
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6807
TITLE:       Regression in FB 4.x : "Unexpected end of command" with incorrect line/column info
NOTES:
    [25.02.2023] pzotov
    COncrete values of line/number are ignored.
    We pay attention only for these values greater than 0 (see substitution part: "line [1-9]+, column [1-9]+")
    Checked on 5.0.0.959
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(id int);
    insert into test(id) values(null);
    set term ^;
    execute block as
        declare v_id int;
    begin
        update test set id = -id returning id as v_id;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions = [('^((?!Unexpected end of command - line [1-9]+, column [1-9]+|SQLSTATE).)*$', ''), ('[ \t]+', ' '), ('line [1-9]+, column [1-9]+', 'line, column')])

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    -Unexpected end of command - line 4, column 44
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
