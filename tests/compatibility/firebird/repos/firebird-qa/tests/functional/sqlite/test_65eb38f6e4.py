#coding:utf-8

"""
ID:          65eb38f6e4
ISSUE:       https://www.sqlite.org/src/tktview/65eb38f6e4
TITLE:       Incorrect answer on LEFT JOIN when STAT4 is enabled
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int);
    create index t1a on t1(a);
    insert into t1(a) values(null);
    insert into t1(a) values(null);
    insert into t1(a) values(42);
    insert into t1(a) values(null);
    insert into t1(a) values(null);

    create table t2(dummy int);
    set count on;
    select count(*) from t1 left join t2 on a is not null;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 5
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
