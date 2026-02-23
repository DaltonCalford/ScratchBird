#coding:utf-8

"""
ID:          procedure.create-08
TITLE:       CREATE PROCEDURE - COMMIT in SP is not alowed
DESCRIPTION:
FBTEST:      functional.procedure.create.08
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create procedure sp_test returns(id int)as
    begin
        commit;
    end ^
    set term ;^
"""

substitutions = [('Token unknown.*', 'Token unknown')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    expected_stdout = """
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Token unknown - line 3, column 3
        -commit
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
