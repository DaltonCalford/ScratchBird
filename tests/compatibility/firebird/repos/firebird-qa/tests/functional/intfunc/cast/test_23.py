#coding:utf-8

"""
ID:          intfunc.cast-23
TITLE:       CAST TIMESTAMP -> TIME
DESCRIPTION:
  Be careful about date/time format on FB server !
  Universal format is not defined or not documented.
FBTEST:      functional.intfunc.cast.23
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST(CAST('1.4.2002 0:59:59.1' AS TIMESTAMP) AS TIME) FROM rdb$Database;")

expected_stdout = """
CAST
=============

00:59:59.1000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
