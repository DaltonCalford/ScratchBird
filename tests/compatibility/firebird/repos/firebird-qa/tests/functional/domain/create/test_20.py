#coding:utf-8

"""
ID:          domain.create-20
FBTEST:      functional.domain.create.20
TITLE:       CREATE DOMAIN - VARCHAR CHARACTER SET
DESCRIPTION: Domain creation based on VARCHAR datatype with CHARACTER SET specification
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN test VARCHAR(32765) CHARACTER SET ASCII;
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST                            VARCHAR(32765) CHARACTER SET ASCII Nullable"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
