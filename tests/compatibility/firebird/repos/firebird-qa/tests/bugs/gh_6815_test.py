#coding:utf-8

"""
ID:          issue-6815
ISSUE:       6815
TITLE:       Support multiple rows for DML RETURNING
DESCRIPTION:
  Test verifies only basic issues related mostly to syntax and ability to get multiple rows.
  Both DSQL and PSQL are tested.

  Interruption of fetching process by client (and check number of affected rows) is NOT verified:
  separate test will be made later (see ticket, comment by Adriano, date: 15-JUL-2021).
FBTEST:      bugs.gh_6815
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table test(id int, x int, y int);
    commit;

    insert into test(id, x, y)
    select
        row_number()over()
        ,row_number()over()*2
        ,row_number()over()*3
    from rdb$types rows 5
    returning id as dsql_id_inserted, x as dsql_x_inserted, y as dsql_y_inserted;
    commit;

    update test set y = -y, x = -y where mod(y,2)=1 returning id as dsql_id_updated, x as dsql_x_updated, y as dsql_y_updated;
    rollback;

    delete from test where mod(y,2)=0 returning id as dsql_id_deleted, x as dsql_x_deleted, y as dsql_y_deleted;
    rollback;


    merge into test t
    using test s on s.id = t.id
    when matched then update set t.x = s.y, t.y = s.x, t.id = -s.id
    returning t.id as dsql_id_merge_updated, t.x as dsql_x_merge_updated, t.y as dsql_y_merge_updated
    ;
    rollback;


    merge into test t
    using test s on s.id = -t.id
    when NOT matched then insert values(-s.id, -s.x, -s.y)
    returning t.id as dsql_id_merge_inserted, t.x as dsql_x_merge_inserted, t.y as dsql_y_merge_inserted
    ;
    rollback;


    update test set x = 2;
    update or insert into test(id, x, y) values(null, 2, null) matching (x)
    returning id as dsql_id_update_or_inserted, x as dsql_x_update_or_inserted, y as dsql_y_update_or_inserted
    ;
    --select * from test;

    rollback;

    --##############################################################################################

    delete from test;
    commit;


    set term ^;
    execute block returns(psql_id_inserted int, psql_x_inserted int, psql_y_inserted int) as
    begin
        for execute statement
            '
                insert into test(id, x, y)
                select
                    row_number()over()
                    ,row_number()over()*2
                    ,row_number()over()*3
                from rdb$types rows 5
                returning id,x,y
            '
            into psql_id_inserted, psql_x_inserted, psql_y_inserted
        do
            suspend;
    end
    ^
    commit
    ^
    execute block returns(psql_id_updated int, psql_x_updated int, psql_y_updated int) as
    begin
        for execute statement
            '
                update test set y = -y, x = -y
                where mod(y,2)=1
                returning id, x, y
            '
            into psql_id_updated, psql_x_updated, psql_y_updated
        do
            suspend;
    end
    ^
    rollback
    ^
    execute block returns(psql_id_deleted int, psql_x_deleted int, psql_y_deleted int) as
    begin
        for execute statement
            '
                delete from test
                where mod(y,2)=0
                returning id, x, y
            '
            into psql_id_deleted, psql_x_deleted, psql_y_deleted
        do
            suspend;
    end
    ^
    rollback
    ^
    execute block returns(psql_id_merge_updated int, psql_x_merge_updated int, psql_y_merge_updated int) as
    begin
        for execute statement
            '
                merge into test t
                using test s on s.id = t.id
                when matched then update set t.x = s.y, t.y = s.x, t.id = -s.id
                returning t.id, t.x, t.y
            '
            into psql_id_merge_updated, psql_x_merge_updated, psql_y_merge_updated
        do
            suspend;
    end
    ^
    rollback
    ^
    execute block returns(psql_id_merge_inserted int, psql_x_merge_inserted int, psql_y_merge_inserted int) as
    begin
        for execute statement
            '
                merge into test t
                using test s on s.id = -t.id
                when NOT matched then insert values(-s.id, -s.x, -s.y)
                returning t.id, t.x, t.y
            '
            into psql_id_merge_inserted, psql_x_merge_inserted, psql_y_merge_inserted
        do
            suspend;
    end
    ^
    rollback
    ^
    execute block returns(psql_id_update_or_inserted int, psql_x_update_or_inserted int, psql_y_update_or_inserted int) as
    begin
        update test set x = 2;
        for execute statement
            '
                update or insert into test(id, x, y) values(null, 2, null) matching (x)
                returning id, x, y
            '
            into psql_id_update_or_inserted, psql_x_update_or_inserted, psql_y_update_or_inserted
        do
            suspend;
    end
    ^
    set term ;^
    rollback;

"""

act = isql_act('db', test_script)

expected_stdout = """
    DSQL_ID_INSERTED                1
    DSQL_X_INSERTED                 2
    DSQL_Y_INSERTED                 3

    DSQL_ID_INSERTED                2
    DSQL_X_INSERTED                 4
    DSQL_Y_INSERTED                 6

    DSQL_ID_INSERTED                3
    DSQL_X_INSERTED                 6
    DSQL_Y_INSERTED                 9

    DSQL_ID_INSERTED                4
    DSQL_X_INSERTED                 8
    DSQL_Y_INSERTED                 12

    DSQL_ID_INSERTED                5
    DSQL_X_INSERTED                 10
    DSQL_Y_INSERTED                 15



    DSQL_ID_UPDATED                 1
    DSQL_X_UPDATED                  -3
    DSQL_Y_UPDATED                  -3

    DSQL_ID_UPDATED                 3
    DSQL_X_UPDATED                  -9
    DSQL_Y_UPDATED                  -9

    DSQL_ID_UPDATED                 5
    DSQL_X_UPDATED                  -15
    DSQL_Y_UPDATED                  -15



    DSQL_ID_DELETED                 2
    DSQL_X_DELETED                  4
    DSQL_Y_DELETED                  6

    DSQL_ID_DELETED                 4
    DSQL_X_DELETED                  8
    DSQL_Y_DELETED                  12



    DSQL_ID_MERGE_UPDATED           -1
    DSQL_X_MERGE_UPDATED            3
    DSQL_Y_MERGE_UPDATED            2

    DSQL_ID_MERGE_UPDATED           -2
    DSQL_X_MERGE_UPDATED            6
    DSQL_Y_MERGE_UPDATED            4

    DSQL_ID_MERGE_UPDATED           -3
    DSQL_X_MERGE_UPDATED            9
    DSQL_Y_MERGE_UPDATED            6

    DSQL_ID_MERGE_UPDATED           -4
    DSQL_X_MERGE_UPDATED            12
    DSQL_Y_MERGE_UPDATED            8

    DSQL_ID_MERGE_UPDATED           -5
    DSQL_X_MERGE_UPDATED            15
    DSQL_Y_MERGE_UPDATED            10



    DSQL_ID_MERGE_INSERTED          -1
    DSQL_X_MERGE_INSERTED           -2
    DSQL_Y_MERGE_INSERTED           -3

    DSQL_ID_MERGE_INSERTED          -2
    DSQL_X_MERGE_INSERTED           -4
    DSQL_Y_MERGE_INSERTED           -6

    DSQL_ID_MERGE_INSERTED          -3
    DSQL_X_MERGE_INSERTED           -6
    DSQL_Y_MERGE_INSERTED           -9

    DSQL_ID_MERGE_INSERTED          -4
    DSQL_X_MERGE_INSERTED           -8
    DSQL_Y_MERGE_INSERTED           -12

    DSQL_ID_MERGE_INSERTED          -5
    DSQL_X_MERGE_INSERTED           -10
    DSQL_Y_MERGE_INSERTED           -15



    DSQL_ID_UPDATE_OR_INSERTED      <null>
    DSQL_X_UPDATE_OR_INSERTED       2
    DSQL_Y_UPDATE_OR_INSERTED       <null>

    DSQL_ID_UPDATE_OR_INSERTED      <null>
    DSQL_X_UPDATE_OR_INSERTED       2
    DSQL_Y_UPDATE_OR_INSERTED       <null>

    DSQL_ID_UPDATE_OR_INSERTED      <null>
    DSQL_X_UPDATE_OR_INSERTED       2
    DSQL_Y_UPDATE_OR_INSERTED       <null>

    DSQL_ID_UPDATE_OR_INSERTED      <null>
    DSQL_X_UPDATE_OR_INSERTED       2
    DSQL_Y_UPDATE_OR_INSERTED       <null>

    DSQL_ID_UPDATE_OR_INSERTED      <null>
    DSQL_X_UPDATE_OR_INSERTED       2
    DSQL_Y_UPDATE_OR_INSERTED       <null>



    PSQL_ID_INSERTED                1
    PSQL_X_INSERTED                 2
    PSQL_Y_INSERTED                 3

    PSQL_ID_INSERTED                2
    PSQL_X_INSERTED                 4
    PSQL_Y_INSERTED                 6

    PSQL_ID_INSERTED                3
    PSQL_X_INSERTED                 6
    PSQL_Y_INSERTED                 9

    PSQL_ID_INSERTED                4
    PSQL_X_INSERTED                 8
    PSQL_Y_INSERTED                 12

    PSQL_ID_INSERTED                5
    PSQL_X_INSERTED                 10
    PSQL_Y_INSERTED                 15



    PSQL_ID_UPDATED                 1
    PSQL_X_UPDATED                  -3
    PSQL_Y_UPDATED                  -3

    PSQL_ID_UPDATED                 3
    PSQL_X_UPDATED                  -9
    PSQL_Y_UPDATED                  -9

    PSQL_ID_UPDATED                 5
    PSQL_X_UPDATED                  -15
    PSQL_Y_UPDATED                  -15



    PSQL_ID_DELETED                 2
    PSQL_X_DELETED                  4
    PSQL_Y_DELETED                  6

    PSQL_ID_DELETED                 4
    PSQL_X_DELETED                  8
    PSQL_Y_DELETED                  12



    PSQL_ID_MERGE_UPDATED           -1
    PSQL_X_MERGE_UPDATED            3
    PSQL_Y_MERGE_UPDATED            2

    PSQL_ID_MERGE_UPDATED           -2
    PSQL_X_MERGE_UPDATED            6
    PSQL_Y_MERGE_UPDATED            4

    PSQL_ID_MERGE_UPDATED           -3
    PSQL_X_MERGE_UPDATED            9
    PSQL_Y_MERGE_UPDATED            6

    PSQL_ID_MERGE_UPDATED           -4
    PSQL_X_MERGE_UPDATED            12
    PSQL_Y_MERGE_UPDATED            8

    PSQL_ID_MERGE_UPDATED           -5
    PSQL_X_MERGE_UPDATED            15
    PSQL_Y_MERGE_UPDATED            10



    PSQL_ID_MERGE_INSERTED          -1
    PSQL_X_MERGE_INSERTED           -2
    PSQL_Y_MERGE_INSERTED           -3

    PSQL_ID_MERGE_INSERTED          -2
    PSQL_X_MERGE_INSERTED           -4
    PSQL_Y_MERGE_INSERTED           -6

    PSQL_ID_MERGE_INSERTED          -3
    PSQL_X_MERGE_INSERTED           -6
    PSQL_Y_MERGE_INSERTED           -9

    PSQL_ID_MERGE_INSERTED          -4
    PSQL_X_MERGE_INSERTED           -8
    PSQL_Y_MERGE_INSERTED           -12

    PSQL_ID_MERGE_INSERTED          -5
    PSQL_X_MERGE_INSERTED           -10
    PSQL_Y_MERGE_INSERTED           -15



    PSQL_ID_UPDATE_OR_INSERTED      <null>
    PSQL_X_UPDATE_OR_INSERTED       2
    PSQL_Y_UPDATE_OR_INSERTED       <null>

    PSQL_ID_UPDATE_OR_INSERTED      <null>
    PSQL_X_UPDATE_OR_INSERTED       2
    PSQL_Y_UPDATE_OR_INSERTED       <null>

    PSQL_ID_UPDATE_OR_INSERTED      <null>
    PSQL_X_UPDATE_OR_INSERTED       2
    PSQL_Y_UPDATE_OR_INSERTED       <null>

    PSQL_ID_UPDATE_OR_INSERTED      <null>
    PSQL_X_UPDATE_OR_INSERTED       2
    PSQL_Y_UPDATE_OR_INSERTED       <null>

    PSQL_ID_UPDATE_OR_INSERTED      <null>
    PSQL_X_UPDATE_OR_INSERTED       2
    PSQL_Y_UPDATE_OR_INSERTED       <null>
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
