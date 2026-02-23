#coding:utf-8

"""
ID:          issue-3272
ISSUE:       3272
TITLE:       A memory corruption cause incorrect query evaluation and may crash the server
DESCRIPTION:
JIRA:        CORE-2888
FBTEST:      bugs.core_2888
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select 1 from rdb$database where 1 in (select (select current_connection from rdb$database) from rdb$database);
select 1 from rdb$database where 1 in (select (select 1 from rdb$database) from rdb$database);
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT
============
           1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

