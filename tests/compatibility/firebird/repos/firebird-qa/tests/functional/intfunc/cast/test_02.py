#coding:utf-8

"""
ID:          intfunc.cast-02
TITLE:       CAST Numeric -> VARCHAR
DESCRIPTION:
FBTEST:      functional.intfunc.cast.02
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "SELECT CAST(1.25001 AS VARCHAR(21)) FROM rdb$Database;")

expected_stdout = """CAST
=====================

1.25001
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
