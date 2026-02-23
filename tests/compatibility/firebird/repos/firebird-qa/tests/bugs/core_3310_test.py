#coding:utf-8

"""
ID:          issue-3677
ISSUE:       3677
TITLE:       RDB$GET_CONTEXT and between in view
DESCRIPTION:
JIRA:        CORE-3310
FBTEST:      bugs.core_3310
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='employee-ods12.fbk')

test_script = """
    create or alter view v_test
    as
    select s.po_number
    from sales s
    where
      cast(coalesce(rdb$get_context('USER_SESSION', 'SELECTED_DATE'), '12.12.1993') as timestamp)
      between
      s.order_date and s.date_needed
    ;
    set list on;
    select * from v_test v order by v.po_number rows 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    PO_NUMBER                       V9320630
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

