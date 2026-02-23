#coding:utf-8

"""
ID:          intfunc.cast-15
TITLE:       CAST DATE -> CHAR
DESCRIPTION:
  Be careful about date format on FB server !
  Universal format is not defined or not documented.
FBTEST:      functional.intfunc.cast.15
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST(CAST('10.2.1973' AS DATE) AS CHAR(32)) FROM rdb$Database;")

expected_stdout = """
CAST
================================

1973-02-10
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
