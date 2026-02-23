#coding:utf-8

"""
ID:          f617ea3125
ISSUE:       https://www.sqlite.org/src/tktview/f617ea3125
TITLE:       Incorrect ORDER BY with colliding input and output column names
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
    create table t1(m char(2));
    insert into t1 values('az');
    insert into t1 values('by');
    insert into t1 values('cx');
    set count on;
    select '1' as msg, substring(m from 2) as m from t1 order by m;
    select '2' as msg, cast(substring(m from 2) as varchar(2) character set octets) as m from t1 order by m;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG 1
    M x
    MSG 1
    M y
    MSG 1
    M z
    Records affected: 3

    MSG 2
    M 78
    MSG 2
    M 79
    MSG 2
    M 7A
    Records affected: 3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
