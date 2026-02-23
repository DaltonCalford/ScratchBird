#coding:utf-8

"""
ID:          issue-3022
ISSUE:       3022
TITLE:       Connection lost immediatelly after compiling procedure with RPAD system function
DESCRIPTION:
JIRA:        CORE-2612
FBTEST:      bugs.core_2612
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
	set term ^;
	create procedure TEST2 (name varchar(50),spaces integer)
	returns (rname varchar(200))
	as
	begin
	  rname = name || rpad(' ',:spaces,' ');
	  suspend;
	end^
	set term ;^
	commit;
	set list on;
	set count on;
	select rname || 'end' as rname_padded from TEST2 ('test',5);
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
	RNAME_PADDED                    test     end
	Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

