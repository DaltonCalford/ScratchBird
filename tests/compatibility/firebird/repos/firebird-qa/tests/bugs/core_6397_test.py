#coding:utf-8

"""
ID:          issue-6635
ISSUE:       6635
TITLE:       Message length error with COALESCE and TIME/TIMESTAMP WITHOUT TIME ZONE and WITH TIME ZONE
DESCRIPTION:
  Test uses EXECUTE BLOCK with dummy variable to store reuslt (w/o suspend in order to prevent any output).
  EXECUTE STATEMENT must be used here for reproducing problem (no error with static PSQL code).
JIRA:        CORE-6397
FBTEST:      bugs.core_6397
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
        declare dummy timestamp;
    begin
        execute statement 'select coalesce(localtimestamp, current_timestamp) from rdb$database' into dummy;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.execute()
