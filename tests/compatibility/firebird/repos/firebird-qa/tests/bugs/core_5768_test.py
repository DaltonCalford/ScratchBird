#coding:utf-8

"""
ID:          issue-6031
ISSUE:       6031
TITLE:       Implement FILTER-clause for aggregate functions (introduced in SQL:2003).
  This syntax allows for filtering before aggregation.
DESCRIPTION:
JIRA:        CORE-5768
FBTEST:      bugs.core_5768
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(x int, y int, q int);
    commit;
    insert into test(x,y,q) values(null, null, 100);
    insert into test(x,y,q) values(null, null, 200);
    insert into test(x,y,q) values(1000, null, 111);
    insert into test(x,y,q) values(1000, null, 222);
    insert into test(x,y,q) values(1000, 1000, 555);
    insert into test(x,y,q) values(1000, 1000, 777);
    commit;
    set list on;
    select
         sum( iif(x is null, q, null) ) sum_q_iif_x_is_null
        ,sum( q )filter( where x is null ) sum_q_filter_x_is_null
        ,min( sum_q_iif_y_is_null_over ) filter( where true ) chk_filter_where_true
        ,min( sum_q_filter_y_is_null_over ) filter( where null is null ) chk_filter_where_n_is_n
        ,min( 1 ) filter( where null ) chk_filter_where_null ------------------------------------- allowed! boolean but without left part ?..
        ,min( 1 ) filter( where not null ) chk_filter_where_not_null
    from (
        select x, y, q
        ,sum( iif(y is null, q, null) ) over() sum_q_iif_y_is_null_over
        ,sum( q )filter( where y is null) over() sum_q_filter_y_is_null_over
        from test
    )
    ;

"""

act = isql_act('db', test_script)

expected_stdout = """
    SUM_Q_IIF_X_IS_NULL             300
    SUM_Q_FILTER_X_IS_NULL          300
    CHK_FILTER_WHERE_TRUE           633
    CHK_FILTER_WHERE_N_IS_N         633
    CHK_FILTER_WHERE_NULL           <null>
    CHK_FILTER_WHERE_NOT_NULL       <null>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
