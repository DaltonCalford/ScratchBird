#coding:utf-8

"""
ID:          f7d890858f
ISSUE:       https://www.sqlite.org/src/tktview/f7d890858f
TITLE:       Segfault when running query that uses window functions
DESCRIPTION:
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test (f01 integer primary key ) ;
    insert into test values ( 99 ) ;

    set count on;
    select
        exists(
            select count(*)over()
            from test
            order by (select sum(f01)over() from test)
        ) as chk
    from rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CHK <true>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
