#coding:utf-8

"""
ID:          issue-3471
ISSUE:       3471
TITLE:       ROW_COUNT is not cleared before the singleton INSERT statement
DESCRIPTION:
JIRA:        CORE-3092
FBTEST:      bugs.core_3092
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

init_script = """
	CREATE TABLE DELME (
		A INTEGER,
		B INTEGER
	);
	COMMIT;
	INSERT INTO DELME (A, B)
			   VALUES (1, 11);
	INSERT INTO DELME (A, B)
			   VALUES (2, 22);
	INSERT INTO DELME (A, B)
			   VALUES (3, 33);
	COMMIT;
	SET TERM ^;
	create procedure uui
	returns (
		result varchar(250))
	as
	begin
	  result = '';

	  update delme set
		b = 111
		where a = 1;
	  result = result||'update-1 '||row_count||'; ';

	  update delme set
		b = 222
		where a = 2;
	  result = result||'update-2 '||row_count||'; ';

	  insert into delme(a,b)
		values(4,44);
	  result = result||'insert-1 '||row_count||'; ';
	end
	^
	SET TERM ;^
	COMMIT;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    execute procedure uui;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
	RESULT                          update-1 1; update-2 1; insert-1 1;
"""

@pytest.mark.version('>=2.1.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

