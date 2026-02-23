#coding:utf-8

"""
ID:          4e8e4857d3
ISSUE:       https://www.sqlite.org/src/tktview/4e8e4857d3
TITLE:       Crash on query using an OR term in the WHERE clause
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(aa int, bb int);
    create index t1x1 on t1 computed by( abs(aa) );
    create index t1x2 on t1 computed by( abs(bb) );
    insert into t1 values(-2,-3);
    insert into t1 values(+2,-3);
    insert into t1 values(-2,+3);
    insert into t1 values(+2,+3);

    set count on;
    select * from t1
    where 
      ( (abs(aa)=1 and 1=2) or abs(aa)=2 )
      and abs(bb)=3
    ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    AA -2
    BB -3

    AA 2
    BB -3

    AA -2
    BB 3

    AA 2
    BB 3

    Records affected: 4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
