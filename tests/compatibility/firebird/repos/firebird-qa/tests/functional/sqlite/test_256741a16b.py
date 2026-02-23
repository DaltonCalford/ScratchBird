#coding:utf-8

"""
ID:          256741a16b
ISSUE:       https://www.sqlite.org/src/tktview/256741a16b
TITLE:       null pointer dereference caused by window functions in result-set of EXISTS(SELECT ...)
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 int);
    insert into t0(c0) values (0);

    set count on;
    select * from t0 
    where 
        exists (
            select 1
            from (
                select min(c0)over() mw, cume_dist()over() cw from t0
            )
            where mw between 1 and 1 and cw between 1 and 1

        );
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
