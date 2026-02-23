#coding:utf-8

"""
ID:          issue-6753
ISSUE:       6753
TITLE:       AV in engine when StatementTimeout is active for user statement and some
  internal DSQL statement was executed as part of overall execution process
DESCRIPTION:
JIRA:        CORE-6526
FBTEST:      bugs.core_6526
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set heading off;
    set term ^;
    execute block as begin
      in autonomous transaction do
         execute statement 'set statistics index rdb$index_0';
    end
    ^
    set statement timeout 60
    ^
    execute block as begin
      in autonomous transaction do
         execute statement 'set statistics index rdb$index_0';
    end
    ^
    set term ;^
    commit;
    select 'Done.' from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Done.
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
