#coding:utf-8

"""
ID:          issue-4777
ISSUE:       4777
TITLE:       DATEADD should support fractional value for MILLISECOND
DESCRIPTION:
JIRA:        CORE-4457
FBTEST:      bugs.core_4457
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select cast(dateadd(-1 * extract(millisecond from ts) millisecond to ts) as varchar(30)) dts, extract(millisecond from ts) ms
    from (
        select timestamp'2014-06-09 13:50:17.4971' as ts
        from rdb$database
    ) a;
"""

act = isql_act('db', test_script)

expected_stdout = """
    DTS                             2014-06-09 13:50:17.0000
    MS                              497.1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

