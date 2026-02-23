#coding:utf-8

"""
ID:          intfunc.cast-06
TITLE:       CAST CHAR -> INTEGER (Round down)
DESCRIPTION:
FBTEST:      functional.intfunc.cast.06
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST('1.25001' AS INTEGER) FROM rdb$Database;")

expected_stdout = """
CAST
============

1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
