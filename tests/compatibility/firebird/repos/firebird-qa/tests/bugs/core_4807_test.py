#coding:utf-8

"""
ID:          issue-5105
ISSUE:       5105
TITLE:       Regression: List of aggregation is not checked properly
DESCRIPTION:
    Field inside subquery not present in GROUP BY clause and therefore can't be used in
    SELECT list as is (only as argument of some aggregation function).
JIRA:        CORE-4807
FBTEST:      bugs.core_4807
NOTES:
    [30.06.2025] pzotov
    Removed check of STDOUT: no sense in this test. Only STDERR must be checked.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;
    select t.rdb$field_name, (select 1 from rdb$database), count(*)
    from rdb$types t
    group by t.rdb$field_name;

    select t.rdb$field_name, (select 1 from rdb$database where t.rdb$system_flag=1), count(*)
    from rdb$types t
    group by t.rdb$field_name;
"""

act = isql_act('db', test_script, substitutions=[('SORT \\(\\(T NATURAL\\)\\)', 'SORT (T NATURAL)')])

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Invalid expression in the select list (not contained in either an aggregate function or the GROUP BY clause)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute(combine_output = False)
    assert act.clean_stderr == act.clean_expected_stderr
