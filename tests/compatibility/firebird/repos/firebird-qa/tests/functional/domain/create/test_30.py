#coding:utf-8

"""
ID:          domain.create-30
FBTEST:      functional.domain.create.30
TITLE:       CREATE DOMAIN - BLOB SUB_TYPE CHARACTER SET
DESCRIPTION: Domain creation based on BLOB datatype with SUBTYPE TEXT and CHARACTER SET specification
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE DOMAIN test BLOB SUB_TYPE 1 CHARACTER SET BIG_5;
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stdout = """TEST                            BLOB segment 80, subtype TEXT CHARACTER SET BIG_5 Nullable"""

@pytest.mark.skip("Covered by 'test_all_datatypes_basic.py'")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
