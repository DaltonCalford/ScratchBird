#coding:utf-8

"""
ID:          de7db14784
ISSUE:       https://www.sqlite.org/src/tktview/de7db14784
TITLE:       Subquery with limit clause fails as EXISTS operand
DESCRIPTION:
    Bug: query with 'limit N' returned nothing whereas without the limit clause returned rows (as it should.)
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t3(id_txt varchar(5) primary key, b varchar(5), x int);
    create table t4(c varchar(5), y int);
    insert into t3 values('one', 'i', 1);
    insert into t3 values('two', 'ii', 2);
    insert into t3 values('three', 'iii', 3);
    insert into t3 values('four', 'iv', 4);
    insert into t3 values('five', 'v', 5);

    insert into t4 values('FIVE',5);
    insert into t4 values('four',4);
    insert into t4 values('TWO',2);
    insert into t4 values('one',1);
    set count on;
    select id_txt from t3 where exists (select 1 from t4 where t3.id_txt = t4.c and t3.x = t4.y rows 1);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID_TXT one
    ID_TXT four
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
