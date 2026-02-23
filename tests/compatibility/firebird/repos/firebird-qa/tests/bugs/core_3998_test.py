#coding:utf-8

"""
ID:          issue-4330
ISSUE:       4330
TITLE:       Parametrized execute statement fails
DESCRIPTION:
JIRA:        CORE-3998
FBTEST:      bugs.core_3998
"""

import pytest
from firebird.qa import *

init_script = """
	create table t (
		id integer not null,
		dir varchar(100) default '' not null,
		note varchar(100) default '' not null
	);
	commit;
	insert into t (id, dir, note) values (1, 'a', 'b');
	commit;
"""

db = db_factory(init=init_script)

test_script = """
	set term ^;
	execute block
	as
	  declare variable dir varchar(100);
	  declare variable note varchar(100);
	  declare variable id integer;
	begin
	  id = 1;
	  dir = 'a';
	  note = 'bbbb';

	  execute statement ('
		update t set
		  note = :note
		where
		  id = :id and dir = :dir
	  ')
	  (
		id := :id,
		note := :note,
		dir := :dir
	  );
	end ^
	set term ;^
	commit;
	set list on;
	select * from t;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
	ID                              1
	DIR                             a
	NOTE                            bbbb
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

