#coding:utf-8

"""
ID:          intfunc.cast-08
TITLE:       CAST CHAR -> DATE
DESCRIPTION:
  Be careful about date format on FB server !
  Universal format is not defined or not documented.
FBTEST:      functional.intfunc.cast.08
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST('28.1.2001' AS DATE) FROM rdb$Database;")

expected_stdout = """
CAST
===========

2001-01-28
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
