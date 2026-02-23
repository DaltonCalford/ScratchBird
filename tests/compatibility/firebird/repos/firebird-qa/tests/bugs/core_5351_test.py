#coding:utf-8

"""
ID:          issue-5624
ISSUE:       5624
TITLE:       LEFT JOIN incorrectly pushes UDF into the inner stream causing wrong results
DESCRIPTION:
NOTES:
[25.01.2019]
  totally changed: use ticket author's sample + dimitr's suggestion
  to operate with usual PSQL fcuntion (rather than UDF).
  Thus test nothing has to do with UDF or UDR.
JIRA:        CORE-5351
FBTEST:      bugs.core_5351
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set term ^;
    create or alter function psql_strlen (val varchar(10)) returns int as
    begin
      return char_length(coalesce(val, ''));
    end
    ^
    set term ;^
    commit;

    recreate table test_table1 (
        id integer not null,
        testtable2_id integer
    );

    alter table test_table1 add primary key (id);
    commit;

    recreate table test_table2 (
        id integer not null,
        groupcode varchar(10)
    );
    alter table test_table2 add primary key (id);
    commit;

    insert into test_table1 (id,testtable2_id) values (1,100);
    insert into test_table2 (id,groupcode) values (100,'a');
    commit;

    set count on;
    set list on;

    select t1.id
    from test_table1 t1
    left join test_table2 t2 on t2.id=t1.testtable2_id
    where psql_strlen(t2.groupcode) = 0 ;

"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
