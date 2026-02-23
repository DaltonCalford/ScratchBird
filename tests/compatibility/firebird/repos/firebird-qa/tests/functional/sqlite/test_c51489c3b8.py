#coding:utf-8

"""
ID:          c51489c3b8
ISSUE:       https://www.sqlite.org/src/tktview/c51489c3b8
TITLE:       Incorrect result from WITH RECURSIVE using DISTINCT
DESCRIPTION:
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test (label varchar(10), step integer);
    insert into test values('a', 1);
    insert into test values('a', 1);
    insert into test values('b', 1);
    set count on;
    with recursive cte(label, step) as (
        select distinct * from test 
      union all 
        select label, step + 1 from cte where step < 3
    )
    select * from cte;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    LABEL a
    STEP 1

    LABEL a
    STEP 2

    LABEL a
    STEP 3

    LABEL b
    STEP 1

    LABEL b
    STEP 2

    LABEL b
    STEP 3

    Records affected: 6
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
