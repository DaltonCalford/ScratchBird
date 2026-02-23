#coding:utf-8

"""
ID:          tabloid.dbp-1940-20061108-2141
TITLE:       Common SQL. Check correctness of the results
DESCRIPTION: 
FBTEST:      functional.tabloid.dbp_1940_20061108_2141
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-dbp-1940.fbk')

test_script = """
    set list on;
    select
        dateadd(n-1 second to dat) f01
        ,(select count(*) from
        (select  vi, sum(bv) as s from bbb where 
         dateadd(n-1 second to dat)>=tm
        group by  vi
        having sum(bv) between 85 and 170) t
        ) f02
    from 
    ( select row_number() over(order by qi) n from bbb rows 30 ) z
    cross join
    (
        select min(tm) as dat from bbb b
        where exists (
            select  vi, sum(bv) from bbb
            where  b.tm>=tm
            group by  vi
            having sum(bv)=255
                    )
    ) q
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
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
