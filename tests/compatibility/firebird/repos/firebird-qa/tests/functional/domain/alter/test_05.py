#coding:utf-8

"""
ID:          domain.alter-05
FBTEST:      functional.domain.alter.05
TITLE:       ALTER DOMAIN - Alter domain that doesn't exists
DESCRIPTION:
"""

import pytest
from firebird.qa import *

db = db_factory(init="CREATE DOMAIN test VARCHAR(63);")
act = isql_act('db', "ALTER DOMAIN notexists DROP CONSTRAINT;")


@pytest.mark.version('>=3.0')
def test_1(act: Action):
    
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    DOMAIN_NAME = 'NOTEXISTS' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"NOTEXISTS"'
    expected_stdout = f"""Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER DOMAIN {DOMAIN_NAME} failed
        -Domain not found
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
