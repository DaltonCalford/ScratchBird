#coding:utf-8

"""
ID:          domain.create-08
FBTEST:      functional.domain.create.08
TITLE:       CREATE DOMAIN - TIME
DESCRIPTION: Simple domain creation based TIME datatype
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN test TIME;
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST                            TIME Nullable"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
