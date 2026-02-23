#coding:utf-8

"""
ID:          issue-5348
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5348
TITLE:       ISQL plan output is unexpectedly truncated after a query is simplified to become shorter
DESCRIPTION:
    Start of discussion: letter to dimitr, 30-dec-2015 13:57; its subject refers to core-4708.
    It was found that explained plan produced by ISQL is unexpectedly ends on WI-V3.0.0.32256.
    This test uses that query, but instead of verifying plan text itself (which can be changed in the future)
    it is sufficient to check only that plan does NOT contain lines with ellipsis or 'truncated' or 'error'.
    This mean that 'expected_stdout' section must be EMPTY. Otherwise expected_stdout will contain info
    about error or invalid plan.
NOTES:
    [04.03.2023] pzotov
    Separated query for FB 5.x: length of explained plan in previous version of query exceeded 64K.
    Discussion with dimitr:  17.01.2023 13:17
JIRA:        CORE-5061
FBTEST:      bugs.core_5061
NOTES:
    [31.07.2023] pzotov
    Test excluded from execution after discuss with dimitr, letter 30.07.2023.
"""

import pytest
from firebird.qa import *

db = db_factory()

fb4x_test_sql = """
    set explain on;
    set planonly;

    with recursive
    r1 as (
      select 1 as i from rdb$database
      union all
      select r.i+1 from r1 r where r.i < 2
    )
    --select count(*) from r1;

    ,r2 as (
      select first 1 row_number() over() i 
      from r1 ra
      full join r1 rb on rb.i=ra.i 
      group by ra.i 
      having count(*)>0 

      union all

      select rx.i+1 from r2 rx
      where rx.i+1 <= 2
    )
    --select count(*) from r2
    ,r3 as (
      select first 1 row_number() over() i 
      from r2 ra
      full join r2 rb on rb.i=ra.i 
      group by ra.i 
      having count(*)>0 

      union all

      select rx.i+1 from r3 rx
      where rx.i+1 <= 2
    )
    --select count(*) from r3
    ,r4 as (
      select first 1 row_number() over() i 
      from r3 ra
      full join r3 rb on rb.i=ra.i 
      group by ra.i 
      having count(*)>0 

      union all

      select rx.i+1 from r4 rx
      where rx.i+1 <= 2
    )
    ,rn as (
      select row_number() over() i 
      from rdb$database r full join rdb$database r2 on r2.rdb$relation_id=r.rdb$relation_id 
      group by r.rdb$relation_id 
      having count(*)>0 
      order by r.rdb$relation_id 
      rows 1 to 1
    )
    select 
        char_length(mon$explained_plan)
       ,(select count(*) from r4)
       ,(select count(*) from rn)
       --,(select count(*) from rn)
    from mon$statements
    ;
"""

#######################################################################

fb5x_test_sql = """
    set explain on;
    set planonly;
    with recursive
    r1 as (
      select 1 as i from rdb$database
      union all
      select r.i+1 from r1 r where r.i < 2
    )
    --select count(*) from r1;

    ,r2 as (
      select first 1 row_number() over() i 
      from r1 ra
      full join r1 rb on rb.i=ra.i 
      group by ra.i 
      having count(*)>0 

      union all

      select rx.i+1 from r2 rx
      where rx.i+1 <= 2
    )
    --select count(*) from r2
    ,r3 as (
      select first 1 row_number() over() i 
      from r2 ra
      full join r2 rb on rb.i=ra.i 
      group by ra.i 
      having count(*)>0 

      union all

      select rx.i+1 from r3 rx
      where rx.i+1 <= 2
    )
    --select count(*) from r3
    ,r4 as (
      select first 1 row_number() over() i 
      from r3 ra
      full join r3 rb on rb.i=ra.i 
      group by ra.i 
      having count(*)>0 

      union all

      select rx.i+1 from r4 rx
      where rx.i+1 <= 2
    )
    /*
    ,rn as (
      select row_number() over() i 
      from rdb$database r full join rdb$database r2 on r2.rdb$relation_id=r.rdb$relation_id 
      group by r.rdb$relation_id 
      having count(*)>0 
      order by r.rdb$relation_id 
      rows 1 to 1
    )
    -- */
    select 
        char_length(mon$explained_plan)
       ,( select count(*) from (select i from r4) )
    from mon$statements

    union all select 1,row_number()over() from rdb$database where exists(select 1, row_number()over() from rdb$database group by 1 having True rows 1)
    union all select 1,row_number()over() from rdb$database where exists(select 1, row_number()over() from rdb$database group by 1 having True rows 1)
    ;
"""

act = python_act('db')

@pytest.mark.skip("No sense to verify length of generated explained plan because it can be vary after any change in optimizer.")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = ''
    act.isql(switches=['-q'], input = fb4x_test_sql if act.is_version('<5') else fb5x_test_sql)
    assert act.clean_stderr == act.clean_expected_stderr

    i = 0
    for line in act.stdout.splitlines():
        i += 1
        if '...' in line or 'truncated' in line or 'error' in line:
            pytest.fail(f"Plan is truncated or empty. Found at line {i}")
