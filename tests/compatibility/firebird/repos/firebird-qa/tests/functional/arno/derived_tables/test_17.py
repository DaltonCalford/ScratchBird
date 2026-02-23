#coding:utf-8

"""
ID:          derived-table-17
TITLE:       Aggregate inside derived table
DESCRIPTION:
FBTEST:      functional.arno.derived_tables.17
"""

import pytest
from firebird.qa import *

init_script = """
    create table table_10 (
      id integer not null,
      groupid integer,
      description varchar(10)
    );
    commit;

    insert into table_10 (id, groupid, description) values (0, null, null);
    insert into table_10 (id, groupid, description) values (1, 1, 'one');
    insert into table_10 (id, groupid, description) values (2, 1, 'two');
    insert into table_10 (id, groupid, description) values (3, 2, 'three');
    insert into table_10 (id, groupid, description) values (4, 2, 'four');
    insert into table_10 (id, groupid, description) values (5, 2, 'five');
    insert into table_10 (id, groupid, description) values (6, 3, 'six');
    insert into table_10 (id, groupid, description) values (7, 3, 'seven');
    insert into table_10 (id, groupid, description) values (8, 3, 'eight');
    insert into table_10 (id, groupid, description) values (9, 3, 'nine');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set count on;
    select dt.*
    from (
        select t1.groupid, count(t1.id)
        from table_10 t1 group by t1.groupid
    ) dt (groupid, id_count)
    where
    dt.id_count = 2;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    GROUPID 1
    ID_COUNT 2
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
