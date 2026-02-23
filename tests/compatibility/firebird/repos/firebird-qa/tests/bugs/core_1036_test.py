#coding:utf-8

"""
ID:          issue-1453
ISSUE:       1453
TITLE:       Extend returning support added in ver 2 in update record of table and in updateble view
DESCRIPTION:
  Only basic support is checked here (i.e. only table and one-to-one projection view,
  WITHOUT trigger(s)).
JIRA:        CORE-1036
FBTEST:      bugs.core_1036
"""

import pytest
from firebird.qa import *

init_script = """
    recreate view v_test as select 1 x from rdb$database;
    commit;

    recreate table test(id int primary key, x int default 11, y int default 12, z computed by(x+y) );
    commit;

    recreate view v_test as select id, x, y, z from test;
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    insert into test(id, x, y) values(1,100,200);
    commit;

    insert into test(id) values(2) returning x as t_inserted_x, y as t_inserted_y, z as t_inserted_z;
    update test set id=-id, x=-2*y, y=-3*x where id=1 returning x as t_updated_x, y as t_updated_y, z as t_updated_z;
    delete from test where id < 0 returning x as t_deleted_x, y as t_deleted_y, z as t_deleted_z;

    delete from test;
    insert into test(id, x, y) values(1,100,200);
    commit;

    insert into v_test(id) values(3) returning x as v_inserted_x, y as v_inserted_y, z as v_inserted_z;
    update v_test set id=-id, x=-2*y, y=-3*x where id=1 returning x as v_updated_x, y as v_updated_y, z as v_updated_z;
    delete from v_test where id < 0 returning x as v_deleted_x, y as v_deleted_y, z as v_deleted_z;
"""

act = isql_act('db', test_script)

expected_stdout = """
    T_INSERTED_X                    11
    T_INSERTED_Y                    12
    T_INSERTED_Z                    23
    T_UPDATED_X                     -400
    T_UPDATED_Y                     -300
    T_UPDATED_Z                     -700
    T_DELETED_X                     -400
    T_DELETED_Y                     -300
    T_DELETED_Z                     -700

    V_INSERTED_X                    11
    V_INSERTED_Y                    12
    V_INSERTED_Z                    23
    V_UPDATED_X                     -400
    V_UPDATED_Y                     -300
    V_UPDATED_Z                     -700
    V_DELETED_X                     -400
    V_DELETED_Y                     -300
    V_DELETED_Z                     -700
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

