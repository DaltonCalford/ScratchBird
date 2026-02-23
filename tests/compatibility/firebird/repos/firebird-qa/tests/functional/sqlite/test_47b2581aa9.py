#coding:utf-8

"""
ID:          47b2581aa9
ISSUE:       https://www.sqlite.org/src/tktview/47b2581aa9
TITLE:       Infinite loop on UPDATE
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    3.x and 4.x do not display 'Records affected: 1'. Test verifies only FB 5.x+
    Checked on 6.0.0.1204, 5.0.4.1701.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int, b int);
    create index t1b on t1(a);
    create index t1c on t1(b);
    insert into t1 values(1,2);
    set count on;
    update t1 set a = a + 2, b = a + 2 where a > 0 or b > 0 returning old.a, old.b, new.a, new.b;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    B 2
    A 3
    B 3
    Records affected: 1
"""

@pytest.mark.version('>=5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
