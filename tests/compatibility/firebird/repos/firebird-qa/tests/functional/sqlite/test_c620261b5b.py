#coding:utf-8

"""
ID:          c620261b5b
ISSUE:       https://www.sqlite.org/src/tktview/c620261b5b
TITLE:       Incorrect result on query involving LEFT JOIN and transitive constraints
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int);
    create table t2(b int);
    create table t3(c integer primary key);
    insert into t1 values(1);
    insert into t3 values(1);
    set count on;
    select 'a row' as msg from t1 left join t2 on b=a join t3 on c=a;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG a row
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
