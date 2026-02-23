#coding:utf-8

"""
ID:          d11a6e908f
ISSUE:       https://www.sqlite.org/src/tktview/d11a6e908f
TITLE:       Query planner fault on three-way nested join with compound inner SELECT
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1 (id integer primary key, data char(10));
    create table t2 (id integer primary key, data char(10));

    insert into t1(id,data) values(9,'nine-a');
    insert into t1(id,data) values(10,'ten-a');
    insert into t1(id,data) values(11,'eleven-a');
    insert into t2(id,data) values(9,'nine-b');
    insert into t2(id,data) values(10,'ten-b');
    insert into t2(id,data) values(11,'eleven-b');

    set count on;
    select id from (
      select id,data from (
         select * from t1 union all select * from t2
      )
      where id=10 order by data
    );
    set count off;
    commit;
    -----------------------
    recreate table t1(id integer, data char);
    recreate table t2(id integer, data char);
    insert into t1 values(4, 'a');
    insert into t2 values(3, 'b');
    insert into t1 values(2, 'c');
    insert into t2 values(1, 'd');

    set count on;
    select data, id from (
      select id, data from (
         select * from t1 union all select * from t2
      ) order by data
    );
    set count off;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 10
    ID 10
    Records affected: 2

    DATA a
    ID 4
    DATA b
    ID 3
    DATA c
    ID 2
    DATA d
    ID 1
    Records affected: 4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
