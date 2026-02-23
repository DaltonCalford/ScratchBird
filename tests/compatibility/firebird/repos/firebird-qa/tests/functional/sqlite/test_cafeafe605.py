#coding:utf-8

"""
ID:          cafeafe605
ISSUE:       https://www.sqlite.org/src/tktview/cafeafe605
TITLE:       UPDATE with causes error when its WHERE expression involves ROW_NUMBER()OVER() call
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test (f01 int);
    insert into test(f01) values (0);
    insert into test(f01) values (1);
    insert into test(f01) values (2);
    set count on;
    update test set f01 = 0 where f01 = (select rnk from (select row_number()over(order by f01) as rnk from test) where rnk = 1);
    set count off;
    select * from test order by f01;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 1
    F01 0
    F01 0
    F01 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
