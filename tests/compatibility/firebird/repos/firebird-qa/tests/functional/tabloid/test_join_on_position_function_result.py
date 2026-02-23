#coding:utf-8

"""
ID:          tabloid.join-on-position-function-result
TITLE:       Records with NULLs could be lost from resultset.
DESCRIPTION: 
  http://www.sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1009792&msg=14032086
FBTEST:      functional.tabloid.join_on_position_function_result
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t(id int, s varchar(30));
    commit;
    insert into t values(1, 'aaa');
    insert into t values(11, 'bbb');
    insert into t values(111, 'ccc');
    insert into t values(12, 'ddd');
    insert into t values(123, 'eee');
    insert into t values(1234, 'fff');
    commit;

    set list on;
    select t.id, t.s, x.p, position(','||cast(t.id as varchar(11))||',', x.p) k
    from t
      left join (select ',123,12,11,' p from rdb$database) x
      on position(','||cast(t.id as varchar(11))||',', x.p)>0
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    S                               aaa
    P                               <null>
    K                               <null>
    ID                              11
    S                               bbb
    P                               ,123,12,11,
    K                               8
    ID                              111
    S                               ccc
    P                               <null>
    K                               <null>
    ID                              12
    S                               ddd
    P                               ,123,12,11,
    K                               5
    ID                              123
    S                               eee
    P                               ,123,12,11,
    K                               1
    ID                              1234
    S                               fff
    P                               <null>
    K                               <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
