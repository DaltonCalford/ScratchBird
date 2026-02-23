#coding:utf-8

"""
ID:          issue-3303
ISSUE:       3303
TITLE:       Incorrect execution of volatile SQL statements inside EXECUTE STATEMENT
DESCRIPTION:
JIRA:        CORE-2920
FBTEST:      bugs.core_2920
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

init_script = """
    create table bugtest (id int);
    insert into bugtest (id) values (123);
    set term ^;
    create procedure p_bugtest (in_id int)
      returns (cnt int)
    as
      declare predicate varchar(1000);
    begin
      if (:in_id is null) then
        predicate = ' ? is null';
      else
        predicate = ' id = ?';

      execute statement ('select count(*) from bugtest where' || predicate) (:in_id)
      into :cnt;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """
    set list on;
    execute procedure p_bugtest (123);
    execute procedure p_bugtest (null);
    execute procedure p_bugtest (123);
"""

act = isql_act('db', test_script)

expected_stdout = """
    CNT                             1
    CNT                             1
    CNT                             1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

