#coding:utf-8

"""
ID:          issue-6283
ISSUE:       6283
TITLE:       SUBSTRING(CURRENT_TIMESTAMP) does not work
DESCRIPTION:
JIRA:        CORE-6033
FBTEST:      bugs.core_6033
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
        declare c varchar(100);
    begin
        c = substring(current_timestamp from 1);
    end
    ^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.execute()
