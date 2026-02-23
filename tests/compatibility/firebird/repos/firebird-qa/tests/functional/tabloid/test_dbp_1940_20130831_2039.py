#coding:utf-8

"""
ID:          tabloid.dbp-1940-20130831-2039
TITLE:       Common SQL. Check correctness of the results
DESCRIPTION: 
FBTEST:      functional.tabloid.dbp_1940_20130831_2039
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-dbp-1940.fbk')

test_script = """
    set list on;
    with recursive
    n as (
      select 0 n from rdb$database union all select s.n+1 from n s where s.n<29
    )
    ,b1 as (
        select min(mt) st 
        from (
            select yyy.*, bbb.*, 
            sum(bv)over(partition by yyy.id) sv,
            max(tm)over(partition by yyy.id) mt
            from yyy join bbb on vi = yyy.id
             ) z
        where sv = 255
    )
    ,b2 as (
        select dateadd(n.n second to b1.st) f01
        from b1, n
    )
    select f01
        ,(select count(distinct id)
         from (
            select yyy.*, bbb.*, 
            sum(bv)over(partition by yyy.id) sv
            from yyy join bbb on vi = yyy.id
            where tm <= f01
          ) x
          where sv >= 255/3 and sv < 2*255/3
        ) f02
    from b2
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
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
