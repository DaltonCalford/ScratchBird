#coding:utf-8

"""
ID:          intfunc.string.left
TITLE:       LEFT function
DESCRIPTION:
  Returns the substring of a specified length that appears at the start of a left-to-right string.
FBTEST:      functional.intfunc.string.left_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select left('bonjour', 3)
from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
LEFT
=======
bon
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
