#coding:utf-8

"""
ID:          25e335f802
ISSUE:       https://www.sqlite.org/src/tktview/25e335f802
TITLE:       Query "<A> left join <B> inner join <C> on <b_c_expr> on <a_b_expr>" caused left join behave like inner join.
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table aaa (a int);
    create table bbb (b int);
    create table ccc (c int);

    insert into aaa values (1);
    insert into aaa values (2);

    insert into bbb values (1);
    insert into bbb values (2);

    insert into ccc values (2);

    set count on;
    select *
    from aaa a
    left join ccc c inner join bbb b on c.c = b.b on a.a = b.b;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    C <null>
    B <null>
    A 2
    C 2
    B 2
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
