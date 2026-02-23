#coding:utf-8

"""
ID:          issue-6724
ISSUE:       6724
TITLE:       Inconsistent translation "string->timestamp->string->timestamp" in dialect 1
DESCRIPTION:
JIRA:        CORE-6494
FBTEST:      bugs.core_6494
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    set heading off;
    select cast(cast(cast(cast('2-dec-0083' as timestamp) as varchar(64))as timestamp)as varchar(64)) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    02-DEC-0083
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
