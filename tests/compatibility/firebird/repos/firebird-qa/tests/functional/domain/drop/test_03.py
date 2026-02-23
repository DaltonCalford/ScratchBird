#coding:utf-8

"""
ID:          domain.drop-03
FBTEST:      functional.domain.drop.03
TITLE:       DROP DOMAIN - that doesn't exists
DESCRIPTION:
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "DROP DOMAIN NO_SUCH_DOMAIN;")

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_stdout_5x = """
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP DOMAIN NO_SUCH_DOMAIN failed
        -Domain not found
    """

    expected_stdout_6x = """
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP DOMAIN "PUBLIC"."NO_SUCH_DOMAIN" failed
        -Domain not found
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
