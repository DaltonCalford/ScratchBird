#coding:utf-8

"""
ID:          issue-4817
ISSUE:       4817
TITLE:       FB 3.0 crashes when getting an explained plan for a DBKEY based retrieval
DESCRIPTION:
JIRA:        CORE-4498
FBTEST:      bugs.core_4498
NOTES:
    [29.06.2025] pzotov
    Added subst to suppress displaying name of table: on 6.x it is prefixed by SQL schema and enclosed in quotes.
    For this test it is enough just to show proper starting part of line with explained plan and check that no error occurs.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
  set explain;
  select 1 from rdb$relations where rdb$db_key = cast('1234' as char(8) character set octets);
"""

act = isql_act('db', test_script, substitutions = [('Table .*', 'Table')])

expected_stdout = """
  Select Expression
        -> Filter
            -> Table "RDB$RELATIONS" Access By ID
                -> DBKEY
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

