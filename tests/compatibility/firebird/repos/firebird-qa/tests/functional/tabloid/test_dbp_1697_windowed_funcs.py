#coding:utf-8

"""
ID:          tabloid.dbp-1697-windowed-funcs
TITLE:       Query for test SUM()OVER() and COUNT()OVER().
DESCRIPTION: 
FBTEST:      functional.tabloid.dbp_1697_windowed_funcs
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-dbp-1697.fbk')

test_script = """
    delete from tresult;
    --------------------
    insert into tresult
    with
    x as(
        select ari q, tbi v, sum(cv) cv from pdata group by ari, tbi
    )
    ,y as (
        select q, v
           ,sum(iif( tc = 'Romania', cv, null))over(partition by q) rmn
           ,sum(iif( tc = 'France', cv, null))over(partition by q) frn
           ,sum(iif( tc = 'Belgium', cv, null))over(partition by q) bgm
        from x
        join ptube on v=id
    )
    
    select ari
    from pdata x
    join ptube on tbi=id
    group by ari
    having
        coalesce(sum(iif( tc = 'Romania', cv, null)),0) between 0 and 50
        and sum(iif( tc = 'France', cv, null)) between 50 and 100
        and sum(iif( tc = 'Belgium', cv, null)) between 100 and 150
        and not exists(
            select q
            from (
                select q, v
                from y -- do not add `x` ==> CTE 'X' has cyclic dependencies
                where coalesce(rmn,0) between 0 and 50 and frn between 50 and 100 and bgm between 100 and 150
            ) y
            join pdata on tbi=v
            where q<>x.ari and ari=x.ari
            group by q
            having min(v)<>max(v)
        )
    ;
    
    insert into tresult
    with
    t as (
        select ari
             ,sum(case when tc = 'Romania' then cv else 0 end) as rmn
             ,sum(case when tc = 'France' then cv else 0 end) as frn
             ,sum(case when tc = 'Belgium' then cv else 0 end) as bgm
        from pdata join ptube on ptube.id = pdata.tbi
        group by ari
    )
    ,t1 as(
        select ari, rmn, frn, bgm
        from t
        where
            t.rmn between 0 and 50
            and t.frn between 50 and 100
            and t.bgm  between 100 and 150
    )
    ,t2 as(
        select distinct ub1.ari, ub1.tbi v1, ub2.tbi v2
        from pdata ub1
        join pdata ub2 on ub2.ari = ub1.ari and ub1.tbi < ub2.tbi
    )
    ,f as(
        select t1.ari, t2.v1, t2.v2
             ,count(*)over(partition by bin_or( bin_shl(t2.v1,32), t2.v2)) s
        from t1
        join t2 on t2.ari = t1.ari
    )
    select ari from f
    group by ari
    having max(s) = 1
    ;

 
    -- check result of subsequent inserts (instead of heavy full join) for mimatches:
    ---------------
    select id,count(*) from tresult group by id having count(*)<>2;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
