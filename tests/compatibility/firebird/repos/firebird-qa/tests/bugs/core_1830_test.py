#coding:utf-8

"""
ID:          issue-2259
ISSUE:       2259
TITLE:       Possible index corruption with multiply updates of the same record in the same transaction and using of savepoints
DESCRIPTION:
JIRA:        CORE-1830
FBTEST:      bugs.core_1830
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
	create table a(id char(1), name varchar(255));

	create index idx_a on a (id);
	create exception exc_wrong 'Something wrong occurs...';
	commit ;

	insert into a (id) values ('1');
	commit;

    set list on;
	select * from a where id = '1';
	set term ^;
	execute block as
	begin
	  update a set name = 'xxx';
	  update a set id = '2';
	  exception exc_wrong;
	end ^
	set term ; ^
	set count on;
	select 'point-1' as msg, a.* from a ;
	select 'point-2' as msg, a.* from a where id = '1' ;
	commit;
	select 'point-3' as msg, a.* from a ;
"""

substitutions = [ ('[ \t]+', ' '), ('column.*', 'column x'), ('-At block line: [\\d]+, col: [\\d]+', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    ID 1
    NAME <null>
    Statement failed, SQLSTATE = HY000
    exception 1
    -EXC_WRONG
    -Something wrong occurs...
    MSG point-1
    ID 1
    NAME <null>
    Records affected: 1

    MSG point-2
    ID 1
    NAME <null>
    Records affected: 1

    MSG point-3
    ID 1
    NAME <null>
    Records affected: 1
"""
expected_stdout_6x = """
    ID 1
    NAME <null>
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EXC_WRONG"
    -Something wrong occurs...
    MSG point-1
    ID 1
    NAME <null>
    Records affected: 1
    MSG point-2
    ID 1
    NAME <null>
    Records affected: 1
    MSG point-3
    ID 1
    NAME <null>
    Records affected: 1
"""

@pytest.mark.version('>=2.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
