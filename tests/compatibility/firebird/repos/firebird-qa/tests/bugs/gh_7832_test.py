#coding:utf-8

"""
ID:          issue-7832
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7832
TITLE:       Firebird 5 and 6 crash on "... RETURNING * " without INTO in PSQL
DESCRIPTION:
NOTES:
    [10.11.2023] pzotov
    Confirmed crash on 6.0.0.104, 5.0.0.1259
    Checked on 6.0.0.107, 5.0.0.1264 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    recreate table ttt (id int)^
    execute block
    as
    begin
        delete from ttt returning *;
    end^
    set term ;^
"""
expected_stdout = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Unexpected end of command -
"""

substitutions = [('line(:)? \\d+, col(umn)?(:)? \\d+', '')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
