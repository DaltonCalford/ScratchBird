#coding:utf-8

"""
ID:          intfunc.cast-18
TITLE:       CAST TIME -> CHAR
DESCRIPTION:
  Be careful about time format on FB server !
  Universal format is not defined or not documented.
FBTEST:      functional.intfunc.cast.18
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST(CAST('13:28:45' AS TIME) AS CHAR(32)) FROM rdb$Database;")

expected_stdout = """
CAST
================================

13:28:45.0000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
