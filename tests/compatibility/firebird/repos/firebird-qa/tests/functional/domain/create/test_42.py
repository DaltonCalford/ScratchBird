#coding:utf-8

"""
ID:          domain.create-42
FBTEST:      functional.domain.create.42
TITLE:       CREATE DOMAIN - domain name equal to existing datatype
DESCRIPTION: Domain creation must fail (SQLCODE -104) if domain name is equal to datatype name
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "CREATE DOMAIN INT AS VARCHAR(32);")

expected_stderr = """Statement failed, SQLSTATE = 42000

Dynamic SQL Error
-SQL error code = -104
-Token unknown - line 1, column 15
-INT"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
