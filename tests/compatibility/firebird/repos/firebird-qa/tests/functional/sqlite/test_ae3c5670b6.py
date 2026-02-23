#coding:utf-8

"""
ID:          ae3c5670b6
ISSUE:       https://www.sqlite.org/src/tktview/ae3c5670b6
TITLE:       Bug caused by factoring of constants in trigger programs
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int, b int, c int);
    create table t2(e int, f int);
    create table empty(x int);
    create table not_empty(x int);
    create table t4(x int);
    create table t5(g int, h int, i int);

    create index i1 on t1(a, c);
    create index i2 on t1(b, c);
    create index i3 on t2(e);

    insert into t1 values(1, 2, 3);
    insert into t2 values(1234567, 3);
    insert into not_empty values(2);

    set term ^;
    create trigger trig before insert on t4 as
    begin
        insert into t5
        select * from t1
        where
            ( a in (select x from empty)
              or b in (select x from not_empty)
            )
        and c in (select f from t2 where e=1234567);
    end
    ^
    set term ;^
    commit;

    insert into t4 values(0) returning *;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    X 0
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
