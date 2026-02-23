#coding:utf-8

"""
ID:          ce82323194
ISSUE:       https://www.sqlite.org/src/tktview/ce82323194
TITLE:       Duplicate CTE name gives incorrect result
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    --set list on;
    set heading off;
    create table t1(id integer primary key, name varchar(10));
    create view v2 as
    with t4(name) as
    (
        select 'a' from rdb$database union all select 'b' from rdb$database 
    )
    select name name from t4
    ;

    create view v3 as
    with t4(att, val, act) as (
        select 'c', 'd', 'e' from rdb$database union all
        select 'f', 'g', 'h' from rdb$database
    )
    select d.id id, p.name protocol, t.att att, t.val val, t.act act
    from t1 d
    cross join v2 p
    cross join t4 t;

    insert into t1 values (1, 'john');
    insert into t1 values (2, 'james');
    insert into t1 values (3, 'jingle');
    insert into t1 values (4, 'himer');
    insert into t1 values (5, 'smith');

    set count on;
    select * from v3;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    1 a        c      d      e
    2 a        c      d      e
    3 a        c      d      e
    4 a        c      d      e
    5 a        c      d      e
    1 a        f      g      h
    2 a        f      g      h
    3 a        f      g      h
    4 a        f      g      h
    5 a        f      g      h
    1 b        c      d      e
    2 b        c      d      e
    3 b        c      d      e
    4 b        c      d      e
    5 b        c      d      e
    1 b        f      g      h
    2 b        f      g      h
    3 b        f      g      h
    4 b        f      g      h
    5 b        f      g      h
    Records affected: 20
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
