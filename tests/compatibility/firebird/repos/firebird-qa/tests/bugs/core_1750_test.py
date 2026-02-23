#coding:utf-8

"""
ID:          issue-2175
ISSUE:       2175
TITLE:       Include information about context variables into the monitoring tables
DESCRIPTION:
JIRA:        CORE-1750
FBTEST:      bugs.core_1750
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
	set count on;
	select mon$variable_name, mon$variable_value from mon$context_variables;
	commit;
	set term ^;
	execute block as
	    declare c int;
	begin
	    c = rdb$set_context('USER_SESSION', 'SESSION_LEVEL_VARIABLE', 1);
	end
	^
	commit
	^
	execute block as
	    declare c int;
	begin
	    c = rdb$set_context('USER_TRANSACTION', 'TRANSACTION_LEVEL_VARIABLE', 1);
	end
	^
	select mon$variable_name, mon$variable_value from mon$context_variables
	^
"""

act = isql_act('db', test_script)

expected_stdout = """
	Records affected: 0

	MON$VARIABLE_NAME               SESSION_LEVEL_VARIABLE
	MON$VARIABLE_VALUE              1
	MON$VARIABLE_NAME               TRANSACTION_LEVEL_VARIABLE
	MON$VARIABLE_VALUE              1

	Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

