#coding:utf-8

"""
ID:          intfunc.cast-07
TITLE:       CAST CHAR -> INTEGER
DESCRIPTION:
FBTEST:      functional.intfunc.cast.07
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST('1.5001' AS INTEGER) FROM rdb$Database;")

expected_stdout = """
CAST
============
           2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
