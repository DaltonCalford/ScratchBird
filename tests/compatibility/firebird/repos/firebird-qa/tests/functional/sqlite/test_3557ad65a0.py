#coding:utf-8

"""
ID:          3557ad65a0
ISSUE:       https://www.sqlite.org/src/tktview/3557ad65a0
TITLE:       Incorrect DISTINCT on an indexed query with IN
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int, b int);
    insert into t1 values(1,1);
    insert into t1 values(2,1);
    insert into t1 values(3,1);
    insert into t1 values(2,2);
    insert into t1 values(3,2);
    insert into t1 values(4,2);
    commit;
    create index t1ab on t1(a,b);

    set count on;
    select distinct b from t1 where a in (1,2,3);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    B 1
    B 2
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
