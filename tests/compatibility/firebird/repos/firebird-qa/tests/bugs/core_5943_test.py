#coding:utf-8

"""
ID:          issue-6199
ISSUE:       6199
TITLE:       Server crashes preparing a query with both DISTINCT/ORDER BY and non-field expression in the select list
DESCRIPTION:
  We run query from ticket and check that it does completed OK with issuing data and 'Records affected: 1'.
JIRA:        CORE-5943
FBTEST:      bugs.core_5943
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    select distinct
        '0' as f01
       ,a.mon$server_pid as f02
    from mon$attachments a
    order by a.mon$server_pid, a.mon$server_pid ;
"""

act = isql_act('db', test_script, substitutions=[('F02\\s+\\d+', 'F02')])

expected_stdout = """
    F01                             0
    F02                             2344
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
