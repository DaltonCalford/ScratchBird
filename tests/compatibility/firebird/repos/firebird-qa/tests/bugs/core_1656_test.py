#coding:utf-8

"""
ID:          issue-2080
ISSUE:       2080
TITLE:       Ability to format UUID from char(16) OCTETS to human readable form and vice versa
DESCRIPTION:
JIRA:        CORE-1656
FBTEST:      bugs.core_1656
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select uuid_to_char(char_to_uuid('93519227-8D50-4E47-81AA-8F6678C096A1')) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
UUID_TO_CHAR
====================================
93519227-8D50-4E47-81AA-8F6678C096A1

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

