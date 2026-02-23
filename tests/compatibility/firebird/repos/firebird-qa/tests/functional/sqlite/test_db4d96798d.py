#coding:utf-8

"""
ID:          db4d96798d
ISSUE:       https://www.sqlite.org/src/tktview/db4d96798d
TITLE:       LIMIT does not work with nested views containing UNION ALL
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
    insert into t1 values(5);
    create view v1 as select x*2 y from t1;
    create view v2 as select * from v1 union all select * from v1;
    create view v4 as select * from v2 union all select * from v2;

    set count on;
    select * from v4 rows 3;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Y 10
    Y 10
    Y 10
    Records affected: 3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
