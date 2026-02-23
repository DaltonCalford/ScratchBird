#coding:utf-8

"""
ID:          intfunc.cast-17
TITLE:       CAST DATE -> TIMESTAMP
DESCRIPTION:
  Be careful about date/time format on FB server !
  Universal format is not defined or not documented.
FBTEST:      functional.intfunc.cast.17
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST(CAST('10.2.1973' AS DATE) AS TIMESTAMP) FROM rdb$Database;")

expected_stdout = """
CAST
=========================

1973-02-10 00:00:00.0000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
