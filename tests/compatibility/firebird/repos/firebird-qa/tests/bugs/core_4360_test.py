#coding:utf-8

"""
ID:          issue-4682
ISSUE:       4682
TITLE:       SELECT from derived table which contains GROUP BY on field with literal value returns wrong result
DESCRIPTION:
JIRA:        CORE-4360
FBTEST:      bugs.core_4360
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
select c from( select 'a' c from rdb$database group by 'a' );
select c from( select 123 c from rdb$database group by 1 );
select c from( select dateadd(999 millisecond to timestamp '31.12.9999 23:59:59') c from rdb$database group by 1 );
"""

act = isql_act('db', test_script)

expected_stdout = """
C
======
a
           C
============
         123
                        C
=========================
9999-12-31 23:59:59.9990
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

