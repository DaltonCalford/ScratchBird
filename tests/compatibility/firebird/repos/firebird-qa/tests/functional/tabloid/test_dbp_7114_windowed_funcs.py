#coding:utf-8

"""
ID:          tabloid.dbp-7114-windowed-funcs
TITLE:       Common SQL. Check correctness of the results
DESCRIPTION: 
FBTEST:      functional.tabloid.dbp_7114_windowed_funcs
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-dbp-7114.fbk')

test_script = """
    delete from tresult;
    insert into tresult(ip_a, ip_b, cnt)
    select
         d1||'.'||d2||'.'||d3||'.'||d4 as ip_a
        ,d5||'.'||d6||'.'||d7||'.'||d8 as ip_b
        ,1 + abs( cast( d1-d5 as bigint ) * 16777216 + (d2-d6) * 65536 + (d3-d7) * 256 + d4 - d8 ) as cnt
    from
    (
        select rn,
            cvsum as d1,
            lead(cvsum, 1, 0)over(order by rn) as d2,
            lead(cvsum, 2, 0)over(order by rn) as d3,
            lead(cvsum, 3, 0)over(order by rn) as d4,
            iif(cnt - rn - 4 >= 0, lead(cvsum, 4, 0)over(order by rn), lag(cvsum, cnt_int, 0)over(order by rn)) as d5,
            iif(cnt - rn - 4 >= 0, lead(cvsum, 5, 0)over(order by rn), lag(cvsum, cnt_int - 1, 0)over(order by rn)) as d6,
            iif(cnt - rn - 4 >= 0, lead(cvsum, 6, 0)over(order by rn), lag(cvsum, cnt_int - 2, 0)over(order by rn)) as d7,
            iif(cnt - rn - 4 >= 0, lead(cvsum, 7, 0)over(order by rn), lag(cvsum, cnt_int - 3, 0)over(order by rn)) as d8
        from
        (
            select cvsum
                ,row_number()over(order by dt asc, tbi desc) as rn
                ,count(*)over() as cnt
                ,count(*)over() / 4 * 4 as cnt_int
            from
            (
                select t.id as tbi,
                    coalesce(min(dts), '99991231') as dt,
                    255 - sum(coalesce(cv, 0)) as cvsum
                from ptube t
                left join pdata d on d.tbi = t.id
                group by t.id
            ) as a1
        ) as a2
    ) as a3
    where mod(rn, 4) = 1
    ;
    
    insert into tresult(ip_a, ip_b, cnt)
    select
        n5.ip as ip_a
        ,coalesce(
            lead(n5.ip)over(order by n5.adr1)
            ,first_value(n5.ip)over (order by n5.adr1)
         ) as ip_b
        ,abs(   n5.ip_num
                - coalesce( lead(n5.ip_num)over(order by n5.adr1)
                            ,first_value(n5.ip_num)over(order by n5.adr1)
                          )
             ) + 1 as cnt
    from
    (
        select
            n4.adr1,
            cast(max( iif(n4.adr2=1, n4.afree,null) ) as varchar(12)) || '.' ||
            coalesce(cast(max( iif(n4.adr2=2, n4.afree,null) ) as varchar(12)),'0')|| '.' ||
            coalesce(cast(max( iif(n4.adr2=3, n4.afree,null) ) as varchar(12)),'0')|| '.' ||
            coalesce(cast(max( iif(n4.adr2=4, n4.afree,null) ) as varchar(12)),'0') as ip,
    
            cast(max(iif(n4.adr2=1, n4.afree,0)) as bigint) * 256 * 256 * 256 +
            coalesce(cast(max( iif(n4.adr2=2, n4.afree,0) ) as bigint),0) * 256 * 256 +
            coalesce(cast(max( iif(n4.adr2=3, n4.afree,0) ) as bigint),0) * 256 +
            coalesce(cast(max( iif(n4.adr2=4, n4.afree,0) ) as bigint),0) as ip_num
        from (
            select n3.*, row_number()over(partition by n3.adr1 order by n3.dts , n3.vid desc) adr2
            from (
                select n2.*, (row_number()over(order by n2.dts, n2.vid desc)-1)/4 adr1
                from
                (
                    select
                        coalesce(n1.tbi, ptube.id) as vid,
                        coalesce(n1.dts, '99991231') as  dts,
                        255 - coalesce(cvsum,0) as afree 
                    from(
                        select pdata.tbi,  min(pdata.dts) as dts ,sum (pdata.cv) as cvsum
                        from pdata
                        group by pdata.tbi
                    ) n1
                    right join ptube on n1.tbi = ptube.id
                ) n2 
            ) n3    
       ) n4
       group by n4.adr1
    ) n5
    ;
    
    set list on;
    select count(*) cnt_mism
    from (
        select 1 x
        from tresult
        group by ip_a, ip_b, cnt
        having count(*)<>2
    );
    set list off;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
    CNT_MISM                        0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
