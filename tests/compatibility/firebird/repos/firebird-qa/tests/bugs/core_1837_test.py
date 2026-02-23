#coding:utf-8

"""
ID:          issue-2266
ISSUE:       2266
TITLE:       Procedure text is stored truncated in system tables if any variable have default value
DESCRIPTION:
JIRA:        CORE-1837
FBTEST:      bugs.core_1837
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create procedure sp_test
    as
    declare x int = 0;
    begin
      exit;
    end ^
    commit ^

    set list on ^
    set blob all ^
    select p.rdb$procedure_source from rdb$procedures p where p.rdb$procedure_name = upper('sp_test') ^
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$PROCEDURE_SOURCE.*', '')])

expected_stdout = """
    RDB$PROCEDURE_SOURCE            1a:0
    declare x int = 0;
    begin
      exit;
    end
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

