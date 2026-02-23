#coding:utf-8

"""
ID:          ba7cbfaedc
ISSUE:       https://www.sqlite.org/src/tktview/ba7cbfaedc
TITLE:       Wrong output order when a DESC index and GROUP BY and ORDER BY.
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
    create table t1(x int);
    insert into t1 values(1);
    insert into t1 values(2);
    commit;
    create descending index ix1 on t1(x);
    set count on;
    select * from t1 group by x order by x;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 1
    X 2
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
