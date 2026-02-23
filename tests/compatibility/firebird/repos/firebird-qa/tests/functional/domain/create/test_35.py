#coding:utf-8

"""
ID:          domain.create-35
FBTEST:      functional.domain.create.35
TITLE:       CREATE DOMAIN - DEFAULT CURRENT_USER
DESCRIPTION: Domain creation based on VARCHAR datatype with CURRENT_USER DEFAULT specification
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN test VARCHAR(32) DEFAULT CURRENT_USER;
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST                            VARCHAR(32) Nullable
DEFAULT CURRENT_USER"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.skip("Test is covered by test_34.py")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
