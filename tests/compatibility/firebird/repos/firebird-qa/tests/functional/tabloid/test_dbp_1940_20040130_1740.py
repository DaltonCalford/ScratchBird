#coding:utf-8

"""
ID:          tabloid.dbp-1940-20040130-1740
TITLE:       Common SQL. Check correctness of the results
DESCRIPTION: 
FBTEST:      functional.tabloid.dbp_1940_20040130_1740
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-dbp-1940.fbk')

test_script = """
    set list on;
    with recursive
    a1 as (
        select min(q.tm) as mt from
         (select xxx.tm,
          (select count(vi) from
            (select y.vi, sum( bv ) as bv
             from bbb y
          where y.tm<=xxx.tm
          group by vi
            )ws where ws.bv=255   )as cntr
          from bbb xxx 
         )q where q.cntr>0
    )
    ,a2 as (
      select 0 i from rdb$database union all select r.i+1 from a2 as r where r.i<29
    )
    ,b1 as (
      select dateadd(a2.i second to a1.mt) as ww1
      from a1, a2
    )
    ,b2 as
    (select tm from bbb)
    ,c as (
      select *
      from b1, b2
      where b1.ww1>=b2.tm
    )
    --select * from c
    
    ,d1 as (
      select ww1,max(tm) as maxt
      from c group by ww1
    )
    -- select * from d1
    ,d2 as (
        select xxx.tm,
          (select count(vi) from
            (select vi, sum( bv ) as bv
        from bbb y
          where  y.tm<=xxx.tm
          group by vi
            )ws where ws.bv>=85  and ws.bv<=170 
          )as cntr
          from bbb xxx 
    )
    select distinct ww1 as f01, cntr as f02
    from d1,d2
    where d1.maxt=d2.tm
    order by 1,2
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    F01                             2003-01-01 01:11:00.0000
    F02                             1
    F01                             2003-01-01 01:11:01.0000
    F02                             3
    F01                             2003-01-01 01:11:02.0000
    F02                             4
    F01                             2003-01-01 01:11:03.0000
    F02                             4
    F01                             2003-01-01 01:11:04.0000
    F02                             3
    F01                             2003-01-01 01:11:05.0000
    F02                             3
    F01                             2003-01-01 01:11:06.0000
    F02                             3
    F01                             2003-01-01 01:11:07.0000
    F02                             3
    F01                             2003-01-01 01:11:08.0000
    F02                             3
    F01                             2003-01-01 01:11:09.0000
    F02                             3
    F01                             2003-01-01 01:11:10.0000
    F02                             4
    F01                             2003-01-01 01:11:11.0000
    F02                             5
    F01                             2003-01-01 01:11:12.0000
    F02                             4
    F01                             2003-01-01 01:11:13.0000
    F02                             4
    F01                             2003-01-01 01:11:14.0000
    F02                             3
    F01                             2003-01-01 01:11:15.0000
    F02                             3
    F01                             2003-01-01 01:11:16.0000
    F02                             3
    F01                             2003-01-01 01:11:17.0000
    F02                             4
    F01                             2003-01-01 01:11:18.0000
    F02                             4
    F01                             2003-01-01 01:11:19.0000
    F02                             4
    F01                             2003-01-01 01:11:20.0000
    F02                             4
    F01                             2003-01-01 01:11:21.0000
    F02                             4
    F01                             2003-01-01 01:11:22.0000
    F02                             4
    F01                             2003-01-01 01:11:23.0000
    F02                             4
    F01                             2003-01-01 01:11:24.0000
    F02                             4
    F01                             2003-01-01 01:11:25.0000
    F02                             4
    F01                             2003-01-01 01:11:26.0000
    F02                             4
    F01                             2003-01-01 01:11:27.0000
    F02                             4
    F01                             2003-01-01 01:11:28.0000
    F02                             4
    F01                             2003-01-01 01:11:29.0000
    F02                             4
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
