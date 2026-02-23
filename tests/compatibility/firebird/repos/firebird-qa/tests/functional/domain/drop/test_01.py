#coding:utf-8

"""
ID:          domain.drop-01
FBTEST:      functional.domain.drop.01
TITLE:       DROP DOMAIN
DESCRIPTION:
"""

import pytest
from firebird.qa import *

db = db_factory(init="CREATE DOMAIN test SMALLINT;")

test_script = """DROP DOMAIN test;
SHOW DOMAIN test;"""

act = isql_act('db', test_script)

expected_stderr = """There is no domain TEST in this database"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
