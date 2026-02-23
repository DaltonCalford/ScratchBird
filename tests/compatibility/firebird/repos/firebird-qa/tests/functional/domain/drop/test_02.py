#coding:utf-8

"""
ID:          domain.drop-02
FBTEST:      functional.domain.drop.02
TITLE:       DROP DOMAIN - in use
DESCRIPTION:
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    CREATE DOMAIN test SMALLINT;
    CREATE TABLE tb( id test);
    DROP DOMAIN test;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_stdout_5x = """
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP DOMAIN TEST failed
        -Domain TEST is used in table TB (local name ID) and cannot be dropped
    """

    expected_stdout_6x = """
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP DOMAIN "PUBLIC"."TEST" failed
        -Domain "PUBLIC"."TEST" is used in table "PUBLIC"."TB" (local name "ID") and cannot be dropped
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
