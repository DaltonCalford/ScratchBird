#coding:utf-8

"""
ID:          tabloid.pg-14421
TITLE:       UPDATE/DETERE RETURNING should issue only one row when applying to table with self-referencing FK
DESCRIPTION: 
  Original issue:
     https://www.postgresql.org/message-id/cakfquwyrb5iyfqs6o9mmtbxp96l40bxpnfgosj8xm88ag%2b5_aa%40mail.gmail.com
FBTEST:      functional.tabloid.pg_14421
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(
        id int primary key,
        pid int references test(id) on delete cascade on update cascade
    );
    insert into test values (1, null);
    insert into test values (2, 1);
    insert into test values (3, 2);
    insert into test values (4, 2);
    insert into test values (5, 2);
    insert into test values (6, 2);
    insert into test values (7, 2);
    commit;

    delete from test
       where id = 2
       returning id as old_id
    ;
    rollback;


    update test set id=9 where id=2 
    returning old.id as old_id, new.id as new_id
    ;

    rollback;
"""

act = isql_act('db', test_script)

expected_stdout = """
    OLD_ID                          2



    OLD_ID                          2
    NEW_ID                          9
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
