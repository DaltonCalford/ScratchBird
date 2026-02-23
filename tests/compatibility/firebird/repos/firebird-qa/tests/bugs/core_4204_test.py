#coding:utf-8

"""
ID:          issue-4529
ISSUE:       4529
TITLE:       Error when compiling the procedure containing the statement if (x = (select ...))
DESCRIPTION:
JIRA:        CORE-4204
FBTEST:      bugs.core_4204
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure Test_C
    as
      declare variable X varchar(16);
    begin

      if (x = (select '123' from Rdb$Database)) then
      begin
        exit;
      end
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
