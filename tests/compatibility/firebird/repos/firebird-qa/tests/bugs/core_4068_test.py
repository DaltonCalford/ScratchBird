#coding:utf-8

"""
ID:          issue-4396
ISSUE:       4396
TITLE:       create package fails on creating header as soon as there is at least 1 procedure name
DESCRIPTION:
JIRA:        CORE-4068
FBTEST:      bugs.core_4068
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter package fb$out
    as
    begin
    procedure enable;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
