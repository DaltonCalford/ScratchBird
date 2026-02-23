#coding:utf-8

"""
ID:          procedure.alter-02
TITLE:       ALTER PROCEDURE - Alter non exists procedure
DESCRIPTION:
FBTEST:      functional.procedure.alter.02
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    alter procedure sp_test returns (id integer)as
    begin
      id=2;
    end ^
    set term ;^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_PROC_NAME = 'SP_TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"SP_TEST"'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER PROCEDURE {TEST_PROC_NAME} failed
        -Procedure {TEST_PROC_NAME} not found
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
