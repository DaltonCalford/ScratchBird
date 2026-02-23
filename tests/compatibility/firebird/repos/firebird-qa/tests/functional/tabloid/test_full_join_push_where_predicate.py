#coding:utf-8

"""
ID:          issue.full-join-push-where-predicate
TITLE:       WHERE-filter must be applied after FULL JOIN result
DESCRIPTION:
  See (rus): https://www.sql.ru/forum/1326682/dva-cte-ih-full-join-i-uslovie-daut-nekorrektnyy-rezultat
      Confirmed bug on 2.5.9.27151.
      Checked on 3.0.6.33322, 4.0.0.2073 -- all fine.
FBTEST:      full_join_push_where_predicate
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table t (
        f1  varchar(10),
        f2  integer
    );

    insert into t (f1, f2) values ('a', 1);
    insert into t (f1, f2) values ('b', 1);
    insert into t (f1, f2) values ('c', 1);
    insert into t (f1, f2) values ('b', 2);
    insert into t (f1, f2) values ('c', 2);
    insert into t (f1, f2) values ('d', 2);
    commit;

    with
      t1 as (select f1 from t where f2 = 1)
     ,t2 as (select f1 from t where f2 = 2)
    select
      t1.f1 t1_f1,
      t2.f1 t2_f1
    from
      t1 full join t2 on t1.f1 = t2.f1
    where t1.f1 is null
    order by 1,2
    ;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    T1_F1                           <null>
    T2_F1                           d
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
