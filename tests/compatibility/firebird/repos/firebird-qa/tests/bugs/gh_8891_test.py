#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8891
TITLE:       Regression. Result of cross join of two procedures + unioned with common datasource (e.g. rdb$database) is wrong
DESCRIPTION:
NOTES:
    [17.02.2026] pzotov
    Confirmed bug on 6.0.0.1428-14c6de6; 5.0.4.1762-7c85eae.
    Checked on 6.0.0.1454-b45aa4e; 5.0.4.1765-2c1e56d.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(x int);
    insert into test select row_number()over() from rdb$types rows 5;
    commit;

    set term ^;
    create or alter procedure sp_issue_row (a_x int default 1) returns (x int) as
    begin
        x = a_x + 1;
        suspend;
    end
    ^ 
    set term ;^
    commit;

    set count on;
    with
    c as (
        select first 1 p2.x
        from sp_issue_row p1
        cross join sp_issue_row (p1.x) p2
        UNION ALL
        select 4 from rdb$database
    )
    select c.x as c_x, n.x as n_x
    from c
    join test n on n.x = c.x
    order by 1,2
    ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C_X 3
    N_X 3

    C_X 4
    N_X 4

    Records affected: 2
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
