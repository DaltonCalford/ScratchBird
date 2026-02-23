#coding:utf-8

"""
ID:          4ef7e3cfca
ISSUE:       https://www.sqlite.org/src/tktview/4ef7e3cfca
TITLE:       Name resolution problem in sub-selects within triggers
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
    create table w(a int);
    create table x(a int);
    create table y(a int);
    create table z(a int);

    insert into x(a) values(5);
    insert into y(a) values(10);

    set term ^;
    create trigger w_ai after insert on w as
    begin
      insert into z select (select x.a+y.a from y) from x;
    end
    ^
    set term ;^
    commit;
    insert into w values(0);
    set count on;
    select * from z;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 15
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
