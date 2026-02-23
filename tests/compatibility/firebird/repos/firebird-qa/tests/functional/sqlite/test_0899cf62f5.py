#coding:utf-8

"""
ID:          0899cf62f5
ISSUE:       https://www.sqlite.org/src/tktview/0899cf62f5
TITLE:       Segfault when running query that uses LEAD()OVER() and GROUP BY
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
    recreate table v0 ( v1 integer primary key ) ;
    insert into v0 values ( 10 ) ; 
    commit;
    set count on;
    select distinct v1, lead (v1) over() from v0 group by v1 order by 1, 2, 1 ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    V1 10
    LEAD <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
