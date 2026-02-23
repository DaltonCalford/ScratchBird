#coding:utf-8

"""
ID:          tabloid.core-3611-aux
TITLE:       Wrong data while retrieving from CTEs (or derived tables) with same column names
DESCRIPTION: 
  See another sample in this ticket (by dimitr, 30/Oct/12 07:13 PM)
FBTEST:      functional.tabloid.core_3611_aux
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;
    with tab as
    (
    select 1 as p1
    from rdb$relations
    )
    select f1.p1, f2.p1 as p2
    from tab f1 cross join tab f2
    group by f1.p1
    ;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Invalid expression in the select list (not contained in either an aggregate function or the GROUP BY clause)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
