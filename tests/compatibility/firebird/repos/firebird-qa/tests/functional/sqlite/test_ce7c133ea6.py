#coding:utf-8

"""
ID:          ce7c133ea6
ISSUE:       https://www.sqlite.org/src/tktview/ce7c133ea6
TITLE:       Foreign key constraint fails when it should succeed.
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int not null, b int, constraint t1_unq unique(a,b));
    create table t2(w int,x int,y int, constraint t2_fk foreign key(x,y) references t1(a,b));
    alter table t1 add constraint t1_pk primary key(a);

    insert into t1 values(100,200);
    insert into t2 values(300,100,200);
    set count on;
    update t1 set b = 200 where a = 100;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
