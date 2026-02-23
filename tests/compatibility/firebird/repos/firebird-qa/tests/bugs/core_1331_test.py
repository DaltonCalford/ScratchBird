#coding:utf-8

"""
ID:          issue-1750
ISSUE:       1750
TITLE:       Charset transliterations don't work with EXECUTE STATEMENT
DESCRIPTION:
JIRA:        CORE-1331
FBTEST:      bugs.core_1331
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core1331-25.fbk', charset='utf8')

test_script = """
	set list on;
    select opis as direct_select from T1;
	set term ^ ;
	execute block returns ( execute_block_select varchar(100) ) as
	BEGIN
	  for select opis from t1
	  into :execute_block_select
	  do
	      SUSPEND;
	END^

	execute block returns ( execute_sttm_select varchar(100) ) as
	BEGIN
	  for execute statement 'select opis from t1'
	  into :execute_sttm_select
	  do
	      SUSPEND;
	END ^

	set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
	DIRECT_SELECT                   ASCII
	DIRECT_SELECT                   милан
	EXECUTE_BLOCK_SELECT            ASCII
	EXECUTE_BLOCK_SELECT            милан
	EXECUTE_STTM_SELECT             ASCII
	EXECUTE_STTM_SELECT             милан
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

