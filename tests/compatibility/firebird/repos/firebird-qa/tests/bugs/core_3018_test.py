#coding:utf-8

"""
ID:          issue-3399
ISSUE:       3399
TITLE:       RECREATE, ALTER and CREATE OR ALTER SEQUENCE/GENERATOR statements
DESCRIPTION:
  FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
  statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus
  next call of gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
  See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d
  This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt

  For this reason, old code was removed and now test checks only ability to use statements rather than results of them.
  Checked on: 4.0.0.2119; 4.0.0.2164; 3.0.7.33356.
JIRA:        CORE-3018
FBTEST:      bugs.core_3018
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Only FIRST of following statements must fail with
    -- SQLSTATE = 42000 / Dynamic SQL Error / -SQL error code = -104 / -Unexpected end of command.
    -- All subsequent must pass w/o errors.
    create or alter sequence g01;

    create or alter sequence g02 start with 2;

    create or alter sequence g03 start with 2 increment by 3;

    create or alter sequence g04 restart increment by 4;

    --#####################################################

    recreate sequence g05;

    recreate sequence g06 start with 6;

    recreate sequence g07 start with 7 increment by 8;
"""

act = isql_act('db', test_script, substitutions=[('end of command.*', 'end of command')])

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Unexpected end of command
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

