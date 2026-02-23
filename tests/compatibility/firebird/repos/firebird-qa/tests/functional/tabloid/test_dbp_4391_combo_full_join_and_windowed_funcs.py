#coding:utf-8

"""
ID:          tabloid.dbp-4391-combo-full-join-and-windowed-funcs
TITLE:       Common SQL. Check correctness of the results
DESCRIPTION: 
FBTEST:      functional.tabloid.dbp_4391_combo_full_join_and_windowed_funcs
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-dbp-4391.fbk')

test_script = """
    set list on;
    with
    n as (
        select rank()over(partition by pdata.ari order by pdata.dts, ptube.id ) rnk,
        pdata.dts, pdata.ari, pdata.tbi, ptube.tc
        from pdata
        join ptube on pdata.tbi = ptube.id
    )
    ,h as (
        select t.*, rank()over(partition by q,g order by t,v) - 1 as r
        from(
                select t.*, y.tc, sum( iif( y.tc='rio',1,0) )over( partition by q order by t,v ) g
                from( select * from pdata )t (t,q,v,w), ptube y
                where t.v = y.id
            )t
    )
    ,x as (
        select --distinct
        n1.ari, n1.tbi as v1, n2.tbi as v2, n3.tbi as v3
        from n n1
        join n n2 on n1.ari = n2.ari and n1.rnk = n2.rnk - 1 and n1.tc = 'rio' and n2.tc = 'foo'
        join n n3 on n1.ari = n3.ari and n1.rnk = n3.rnk - 2 and n3.tc = 'bar'
    )
    ,z as(
        select --distinct
            q as ari
            ,max(iif(r=0,v,'')) v1
            ,max(iif(r=1,v,'')) v2
            ,max(iif(r=2,v,'')) v3
        from h where r<=3
        group by q,g
        having
            count(*)>=3
            and max(iif(r=0,tc,''))='rio'
            and max(iif(r=1,tc,''))='foo'
            and max(iif(r=2,tc,''))='bar'
    
    )
    select count(*) cnt
    from x full join z using(ari, v1, v2, v3)
    where
        x.ari is distinct from z.ari
        or x.v1 is distinct from z.v1
        or x.v2 is distinct from z.v2
        or x.v3 is distinct from z.v3
    ;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
    CNT                             0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
