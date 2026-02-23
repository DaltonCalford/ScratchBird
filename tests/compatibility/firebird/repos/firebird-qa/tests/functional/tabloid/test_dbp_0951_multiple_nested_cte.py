#coding:utf-8

"""
ID:          tabloid.dbp-0951-multiple-nested-cte
TITLE:       Query for test multiple CTEs
DESCRIPTION: 
FBTEST:      functional.tabloid.dbp_0951_multiple_nested_cte
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-dbp-0951.fbk')

test_script = """
    set list on;
    with dup as(select 1 i from rdb$database union all select 2 from rdb$database)
    ,mx as
    (select iif(d.i=1,q, v) q, iif(d.i=1, v, q) v, 1 d
        from(
            select distinct
            iif(ari<=tbi, ari, tbi) q,
            iif(ari<=tbi, tbi, ari) v
            from pdata
            where ari<>tbi
        )t cross join dup d
    )
    
    ,m2 as
    (
        select q,v,min(dx) d
        from(
            select mp.q, iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from mx mp left join mx on mp.d=1 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    ,m3 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m2 mp left join mx on mp.d=2 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    ,m4 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m3 mp left join mx on mp.d=3 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m5 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m4 mp left join mx on mp.d=4 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m6 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m5 mp left join mx on mp.d=5 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m7 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m6 mp left join mx on mp.d=6 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m8 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m7 mp left join mx on mp.d=7 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m9 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m8 mp left join mx on mp.d=8 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m10 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m9 mp left join mx on mp.d=9 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m11 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m10 mp left join mx on mp.d=10 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m12 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m11 mp left join mx on mp.d=11 and mp.v=mx.q and mp.q<>mx.v cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m13 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m12 mp left join mx on mp.d=12 and mp.v=mx.q and mp.q<>mx.v cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m14 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m13 mp left join mx on mp.d=13 and mp.v=mx.q and mp.q<>mx.v cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m15 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m14 mp left join mx on mp.d=14 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m16 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m15 mp left join mx on mp.d=15 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m17 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m16 mp left join mx on mp.d=16 and mp.v=mx.q and mp.q<>mx.v
            cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m18 as
    (
        select q,v,min(dx) d 
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m17 mp left join mx on mp.d=17 and mp.v=mx.q and mp.q<>mx.v cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
        group by q,v
    )
    
    ,m19 as
    (
        select
            cast(q as varchar(12)) q
            ,cast(v as varchar(12)) ||',' v
            ,min(dx) d
            ,sign(v-q)*dense_rank()over(partition by q order by abs(q-v)) rk
        from(
            select mp.q,iif(d.i=1, mp.v, mx.v) v,mp.d+d.i-1 dx
            from m18 mp left join mx on mp.d=18 and mp.v=mx.q and mp.q<>mx.v cross join dup d where d.i=1 or d.i=2 and mx.v>0
        )t
    group by t.q,t.v
    )
    ,p as (
        select
            q, max(d) d
            ,  max(iif(rk=-59,v,'')) || max(iif(rk=-58,v,'')) || max(iif(rk=-57,v,'')) || max(iif(rk=-56,v,'')) || max(iif(rk=-55,v,''))
            || max(iif(rk=-54,v,'')) || max(iif(rk=-53,v,'')) || max(iif(rk=-52,v,'')) || max(iif(rk=-51,v,'')) || max(iif(rk=-50,v,''))
            || max(iif(rk=-49,v,'')) || max(iif(rk=-48,v,'')) || max(iif(rk=-47,v,'')) || max(iif(rk=-46,v,'')) || max(iif(rk=-45,v,''))
            || max(iif(rk=-44,v,'')) || max(iif(rk=-43,v,'')) || max(iif(rk=-42,v,'')) || max(iif(rk=-41,v,'')) || max(iif(rk=-40,v,''))
            || max(iif(rk=-39,v,'')) || max(iif(rk=-38,v,'')) || max(iif(rk=-37,v,'')) || max(iif(rk=-36,v,'')) || max(iif(rk=-35,v,''))
            || max(iif(rk=-34,v,'')) || max(iif(rk=-33,v,'')) || max(iif(rk=-32,v,'')) || max(iif(rk=-31,v,'')) || max(iif(rk=-30,v,''))
            || max(iif(rk=-29,v,'')) || max(iif(rk=-28,v,'')) || max(iif(rk=-27,v,'')) || max(iif(rk=-26,v,'')) || max(iif(rk=-25,v,''))
            || max(iif(rk=-24,v,'')) || max(iif(rk=-23,v,'')) || max(iif(rk=-22,v,'')) || max(iif(rk=-21,v,'')) || max(iif(rk=-20,v,''))
            || max(iif(rk=-19,v,'')) || max(iif(rk=-18,v,'')) || max(iif(rk=-17,v,'')) || max(iif(rk=-16,v,'')) || max(iif(rk=-15,v,''))
            || max(iif(rk=-14,v,'')) || max(iif(rk=-13,v,'')) || max(iif(rk=-12,v,'')) || max(iif(rk=-11,v,'')) || max(iif(rk=-10,v,''))
            || max(iif(rk=-09,v,'')) || max(iif(rk=-08,v,'')) || max(iif(rk=-07,v,'')) || max(iif(rk=-06,v,'')) || max(iif(rk=-05,v,''))
            || max(iif(rk=-04,v,'')) || max(iif(rk=-03,v,'')) || max(iif(rk=-02,v,'')) || max(iif(rk=-01,v,''))
            || q || ','
            || max(iif(rk= 01,v,'')) || max(iif(rk= 02,v,'')) || max(iif(rk= 03,v,'')) || max(iif(rk= 04,v,'')) || max(iif(rk= 05,v,''))
            || max(iif(rk= 06,v,'')) || max(iif(rk= 07,v,'')) || max(iif(rk= 08,v,'')) || max(iif(rk= 09,v,''))
            || max(iif(rk= 10,v,'')) || max(iif(rk= 11,v,'')) || max(iif(rk= 12,v,'')) || max(iif(rk= 13,v,'')) || max(iif(rk= 14,v,''))
            || max(iif(rk= 15,v,'')) || max(iif(rk= 16,v,'')) || max(iif(rk= 17,v,'')) || max(iif(rk= 18,v,'')) || max(iif(rk= 19,v,''))
            || max(iif(rk= 20,v,'')) || max(iif(rk= 21,v,'')) || max(iif(rk= 22,v,'')) || max(iif(rk= 23,v,'')) || max(iif(rk= 24,v,''))
            || max(iif(rk= 25,v,'')) || max(iif(rk= 26,v,'')) || max(iif(rk= 27,v,'')) || max(iif(rk= 28,v,'')) || max(iif(rk= 29,v,''))
            || max(iif(rk= 30,v,'')) || max(iif(rk= 31,v,'')) || max(iif(rk= 32,v,'')) || max(iif(rk= 33,v,'')) || max(iif(rk= 34,v,''))
            || max(iif(rk= 35,v,'')) || max(iif(rk= 36,v,'')) || max(iif(rk= 37,v,'')) || max(iif(rk= 38,v,'')) || max(iif(rk= 39,v,''))
            || max(iif(rk= 40,v,'')) || max(iif(rk= 41,v,'')) || max(iif(rk= 42,v,'')) || max(iif(rk= 43,v,'')) || max(iif(rk= 44,v,''))
            || max(iif(rk= 45,v,'')) || max(iif(rk= 46,v,'')) || max(iif(rk= 47,v,'')) || max(iif(rk= 48,v,'')) || max(iif(rk= 49,v,''))
            || max(iif(rk= 50,v,'')) || max(iif(rk= 51,v,'')) || max(iif(rk= 52,v,'')) || max(iif(rk= 53,v,'')) || max(iif(rk= 54,v,''))
            || max(iif(rk= 55,v,'')) || max(iif(rk= 56,v,'')) || max(iif(rk= 57,v,'')) || max(iif(rk= 58,v,'')) || max(iif(rk= 59,v,''))
            as s
        from m19
        group by q
    )
    select count(*) c,max(d) d, left(s, char_length(s)-1) s
    from p
    group by s
    order by 1,2,3
    ;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
    C                               2
    D                               1
    S                               4,37
    C                               2
    D                               1
    S                               8,38
    C                               4
    D                               3
    S                               13,14,39,40
    C                               6
    D                               4
    S                               15,16,41,42,43,47
    C                               6
    D                               4
    S                               21,22,48,49,50,51
    C                               7
    D                               2
    S                               10,25,26,27,28,29,30
    C                               7
    D                               4
    S                               17,18,19,20,44,45,46
    C                               8
    D                               2
    S                               11,12,31,32,33,34,35,36
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
