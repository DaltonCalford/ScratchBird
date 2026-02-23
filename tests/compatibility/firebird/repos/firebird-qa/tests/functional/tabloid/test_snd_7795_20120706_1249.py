#coding:utf-8

"""
ID:          tabloid.snd-7795-20120706-1249
TITLE:       Common SQL. Check correctness of the results
DESCRIPTION: 
  NB: new datatype in FB 4.0 was introduces: numeric(38,0).
      It leads to additional ident of values when we show them in form "SET LIST ON",
      so we have to ignore all internal spaces - see added 'substitution' section below.
  
      Checked on:
          4.0.0.1635 SS: 1.824s.
          3.0.5.33182 SS: 1.387s.
FBTEST:      functional.tabloid.snd_7795_20120706_1249
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-snd-7795.fbk')

test_script = """
      set list on;
    with recursive
    n as(select -1 i from rdb$database union all select n.i+1 from n where n.i<1),
    c0 as
    (
        select
            gate
            ,dts
            ,coalesce(sum(purchase),0) purchase
            ,coalesce( sum(retail),0) retail
            ,iif( sum(purchase) is not null, iif( sum(retail) is not null, 'u', 'i' ), 'o' ) kind
            ,row_number()over(partition by gate order by dts desc) r
        from
        (
            select cast(i.igate as  char(12)) || 'a' gate, i.idate dts, i.icost purchase, null retail
            from  sndxi i
            union all
            select cast(ogate as  char(12)) || 'a', o.odate dts, null, o.ocost retail
            from  sndxo o
            union all
            select cast(igate as  char(12)) || 'b', i.idate, i.icost, null
            from snd1i i
            union all
            select cast(o.ogate as char(12)) || 'b', o.odate, null, o.ocost
            from snd1o o
        )a
        group by gate,dts
    )
    ,c1 as
    (
        select
            gate
            ,max(case when i=0 then r end) r
            ,max(case when i=0 then purchase end) purchase
            ,max(case when i=0 then retail end) retail
            ,max(case when i=0 then dts end) dts
            ,max(case when i=1 then dts end) next_dts
            ,max(case when i=-1 then dts end) prev_dts
            ,max(case when i=0 then kind end) kind
            ,max(case when i=1 then kind end) next_kind
            ,max(case when i=-1 then kind end) prev_kind
        from c0
        cross join n -- <<< small set of 3 rows, created via recursive
        -- this leads to VERY poor performance:
        --cross join ( select row_number()over()-2 i from rdb$types rows 3 )
        group by gate,r-i
        having max(case when i=0 then r end) is not null
    )
    --select * from c1
    
    ,c2 as(
        select
             t1.gate g1
             ,t2.gate g2
             ,t1.r
             ,t1.dts
             ,t1.purchase purchase_1
             ,t1.retail retail_1
             ,t2.purchase purchase_2
             ,t2.retail retail_2
             ,row_number()over(partition by t1.gate,t2.gate order by t1.r) n
             ,min(
                 min(
                 case
                    when
                    t3.gate is not null
                    and
                        (
                            t3.prev_dts is null and t1.r>1
                            or t1.prev_dts<>t3.prev_dts
                            or t1.prev_kind<>t3.prev_kind
                            or t2.prev_dts<>t3.prev_dts
                            or t2.prev_kind<>t3.prev_kind
                        )
                    then t1.r
                    when t1.r=1 and t3.r<>1
                    then -1
                end)
             )over(partition by t1.gate,t2.gate) rmin1
        from 
        c1 t1
        join c1 t2 on t1.gate<t2.gate and t1.r=t2.r and t1.dts=t2.dts and t1.kind=t2.kind
        left join c1 t3 on
            t3.gate not in(t1.gate,t2.gate)
            and t1.dts=t3.dts and t1.kind=t3.kind and  t2.dts=t3.dts and t2.kind=t3.kind  
            and t1.next_dts=t3.next_dts and t1.next_kind=t3.next_kind 
            and t2.next_dts=t3.next_dts and t2.next_kind=t3.next_kind 
        group by t1.gate,t1.r,t1.dts,t1.purchase,t1.retail,t2.gate,t2.purchase,t2.retail
    )
    
    ,c3 as
    (
        select
            g1
            ,g2
            ,r
            ,dts
            ,purchase_1
            ,purchase_2
            ,retail_1
            ,retail_2
            ,rmin1
            ,count(*)over(partition by g1,g2) cnt
            ,min(case when r<>n then n end)over(partition by g1,g2) rmin2
        from c2 a
    )
    --select * from c3
    
    ,c4 as
    (
        select
            g1
            ,g2
            ,min(dts) dts
            ,sum(retail_1) retail_1
            ,sum(retail_2) retail_2
            ,sum(purchase_1) purchase_1
            ,sum(purchase_2) purchase_2 
        from c3
        where
            (rmin2 is null and cnt>=2 or r<rmin2 and rmin2>2)
            and (r<rmin1 and rmin1>2 or rmin1 is null or rmin1=1)
        group by g1,g2
    )
    --select * from c4
    
    ,c5 as(
        select
            g1
            ,min(g2) g2
            ,max(g2) g3
            ,min(dts) dts
            ,max(purchase_1) purchase_1
            ,max(retail_1) retail_1
            ,sum(purchase_2) purchase_2
            ,sum(retail_2) retail_2
        from c4
        group by g1
    )
    --select * from c5
    
    ,t5 as(
        select
            min(g1) g1,min(g2) g2,g3,min(dts) dts,
            max(case when g2<>g3 then purchase_1 else 0 end) purchase_1,
            max(case when g2<>g3 then retail_1 else 0 end) retail_1,
            max(case when g2<>g3 then purchase_2 else purchase_2+purchase_1 end) purchase_2,
            max(case when g2<>g3 then retail_2 else retail_2+retail_1 end) retail_2
        from c5
        group by g3
    )
    ,c6 as (
        select 
            cast(substring(g1 from 1 for 12) as int) g1
            ,cast(substring(g2 from 1 for 12) as int) g2
            ,cast(case when g3<>g2 then substring(g3 from 1 for 12) end as int) g3
            ,dts
            ,retail_1+retail_2 retail_sum
            ,purchase_1+purchase_2 purchase_sum
        from t5
    )
    --select * from c6
    
    ,c7 as
    (
        select 
            case
             when g1<=g2 and g2<=g3 then g1||','||g2||','||g3
             when g1<=g3 and g3<=g2 then g1||','||g3||','||g2
             when g2<=g1 and g1<=g3 then g2||','||g1||','||g3
             when g2<=g3 and g3<=g1 then g2||','||g3||','||g1
             when g3<=g1 and g1<=g2 then g3||','||g1||','||g2
             when g3<=g2 and g2<=g1 then g3||','||g2||','||g1
             when g1<=g2 then g1||','||g2
             when g2<=g1 then g2||','||g1
            end gate,
            dts,
            retail_sum,
            purchase_sum
        from c6
    )
    select gate,dts,retail_sum,purchase_sum
    from c7
    order by 1,2;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    GATE                            1,1
    DTS                             2001-03-14
    RETAIL_SUM                      80778.08
    PURCHASE_SUM                    109400.00
    
    GATE                            100,101,102
    DTS                             2007-07-01
    RETAIL_SUM                      0.00
    PURCHASE_SUM                    10.00
    
    GATE                            121,122,124
    DTS                             2011-05-01
    RETAIL_SUM                      0.00
    PURCHASE_SUM                    9.00
    
    GATE                            141,142,144
    DTS                             2012-01-02
    RETAIL_SUM                      0.00
    PURCHASE_SUM                    27.00
    
    GATE                            161,162,163
    DTS                             2013-05-01
    RETAIL_SUM                      33022.00
    PURCHASE_SUM                    0.00
    
    GATE                            171,172
    DTS                             2013-01-01
    RETAIL_SUM                      22222.00
    PURCHASE_SUM                    0.00
    
    GATE                            181,182
    DTS                             2013-10-03
    RETAIL_SUM                      2200.00
    PURCHASE_SUM                    0.00
    
    GATE                            2,2
    DTS                             2001-03-22
    RETAIL_SUM                      24096.00
    PURCHASE_SUM                    24500.00
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
