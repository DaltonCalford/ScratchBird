#coding:utf-8

"""
ID:          f7f8c97e97
ISSUE:       https://www.sqlite.org/src/tktview/f7f8c97e97
TITLE:       Valid query fails to compile due to WHERE clause optimization
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
    create table t1(a int, b int);
    insert into t1 values(1,2);
    insert into t1 values(1,18);
    insert into t1 values(2,19);

    set count on;
    select x, y from (
      select a as x, sum(b) as y from t1 group by a
      UNION
      select 98, 99 from rdb$database
    ) as w where y>=20;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 1
    Y 20
    X 98
    Y 99
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
