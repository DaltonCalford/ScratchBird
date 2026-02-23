#coding:utf-8

"""
ID:          issue-3793
ISSUE:       3793
TITLE:       ISQL pads UTF-8 data incorrectly
DESCRIPTION:
JIRA:        CORE-3431
FBTEST:      bugs.core_3431
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
	set term ^;
	create or alter procedure p1 returns (
	  c1 char(5) character set utf8,
	  c2 char(10) character set utf8,
	  vc1 varchar(5) character set utf8,
	  vc2 varchar(10) character set utf8
	)
	as
	begin
	  vc1 = '12345';
	  c1 = vc1;
	  c2 = vc1;
	  vc2 = vc1;
	  suspend;

	  vc1 = 'áé';
	  c1 = vc1;
	  c2 = vc1;
	  vc2 = vc1;
	  suspend;

	  vc1 = '123';
	  c1 = vc1;
	  c2 = vc1;
	  vc2 = vc1;
	  suspend;

	  vc1 = '12345';
	  c1 = vc1;
	  c2 = vc1;
	  vc2 = vc1;
	  suspend;

	  vc1 = '1234';
	  c1 = vc1;
	  c2 = vc1;
	  vc2 = vc1;
	  suspend;

	  vc1 = 'áéíóú';
	  c1 = vc1;
	  c2 = vc1;
	  vc2 = vc1;
	  suspend;
	end
	^
	set term ;^
	commit;

	-- Padding in WI-T3.0.0.31767 looks OK: checked output on NotePad++ 6.6.9
	select * from p1;
"""

act = isql_act('db', test_script)

expected_stdout = """
	C1     C2         VC1    VC2
	====== ========== ====== ==========
	12345  12345      12345  12345
	áé     áé         áé     áé
	123    123        123    123
	12345  12345      12345  12345
	1234   1234       1234   1234
	áéíóú  áéíóú      áéíóú  áéíóú
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

