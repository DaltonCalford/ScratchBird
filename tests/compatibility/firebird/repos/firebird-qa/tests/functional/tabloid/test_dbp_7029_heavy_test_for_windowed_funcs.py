#coding:utf-8

"""
ID:          tabloid.dbp-7029-heavy-test-for-windowed-funcs
TITLE:       Common SQL. Check correctness of the results
DESCRIPTION: 
FBTEST:      functional.tabloid.dbp_7029_heavy_test_for_windowed_funcs
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='tabloid-dbp-7029.fbk')

test_script = """
    set list on;
    with
    b as
    (
        select ai,sc,k1,(k1-1)/sc+1 x, mod((k1-1),sc) + 1 y
        from
        (
            select id ai,row_number()over(order by id )k1,
            cast(floor(sqrt((select count(*)from parea))) as int) sc
            from parea
        ) q where k1<=sc*sc
    )
    
    ,a as(
        select
            sc,k1,
            coalesce(u,0)*1000000+coalesce(v,0)*1000+coalesce(w,0)c,
            ai,x,y,x+sc*(y-1) k2
        from b
            left join
            (
                select ari,
                    sum(iif(tc='Argon', cv, 0)) u,
                    sum(iif(tc='Krypton', cv, 0)) v,
                    sum(iif(tc='Xenon', cv, 0)) w
                from pdata join ptube on tbi = id
                group by ari
            )a
            on ai=ari
    )
    
    ,t as
    (
        select t.*,
            count(*)over(partition by c, ax) cxa,
            count(*)over(partition by c, bx) cxb,
            min(ai)over(partition by c, ax) maa,
            min(ai)over(partition by c, bx) mab
        from
        (
            select
                c,ai,x,y,
                k1-row_number()over(partition by c,x order by k1) ax,
                k2-row_number()over(partition by c,y order by k2) bx
            from a
        )t
    )
    
    ,w1 as
    (
        select c,min(az) over(partition by maa) az,ai,maa,mab
        from
        (
            select c,min(maa)over(partition by mab) az,ai,maa,mab
            from T where cxa>1 or cxb>1
        )t
    )
    
    ,w2 as
    (
        select c,min(az) over(partition by maa) az,ai,maa,mab
        from (select c,min(az)over(partition by mab) az,ai,maa,mab from w1 )t
    )
    
    ,w3 as
    (
        select c,min(az) over(partition by maa) az,ai,maa,mab
        from (select c, min(az)over(partition by mab) az,ai,maa,mab from w2)t
    )
    
    ,w4 as
    (
        select c,min(az) over(partition by maa) az,ai,maa,mab
        from (select c, min(az)over(partition by mab) az,ai,maa,mab from w3)t
    )
    
    ,w5 as
    (
        select c,min(az) over(partition by maa) az,ai,maa,mab
        from (select c,min(az)over(partition by mab) az,ai,maa,mab from w4 )t
    )
    
    ,w6 as
    (
        select c,min(az)over(partition by mab) az,ai,maa,mab
        from w5
    )
    ,w7 as (
        select
            c,az,ai
           ,row_number()over(partition by az order by ai) r
           ,min(ai)over(partition by az) mq
           ,count(*)over(partition by az) mc
        from w6
    )
    
    select 
        cast(mod(c, 16*16) as varchar(12)) || ','
        || cast(mod(c,65536)/256 as varchar(12)) || ',' || cast(c/65536 as varchar(12)) f01
       ,max(mc) f02
       ,cast(az as varchar(12))
        || max(iif(r=2, ',' || cast(ai as varchar(12)), ''))
        || max(iif(r=3, ',' || cast(ai as varchar(12)), ''))
        || max(iif(r=4, ',' || cast(ai as varchar(12)), ''))
        || max(iif(r=5, ',' || cast(ai as varchar(12)), ''))
        || max(iif(r=6, ',' || cast(ai as varchar(12)), ''))
        || max(iif(r=7, ',' || cast(ai as varchar(12)), ''))
        || max(iif(r=8, ',' || cast(ai as varchar(12)), ''))
        || max(iif(r=9, ',' || cast(ai as varchar(12)), ''))
        || max(iif(r=10,',' || cast(ai as varchar(12)), '')) as f03
    from w7
    where mc < 11 and mq = az
    group by c, az
    order by 1,2,3;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
    F01                             0,0,0
    F02                             9
    F03                             9,10,58,59,60,108,109,110,1158
    F01                             0,0,0
    F02                             10
    F03                             16,65,114,1163,1212,1213,1214,1215,1216,1217
    F01                             0,0,0
    F02                             10
    F03                             2406,2416,2417,2418,2419,2420,2421,2422,2423,3472
    F01                             0,0,0
    F02                             10
    F03                             3556,3605,4654,4655,4656,4705,4743,4744,4754,4755
    F01                             0,0,0
    F02                             10
    F03                             38,86,87,88,134,135,137,138,1183,1187
    F01                             0,0,0
    F02                             10
    F03                             79,80,120,130,1170,1219,1268,2316,2317,2318
    F01                             0,0,0
    F02                             10
    F03                             8092,8093,8133,8134,8183,8184,9233,9234,9283,9284
    F01                             0,0,0
    F02                             10
    F03                             8097,8098,8099,8100,8101,8146,8148,8150,8186,8197
    F01                             0,0,0
    F02                             10
    F03                             94,104,105,106,143,1146,1192,1193,1194,1195
    F01                             132,83,184
    F02                             2
    F03                             4610,4659
    F01                             132,83,184
    F02                             2
    F03                             4757,5806
    F01                             138,150,152
    F02                             2
    F03                             1169,1218
    F01                             138,150,152
    F02                             2
    F03                             31,32
    F01                             138,150,152
    F02                             2
    F03                             55,56
    F01                             138,150,152
    F02                             4
    F03                             118,119,1166,1167
    F01                             138,150,152
    F02                             4
    F03                             13,14,61,62
    F01                             138,150,152
    F02                             4
    F03                             2364,2365,2403,2404
    F01                             138,150,152
    F02                             5
    F03                             34,35,82,83,131
    F01                             138,150,152
    F02                             6
    F03                             133,1172,1182,1220,1221,1269
    F01                             138,150,152
    F02                             7
    F03                             1235,1274,1275,2322,2323,2370,2371
    F01                             138,150,152
    F02                             7
    F03                             28,29,67,68,115,116,1164
    F01                             138,150,152
    F02                             7
    F03                             52,53,91,92,139,140,1188
    F01                             138,150,152
    F02                             8
    F03                             1241,1242,1289,1290,2328,2338,2376,2377
    F01                             138,150,152
    F02                             8
    F03                             1262,1263,2301,2302,2349,2350,2397,2398
    F01                             138,150,152
    F02                             8
    F03                             1265,1266,2313,2314,2352,2353,2400,2401
    F01                             138,150,152
    F02                             8
    F03                             142,1190,1191,1238,1239,1286,1287,2326
    F01                             138,150,152
    F02                             9
    F03                             1211,1250,1260,1298,2299,2346,2347,2394,2395
    F01                             140,150,152
    F02                             2
    F03                             2325,2374
    F01                             140,150,152
    F02                             2
    F03                             40,89
    F01                             170,195,1220
    F02                             2
    F03                             3454,3455
    F01                             170,195,1220
    F02                             2
    F03                             3457,3458
    F01                             170,195,1220
    F02                             2
    F03                             3469,3470
    F01                             170,195,1220
    F02                             2
    F03                             3496,3497
    F01                             170,195,1220
    F02                             2
    F03                             3499,3500
    F01                             26,39,0
    F02                             2
    F03                             10378,10379
    F01                             26,39,0
    F02                             2
    F03                             10381,10382
    F01                             26,39,0
    F02                             2
    F03                             10384,10385
    F01                             26,39,0
    F02                             2
    F03                             9339,9340
    F01                             26,39,0
    F02                             2
    F03                             9342,9352
    F01                             26,39,0
    F02                             2
    F03                             9354,9355
    F01                             26,39,0
    F02                             2
    F03                             9357,9358
    F01                             26,39,0
    F02                             2
    F03                             9360,9361
    F01                             26,39,0
    F02                             2
    F03                             9363,9364
    F01                             26,39,0
    F02                             8
    F03                             1,2,3,4,5,6,7,8
    F01                             4,207,153
    F02                             2
    F03                             3604,4653
    F01                             4,207,153
    F02                             2
    F03                             4703,4704
    F01                             4,207,153
    F02                             2
    F03                             5814,5815
    F01                             4,207,153
    F02                             3
    F03                             3508,3509,3557
    F01                             4,207,153
    F02                             3
    F03                             3520,3559,3560
    F01                             4,207,153
    F02                             3
    F03                             3547,3548,3587
    F01                             4,207,153
    F02                             3
    F03                             4658,4706,4707
    F01                             4,207,153
    F02                             3
    F03                             5787,5835,5836
    F01                             4,207,153
    F02                             4
    F03                             3577,3578,4625,4626
    F01                             4,207,153
    F02                             4
    F03                             4634,4635,4682,4683
    F01                             4,207,153
    F02                             4
    F03                             4691,4692,4739,4740
    F01                             4,207,153
    F02                             4
    F03                             5793,5794,5841,5842
    F01                             4,207,153
    F02                             5
    F03                             3523,3524,3562,3572,4611
    F01                             4,207,153
    F02                             5
    F03                             4730,5769,5770,5817,5818
    F01                             4,207,153
    F02                             5
    F03                             4742,5790,5791,5838,5839
    F01                             4,207,153
    F02                             7
    F03                             4664,4712,4713,4760,5761,5808,5809
    F01                             4,207,153
    F02                             9
    F03                             3526,3527,3574,3575,4613,4614,4661,4662,4710
    F01                             42,72,1
    F02                             2
    F03                             3502,3503
    F01                             48,56,61
    F02                             2
    F03                             8180,9229
    F01                             96,48,61
    F02                             2
    F03                             7048,7049
    F01                             96,48,61
    F02                             2
    F03                             7051,7052
    F01                             96,48,61
    F02                             2
    F03                             8132,8181
    F01                             96,48,61
    F02                             2
    F03                             8147,8196
    F01                             96,48,61
    F02                             3
    F03                             7045,7046,8094
    F01                             96,48,61
    F02                             3
    F03                             8096,8144,8145
    F01                             96,48,61
    F02                             3
    F03                             9261,9309,9310
    F01                             96,48,61
    F02                             3
    F03                             9282,9330,9331
    F01                             96,48,61
    F02                             4
    F03                             8108,8118,8156,8157
    F01                             96,48,61
    F02                             4
    F03                             8174,8175,8222,9223
    F01                             96,48,61
    F02                             4
    F03                             9231,9232,9279,9280
    F01                             96,48,61
    F02                             4
    F03                             9288,9289,9336,9337
    F01                             96,48,61
    F02                             5
    F03                             7030,7040,8078,8079,8127
    F01                             96,48,61
    F02                             5
    F03                             7054,7055,8102,8103,8151
    F01                             96,48,61
    F02                             5
    F03                             8204,9252,9253,9300,9301
    F01                             96,48,61
    F02                             5
    F03                             9228,9276,9277,9315,9316
    F01                             96,48,61
    F02                             8
    F03                             8198,8199,9237,9238,9285,9286,9333,9334
    F01                             96,48,61
    F02                             9
    F03                             7066,7067,8105,8106,8153,8154,8201,8202,9250
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
