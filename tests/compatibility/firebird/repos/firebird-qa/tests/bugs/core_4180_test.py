#coding:utf-8

"""
ID:          issue-4506
ISSUE:       4506
TITLE:       CREATE COLLATION does not verify base collation charset
DESCRIPTION:
JIRA:        CORE-4180
FBTEST:      bugs.core_4180
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table tci(id int);
    commit;

    set term ^;
    execute block as
    begin
      begin
        execute statement 'drop collation ci_coll';
      when any do begin end
      end

      begin
        execute statement 'drop collation win1252_unicode';
      when any do begin end
      end
    end
    ^ set term ;^
    commit;

    create collation win1252_unicode for win1252; -- this is special collation named (charset_unicode)
    create collation ci_coll for win1252 from win1252_unicode case insensitive;
    commit;

    recreate table tci(id int, name varchar(30) character set win1252 collate ci_coll);
    commit;
    insert into tci(id, name) values(9, 'one Row');
    insert into tci(id, name) values(7, 'One row');
    insert into tci(id, name) values(0, 'oNE row');
    insert into tci(id, name) values(1, 'one ROw');
    insert into tci(id, name) values(5, 'one roW');
    insert into tci(id, name) values(4, 'oNe roW');
    insert into tci(id, name) values(2, 'onE ROw');
    insert into tci(id, name) values(3, 'ONE ROW');
    insert into tci(id, name) values(8, 'oNE rOW');
    insert into tci(id, name) values(1, 'one row');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select n1, n2, min(n1) n3, max(n2) n4
    from (
      select n1, n2
      from (
        select min(name) n1, max(name) n2 from tci group by name
        union all
        select distinct name, name from tci
        union all
        select distinct max(name), max(name) from tci
      )
      order by rand()
    )
    group by n1, n2;
"""

act = isql_act('db', test_script)

expected_stdout = """
N1                              one row
N2                              one row
N3                              ONE ROW
N4                              ONE ROW
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
