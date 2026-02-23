#coding:utf-8

"""
ID:          issue-4948
ISSUE:       4948
TITLE:       Regression: ORDER BY via an index + WHERE clause: error "no current record for fetch operation"
DESCRIPTION:
JIRA:        CORE-4634
FBTEST:      bugs.core_4634
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='employee-ods12.fbk')

test_script = """
    -- Confirmed for WI-T3.0.0.31374 Firebird 3.0 Beta 1:
    -- Statement failed, SQLSTATE = 22000
    -- no current record for fetch operation
    set list on;
    select *
    from sales
    where (order_status like '1%' or order_status like '%1%')
    order by order_status
    rows 1;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
