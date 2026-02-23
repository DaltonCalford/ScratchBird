#coding:utf-8

"""
ID:          issue-3149
ISSUE:       3149
TITLE:       SIMILAR TO works wrongly
DESCRIPTION:
JIRA:        CORE-2755
FBTEST:      bugs.core_2755
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """select
    case when 'ab' SIMILAR TO 'ab|cd|efg' then 'ok' else 'bad' end as ab,
    case when 'efg' SIMILAR TO 'ab|cd|efg' then 'ok' else 'bad' end as efg,
    case when 'a' SIMILAR TO 'ab|cd|efg' then 'bad' else 'ok' end as a
  from rdb$database;

"""

act = isql_act('db', test_script)

expected_stdout = """
AB     EFG    A
====== ====== ======
ok     ok     ok
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

