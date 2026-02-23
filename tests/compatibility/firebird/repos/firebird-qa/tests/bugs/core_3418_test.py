#coding:utf-8

"""
ID:          issue-3781
ISSUE:       3781
TITLE:       Database trigger created as INACTIVE is active
DESCRIPTION:
JIRA:        CORE-3418
FBTEST:      bugs.core_3418
"""

import pytest
from firebird.qa import *

init_script = """
	set term ^ ;
	create or alter trigger trg$start inactive on transaction start position 0 as
	begin
		rdb$set_context('USER_SESSION', 'TRANS_ID', current_transaction);
	end
	^
	set term ; ^
	commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select rdb$get_context('USER_SESSION', 'TRANS_ID') as ctx_var from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    CTX_VAR                         <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

