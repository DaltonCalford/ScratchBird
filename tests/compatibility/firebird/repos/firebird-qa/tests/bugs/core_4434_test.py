#coding:utf-8

"""
ID:          issue-4754
ISSUE:       4754
TITLE:       Extend the use of colon prefix for read/assignment OLD/NEW fields and assignment to variables
DESCRIPTION:
JIRA:        CORE-4434
FBTEST:      bugs.core_4434
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t1(x int, n int);
    commit;
    insert into t1(x) values(777);
    commit;

    set term ^;
    create trigger t1_bu before update on t1 as
        declare v int;
    begin
        :v = :old.x * 2;
        :new.n = :v;
    end
    ^
    set term ;^
    commit;

    set list on;
    update t1 set x = -x rows 1;
    select * from t1;

"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               -777
    N                               1554
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

