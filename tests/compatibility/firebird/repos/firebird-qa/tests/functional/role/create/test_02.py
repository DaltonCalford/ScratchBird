#coding:utf-8

"""
ID:          role.create-02
TITLE:       CREATE ROLE - try create role with same name
DESCRIPTION:
FBTEST:      functional.role.create.02
"""

import pytest
from firebird.qa import *

db = db_factory()
test_role = role_factory('db', name='test')

act = isql_act('db', "CREATE ROLE test;")

expected_stderr = """Statement failed, SQLSTATE = 42000
unsuccessful metadata update
-CREATE ROLE TEST failed
-SQL role TEST already exists
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, test_role):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
