#coding:utf-8

"""
ID:          af4556bb5c
ISSUE:       https://www.sqlite.org/src/tktview/af4556bb5c
TITLE:       Segfault while trying to prepare a malformed window-function query
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
    create table a(b int, c int);

    set count on;
    select c
    from a 
    group by c
    having(
        exists(
            select(
                sum(b)over(partition by (select min(distinct c) from a),c order by b)
            )
            from a
        )
    );
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0  
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
