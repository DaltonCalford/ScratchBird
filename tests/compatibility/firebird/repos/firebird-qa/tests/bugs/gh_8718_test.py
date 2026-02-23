#coding:utf-8

"""
ID:          issue-8718
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8718
TITLE:       Incorrect result using UNLIST and 2 CTE
NOTES:
    [01.09.2025] pzotov
    Confirmed bug on 6.0.0.1244
    Checked on 6.0.0.1261-8d5bb71.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    with
      t(n) as (
         select n from unlist('0,1,2,3,4,5,6,7,8,9' returning int) as u(n)
      ),
      t2(n) as (
        select 100000 * t1.n + 10000 * t2.n + 1000 * t3.n + 100 * t4.n + 10 * t5.n + t6.n
        from t t1, t t2, t t3, t t4, t t5, t t6
      )
    select sum(n) as sum_n, count(*) as cnt_n from t2;
"""
substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        SUM_N 499999500000
        CNT_N 1000000
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
