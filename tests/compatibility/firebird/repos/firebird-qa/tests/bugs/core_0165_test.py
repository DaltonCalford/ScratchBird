#coding:utf-8

"""
ID:          issue-494
ISSUE:       494
TITLE:       Query using VIEW with UNION causes crash
DESCRIPTION:
NOTES:
Original test see im:
https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_24.script
JIRA:        CORE-165
FBTEST:      bugs.core_0165
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate view v_test1 ( id, x ) as select 1, 2 from rdb$database;
    recreate view v_test2 ( id, x ) as select 1, 2 from rdb$database;
    commit;

    recreate table test1 (
      id int not null,
      x int not null);

    recreate table test2 (
      id int not null,
      y int not null);

    recreate view v_test1 ( id, x ) as
    select id, x
    from test1
    union
    select id, x
    from test1;

    recreate view v_test2 ( id, y ) as
    select id, y
    from test2
    union
    select id, y
    from test2;
    commit;

    insert into test1 values(1, 123);
    insert into test1 values(2, 456);
    insert into test2 values(3, 151);
    insert into test2 values(2, 456);
    insert into test2 values(1, 123);
    commit;

    set list on;
    --set plan on;
    select i.id as id_1, i.x as x, j.id as id_2, j.y as y
    from v_test1 i, v_test2 j
    where i.id = j.id
    and j.y = (select max(x.y) from v_test2 x)
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID_1                            2
    X                               456
    ID_2                            2
    Y                               456
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

