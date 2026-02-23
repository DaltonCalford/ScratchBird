#coding:utf-8

"""
ID:          7a5279a25c
ISSUE:       https://www.sqlite.org/src/tktview/7a5279a25c
TITLE:       Segfault when running query that has WHERE expression with IN(...) containing LEAD()OVER() function
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    recreate table test(id int primary key, f01 int unique);
    insert into test(id, f01)
    select r, iif(mod(r,2)=0,1,-1) * r
    from (select row_number()over() r from rdb$types rows 20);
    commit;

    set count on;
    select a.id, a.f01
    from (
        select a.id, a.f01
        from test a
        join test b on b.f01 = a.f01
    ) a
    natural join test c
    where c.f01 in (
        (
            select (select coalesce(lead(2)over(), sum(a.f01)) from test a) as x
            from test d
            where d.f01 > 0
        )
    )
    order by id;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID 10
    F01 10
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
