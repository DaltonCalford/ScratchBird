#coding:utf-8

"""
ID:          tabloid.dbp-1940-20080912-1005
TITLE:       Common SQL. Check correctness of the results
DESCRIPTION: 
FBTEST:      functional.tabloid.dbp_1940_20080912_1005
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-dbp-1940.fbk')

test_script = """
    set list on;
    select
        x.t as f01
        ,(
            select count(distinct iif(bx.s>=85 and bx.s<=170,bx.vi, null))
            from (
                select
                    x2.vi
                    ,x2.s
                    ,row_number()over(partition by x2.vi order by x2.t desc) rn2
                from (
                    select b.vi, b.tm t
                        ,(select sum(b2.bv) from bbb b2 where b2.vi=b.vi and b2.tm<=b.tm) s
                    from bbb b
                    ) x2
                where x2.t<=x.t
                ) bx
            where bx.rn2=1
        ) as f02
    from (
        select dateadd(q2.s second to q3.t) t
        from (  select min(q1.tm) t
                from (
                    select b.tm
                       ,(select sum(b2.bv) from bbb b2 where b.vi=b2.vi and b2.tm<=b.tm) s
                    from bbb b
                    ) q1
                where q1.s=255
              ) q3
            , (select row_number()over(order by bbb.qi)-1 s from bbb rows 30) q2
    ) x
    order by 1,2;
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
