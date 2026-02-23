#coding:utf-8

"""
ID:          view.create-03
TITLE:       CREATE VIEW - bad number of columns
DESCRIPTION:
FBTEST:      functional.view.create.03
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create view test (id, num, text) as select 1 as id, 5 as num from rdb$database;
"""
act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_VEW_NAME = "TEST" if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 07002
        unsuccessful metadata update
        -CREATE VIEW {TEST_VEW_NAME} failed
        -SQL error code = -607
        -Invalid command
        -number of columns does not match select list
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
