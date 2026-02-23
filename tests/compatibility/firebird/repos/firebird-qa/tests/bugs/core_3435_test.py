#coding:utf-8

"""
ID:          issue-3797
ISSUE:       3797
TITLE:       Lateral derived tables
DESCRIPTION:
  Test is based on public database from sql-ex.ru and several example queries from sql-tutorial.ru:
  http://www.sql-tutorial.ru/ru/book_cross_apply/page2.html

  Example queries are published here with kind permission of Sergey Moiseenko, 07.04.2020 11:46.
  This is INITIAL test of LATERAL-JOIN functional. Additional examples will be implemented later.
JIRA:        CORE-3435
FBTEST:      bugs.core_3435
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='sql-ex-open-data.fbk')

test_script = """
    set term ^;
    create or alter function fn_get_next_laptop_model(a_code type of column laptop.code ) returns varchar(50) deterministic  as
    begin
        return ( select p.model from laptop p where p.code > :a_code order by p.code rows 1 );
    end
    ^
    create or alter function fn_get_next_laptop_speed(a_code type of column laptop.code ) returns int deterministic  as
    begin
        return ( select p.speed from laptop p where p.code > :a_code order by p.code rows 1 );
    end
    ^
    create or alter function fn_get_next_laptop_price(a_code type of column laptop.code ) returns numeric(12,2) deterministic  as
    begin
        return ( select p.price from laptop p where p.code > :a_code order by p.code rows 1 );
    end
    ^
    set term ;^
    commit;

    --------------------------------

    select 'test-1' as msg, l1.*, x.*
    from laptop l1
    cross join lateral
    (
        select max(price) max_price, min(price) min_price
        from laptop l2
        join product p1 on l2.model = p1.model
        where p1.maker =
            (
                select p2.maker
                from product p2
                where p2.model = l1.model
            )
    ) x;

    ----------------------

    select 'test-2' as msg, l1.code as curr_core, x.code as lead_code
    from laptop l1
    left join lateral
        (
            select first 1 l2.*
            from laptop l2
            where
                l1.model < l2.model
                or (l1.model = l2.model and l1.code < l2.code)
            order by l2.model, l2.code
        ) x on 1=1
    order by l1.model
    ;

    --------------------------

    select 'test-3' as msg, x.*
    from
        (
            select distinct
            p.type
            from product p
        ) pr1
    cross join lateral
        (
            select first 3 pr2.*
            from product pr2
            where pr1.type=pr2.type
            order by pr2.model
        ) x;

    ---------------------------

    -- Check ability to use in LATERAL datasource procedural objects instead of usual tables:

    select 'test-4' as msg, p.code, p.model, p.speed, p.price, x.*
    from laptop p
    left join lateral (
        select
            fn_get_next_laptop_model(p.code) as model_for_next_code
            ,fn_get_next_laptop_speed(p.code) as speed_for_next_code
            ,fn_get_next_laptop_price(p.code) as price_for_next_code
        from rdb$database
    ) x on 1=1;

    ----------------------------

    -- Check ability to use recursive datasource as LATERAL:

    with recursive
    r1 as (
        select 1 i, cast(1 as decfloat(34)) f from rdb$database
        union all
        select r.i+1, r.f * (r.i+1) from r1 as r where r.i < 10
    )
    --select * from r1

    select 'test-5' as msg, r1.i, sum( rx.xf ) factorials_running_total
    from r1
    inner join lateral (
        select rx.f as xf
        from r1 as rx
        where rx.i <= r1.i
    ) rx
    on true
    group by 1,2
    order by 3 desc;

    ------------------------------

    -- Check ability to run query with 255 contexts.

    /******************************************************
        batch for generating this query:
        ################################
        @echo off
        setlocal enabledelayedexpansion enableextensions

        set sql=%~dpn0.tmp.sql
        del !sql! 2>nul
        set n_max=254

        (
            echo set list on;
            echo select
            for /l %%i in (0,1,!n_max!) do (
                if %%i EQU 0 (
                    echo.    r%%i.i%%i
                ) else (
                    echo.    ,r%%i.i%%i
                )
            )
            echo from (
            echo       select 1 i0
            echo       from rdb$database r0
            echo ^) r0
        ) >>!sql!


        for /l %%i in (0,1,!n_max!) do (
            set p=%%i
            set /a x=%%i+1
            (
                echo cross join lateral (
                echo     select r!p!.i!p!+1 as i!x!
                echo     from rdb$database
                echo ^) r!x!
            ) >>!sql!
        )
        echo ;>>!sql!
    ***************************************************/

    set list on;
    select
        r0.i0
        ,r1.i1
        ,r2.i2
        ,r3.i3
        ,r4.i4
        ,r5.i5
        ,r6.i6
        ,r7.i7
        ,r8.i8
        ,r9.i9
        ,r10.i10
        ,r11.i11
        ,r12.i12
        ,r13.i13
        ,r14.i14
        ,r15.i15
        ,r16.i16
        ,r17.i17
        ,r18.i18
        ,r19.i19
        ,r20.i20
        ,r21.i21
        ,r22.i22
        ,r23.i23
        ,r24.i24
        ,r25.i25
        ,r26.i26
        ,r27.i27
        ,r28.i28
        ,r29.i29
        ,r30.i30
        ,r31.i31
        ,r32.i32
        ,r33.i33
        ,r34.i34
        ,r35.i35
        ,r36.i36
        ,r37.i37
        ,r38.i38
        ,r39.i39
        ,r40.i40
        ,r41.i41
        ,r42.i42
        ,r43.i43
        ,r44.i44
        ,r45.i45
        ,r46.i46
        ,r47.i47
        ,r48.i48
        ,r49.i49
        ,r50.i50
        ,r51.i51
        ,r52.i52
        ,r53.i53
        ,r54.i54
        ,r55.i55
        ,r56.i56
        ,r57.i57
        ,r58.i58
        ,r59.i59
        ,r60.i60
        ,r61.i61
        ,r62.i62
        ,r63.i63
        ,r64.i64
        ,r65.i65
        ,r66.i66
        ,r67.i67
        ,r68.i68
        ,r69.i69
        ,r70.i70
        ,r71.i71
        ,r72.i72
        ,r73.i73
        ,r74.i74
        ,r75.i75
        ,r76.i76
        ,r77.i77
        ,r78.i78
        ,r79.i79
        ,r80.i80
        ,r81.i81
        ,r82.i82
        ,r83.i83
        ,r84.i84
        ,r85.i85
        ,r86.i86
        ,r87.i87
        ,r88.i88
        ,r89.i89
        ,r90.i90
        ,r91.i91
        ,r92.i92
        ,r93.i93
        ,r94.i94
        ,r95.i95
        ,r96.i96
        ,r97.i97
        ,r98.i98
        ,r99.i99
        ,r100.i100
        ,r101.i101
        ,r102.i102
        ,r103.i103
        ,r104.i104
        ,r105.i105
        ,r106.i106
        ,r107.i107
        ,r108.i108
        ,r109.i109
        ,r110.i110
        ,r111.i111
        ,r112.i112
        ,r113.i113
        ,r114.i114
        ,r115.i115
        ,r116.i116
        ,r117.i117
        ,r118.i118
        ,r119.i119
        ,r120.i120
        ,r121.i121
        ,r122.i122
        ,r123.i123
        ,r124.i124
        ,r125.i125
        ,r126.i126
        ,r127.i127
        ,r128.i128
        ,r129.i129
        ,r130.i130
        ,r131.i131
        ,r132.i132
        ,r133.i133
        ,r134.i134
        ,r135.i135
        ,r136.i136
        ,r137.i137
        ,r138.i138
        ,r139.i139
        ,r140.i140
        ,r141.i141
        ,r142.i142
        ,r143.i143
        ,r144.i144
        ,r145.i145
        ,r146.i146
        ,r147.i147
        ,r148.i148
        ,r149.i149
        ,r150.i150
        ,r151.i151
        ,r152.i152
        ,r153.i153
        ,r154.i154
        ,r155.i155
        ,r156.i156
        ,r157.i157
        ,r158.i158
        ,r159.i159
        ,r160.i160
        ,r161.i161
        ,r162.i162
        ,r163.i163
        ,r164.i164
        ,r165.i165
        ,r166.i166
        ,r167.i167
        ,r168.i168
        ,r169.i169
        ,r170.i170
        ,r171.i171
        ,r172.i172
        ,r173.i173
        ,r174.i174
        ,r175.i175
        ,r176.i176
        ,r177.i177
        ,r178.i178
        ,r179.i179
        ,r180.i180
        ,r181.i181
        ,r182.i182
        ,r183.i183
        ,r184.i184
        ,r185.i185
        ,r186.i186
        ,r187.i187
        ,r188.i188
        ,r189.i189
        ,r190.i190
        ,r191.i191
        ,r192.i192
        ,r193.i193
        ,r194.i194
        ,r195.i195
        ,r196.i196
        ,r197.i197
        ,r198.i198
        ,r199.i199
        ,r200.i200
        ,r201.i201
        ,r202.i202
        ,r203.i203
        ,r204.i204
        ,r205.i205
        ,r206.i206
        ,r207.i207
        ,r208.i208
        ,r209.i209
        ,r210.i210
        ,r211.i211
        ,r212.i212
        ,r213.i213
        ,r214.i214
        ,r215.i215
        ,r216.i216
        ,r217.i217
        ,r218.i218
        ,r219.i219
        ,r220.i220
        ,r221.i221
        ,r222.i222
        ,r223.i223
        ,r224.i224
        ,r225.i225
        ,r226.i226
        ,r227.i227
        ,r228.i228
        ,r229.i229
        ,r230.i230
        ,r231.i231
        ,r232.i232
        ,r233.i233
        ,r234.i234
        ,r235.i235
        ,r236.i236
        ,r237.i237
        ,r238.i238
        ,r239.i239
        ,r240.i240
        ,r241.i241
        ,r242.i242
        ,r243.i243
        ,r244.i244
        ,r245.i245
        ,r246.i246
        ,r247.i247
        ,r248.i248
        ,r249.i249
        ,r250.i250
        ,r251.i251
        ,r252.i252
        ,r253.i253
        ,r254.i254
    from (
          select 1 i0
          from rdb$database r0
    ) r0
    cross join lateral (
        select r0.i0+1 as i1
        from rdb$database
    ) r1
    cross join lateral (
        select r1.i1+1 as i2
        from rdb$database
    ) r2
    cross join lateral (
        select r2.i2+1 as i3
        from rdb$database
    ) r3
    cross join lateral (
        select r3.i3+1 as i4
        from rdb$database
    ) r4
    cross join lateral (
        select r4.i4+1 as i5
        from rdb$database
    ) r5
    cross join lateral (
        select r5.i5+1 as i6
        from rdb$database
    ) r6
    cross join lateral (
        select r6.i6+1 as i7
        from rdb$database
    ) r7
    cross join lateral (
        select r7.i7+1 as i8
        from rdb$database
    ) r8
    cross join lateral (
        select r8.i8+1 as i9
        from rdb$database
    ) r9
    cross join lateral (
        select r9.i9+1 as i10
        from rdb$database
    ) r10
    cross join lateral (
        select r10.i10+1 as i11
        from rdb$database
    ) r11
    cross join lateral (
        select r11.i11+1 as i12
        from rdb$database
    ) r12
    cross join lateral (
        select r12.i12+1 as i13
        from rdb$database
    ) r13
    cross join lateral (
        select r13.i13+1 as i14
        from rdb$database
    ) r14
    cross join lateral (
        select r14.i14+1 as i15
        from rdb$database
    ) r15
    cross join lateral (
        select r15.i15+1 as i16
        from rdb$database
    ) r16
    cross join lateral (
        select r16.i16+1 as i17
        from rdb$database
    ) r17
    cross join lateral (
        select r17.i17+1 as i18
        from rdb$database
    ) r18
    cross join lateral (
        select r18.i18+1 as i19
        from rdb$database
    ) r19
    cross join lateral (
        select r19.i19+1 as i20
        from rdb$database
    ) r20
    cross join lateral (
        select r20.i20+1 as i21
        from rdb$database
    ) r21
    cross join lateral (
        select r21.i21+1 as i22
        from rdb$database
    ) r22
    cross join lateral (
        select r22.i22+1 as i23
        from rdb$database
    ) r23
    cross join lateral (
        select r23.i23+1 as i24
        from rdb$database
    ) r24
    cross join lateral (
        select r24.i24+1 as i25
        from rdb$database
    ) r25
    cross join lateral (
        select r25.i25+1 as i26
        from rdb$database
    ) r26
    cross join lateral (
        select r26.i26+1 as i27
        from rdb$database
    ) r27
    cross join lateral (
        select r27.i27+1 as i28
        from rdb$database
    ) r28
    cross join lateral (
        select r28.i28+1 as i29
        from rdb$database
    ) r29
    cross join lateral (
        select r29.i29+1 as i30
        from rdb$database
    ) r30
    cross join lateral (
        select r30.i30+1 as i31
        from rdb$database
    ) r31
    cross join lateral (
        select r31.i31+1 as i32
        from rdb$database
    ) r32
    cross join lateral (
        select r32.i32+1 as i33
        from rdb$database
    ) r33
    cross join lateral (
        select r33.i33+1 as i34
        from rdb$database
    ) r34
    cross join lateral (
        select r34.i34+1 as i35
        from rdb$database
    ) r35
    cross join lateral (
        select r35.i35+1 as i36
        from rdb$database
    ) r36
    cross join lateral (
        select r36.i36+1 as i37
        from rdb$database
    ) r37
    cross join lateral (
        select r37.i37+1 as i38
        from rdb$database
    ) r38
    cross join lateral (
        select r38.i38+1 as i39
        from rdb$database
    ) r39
    cross join lateral (
        select r39.i39+1 as i40
        from rdb$database
    ) r40
    cross join lateral (
        select r40.i40+1 as i41
        from rdb$database
    ) r41
    cross join lateral (
        select r41.i41+1 as i42
        from rdb$database
    ) r42
    cross join lateral (
        select r42.i42+1 as i43
        from rdb$database
    ) r43
    cross join lateral (
        select r43.i43+1 as i44
        from rdb$database
    ) r44
    cross join lateral (
        select r44.i44+1 as i45
        from rdb$database
    ) r45
    cross join lateral (
        select r45.i45+1 as i46
        from rdb$database
    ) r46
    cross join lateral (
        select r46.i46+1 as i47
        from rdb$database
    ) r47
    cross join lateral (
        select r47.i47+1 as i48
        from rdb$database
    ) r48
    cross join lateral (
        select r48.i48+1 as i49
        from rdb$database
    ) r49
    cross join lateral (
        select r49.i49+1 as i50
        from rdb$database
    ) r50
    cross join lateral (
        select r50.i50+1 as i51
        from rdb$database
    ) r51
    cross join lateral (
        select r51.i51+1 as i52
        from rdb$database
    ) r52
    cross join lateral (
        select r52.i52+1 as i53
        from rdb$database
    ) r53
    cross join lateral (
        select r53.i53+1 as i54
        from rdb$database
    ) r54
    cross join lateral (
        select r54.i54+1 as i55
        from rdb$database
    ) r55
    cross join lateral (
        select r55.i55+1 as i56
        from rdb$database
    ) r56
    cross join lateral (
        select r56.i56+1 as i57
        from rdb$database
    ) r57
    cross join lateral (
        select r57.i57+1 as i58
        from rdb$database
    ) r58
    cross join lateral (
        select r58.i58+1 as i59
        from rdb$database
    ) r59
    cross join lateral (
        select r59.i59+1 as i60
        from rdb$database
    ) r60
    cross join lateral (
        select r60.i60+1 as i61
        from rdb$database
    ) r61
    cross join lateral (
        select r61.i61+1 as i62
        from rdb$database
    ) r62
    cross join lateral (
        select r62.i62+1 as i63
        from rdb$database
    ) r63
    cross join lateral (
        select r63.i63+1 as i64
        from rdb$database
    ) r64
    cross join lateral (
        select r64.i64+1 as i65
        from rdb$database
    ) r65
    cross join lateral (
        select r65.i65+1 as i66
        from rdb$database
    ) r66
    cross join lateral (
        select r66.i66+1 as i67
        from rdb$database
    ) r67
    cross join lateral (
        select r67.i67+1 as i68
        from rdb$database
    ) r68
    cross join lateral (
        select r68.i68+1 as i69
        from rdb$database
    ) r69
    cross join lateral (
        select r69.i69+1 as i70
        from rdb$database
    ) r70
    cross join lateral (
        select r70.i70+1 as i71
        from rdb$database
    ) r71
    cross join lateral (
        select r71.i71+1 as i72
        from rdb$database
    ) r72
    cross join lateral (
        select r72.i72+1 as i73
        from rdb$database
    ) r73
    cross join lateral (
        select r73.i73+1 as i74
        from rdb$database
    ) r74
    cross join lateral (
        select r74.i74+1 as i75
        from rdb$database
    ) r75
    cross join lateral (
        select r75.i75+1 as i76
        from rdb$database
    ) r76
    cross join lateral (
        select r76.i76+1 as i77
        from rdb$database
    ) r77
    cross join lateral (
        select r77.i77+1 as i78
        from rdb$database
    ) r78
    cross join lateral (
        select r78.i78+1 as i79
        from rdb$database
    ) r79
    cross join lateral (
        select r79.i79+1 as i80
        from rdb$database
    ) r80
    cross join lateral (
        select r80.i80+1 as i81
        from rdb$database
    ) r81
    cross join lateral (
        select r81.i81+1 as i82
        from rdb$database
    ) r82
    cross join lateral (
        select r82.i82+1 as i83
        from rdb$database
    ) r83
    cross join lateral (
        select r83.i83+1 as i84
        from rdb$database
    ) r84
    cross join lateral (
        select r84.i84+1 as i85
        from rdb$database
    ) r85
    cross join lateral (
        select r85.i85+1 as i86
        from rdb$database
    ) r86
    cross join lateral (
        select r86.i86+1 as i87
        from rdb$database
    ) r87
    cross join lateral (
        select r87.i87+1 as i88
        from rdb$database
    ) r88
    cross join lateral (
        select r88.i88+1 as i89
        from rdb$database
    ) r89
    cross join lateral (
        select r89.i89+1 as i90
        from rdb$database
    ) r90
    cross join lateral (
        select r90.i90+1 as i91
        from rdb$database
    ) r91
    cross join lateral (
        select r91.i91+1 as i92
        from rdb$database
    ) r92
    cross join lateral (
        select r92.i92+1 as i93
        from rdb$database
    ) r93
    cross join lateral (
        select r93.i93+1 as i94
        from rdb$database
    ) r94
    cross join lateral (
        select r94.i94+1 as i95
        from rdb$database
    ) r95
    cross join lateral (
        select r95.i95+1 as i96
        from rdb$database
    ) r96
    cross join lateral (
        select r96.i96+1 as i97
        from rdb$database
    ) r97
    cross join lateral (
        select r97.i97+1 as i98
        from rdb$database
    ) r98
    cross join lateral (
        select r98.i98+1 as i99
        from rdb$database
    ) r99
    cross join lateral (
        select r99.i99+1 as i100
        from rdb$database
    ) r100
    cross join lateral (
        select r100.i100+1 as i101
        from rdb$database
    ) r101
    cross join lateral (
        select r101.i101+1 as i102
        from rdb$database
    ) r102
    cross join lateral (
        select r102.i102+1 as i103
        from rdb$database
    ) r103
    cross join lateral (
        select r103.i103+1 as i104
        from rdb$database
    ) r104
    cross join lateral (
        select r104.i104+1 as i105
        from rdb$database
    ) r105
    cross join lateral (
        select r105.i105+1 as i106
        from rdb$database
    ) r106
    cross join lateral (
        select r106.i106+1 as i107
        from rdb$database
    ) r107
    cross join lateral (
        select r107.i107+1 as i108
        from rdb$database
    ) r108
    cross join lateral (
        select r108.i108+1 as i109
        from rdb$database
    ) r109
    cross join lateral (
        select r109.i109+1 as i110
        from rdb$database
    ) r110
    cross join lateral (
        select r110.i110+1 as i111
        from rdb$database
    ) r111
    cross join lateral (
        select r111.i111+1 as i112
        from rdb$database
    ) r112
    cross join lateral (
        select r112.i112+1 as i113
        from rdb$database
    ) r113
    cross join lateral (
        select r113.i113+1 as i114
        from rdb$database
    ) r114
    cross join lateral (
        select r114.i114+1 as i115
        from rdb$database
    ) r115
    cross join lateral (
        select r115.i115+1 as i116
        from rdb$database
    ) r116
    cross join lateral (
        select r116.i116+1 as i117
        from rdb$database
    ) r117
    cross join lateral (
        select r117.i117+1 as i118
        from rdb$database
    ) r118
    cross join lateral (
        select r118.i118+1 as i119
        from rdb$database
    ) r119
    cross join lateral (
        select r119.i119+1 as i120
        from rdb$database
    ) r120
    cross join lateral (
        select r120.i120+1 as i121
        from rdb$database
    ) r121
    cross join lateral (
        select r121.i121+1 as i122
        from rdb$database
    ) r122
    cross join lateral (
        select r122.i122+1 as i123
        from rdb$database
    ) r123
    cross join lateral (
        select r123.i123+1 as i124
        from rdb$database
    ) r124
    cross join lateral (
        select r124.i124+1 as i125
        from rdb$database
    ) r125
    cross join lateral (
        select r125.i125+1 as i126
        from rdb$database
    ) r126
    cross join lateral (
        select r126.i126+1 as i127
        from rdb$database
    ) r127
    cross join lateral (
        select r127.i127+1 as i128
        from rdb$database
    ) r128
    cross join lateral (
        select r128.i128+1 as i129
        from rdb$database
    ) r129
    cross join lateral (
        select r129.i129+1 as i130
        from rdb$database
    ) r130
    cross join lateral (
        select r130.i130+1 as i131
        from rdb$database
    ) r131
    cross join lateral (
        select r131.i131+1 as i132
        from rdb$database
    ) r132
    cross join lateral (
        select r132.i132+1 as i133
        from rdb$database
    ) r133
    cross join lateral (
        select r133.i133+1 as i134
        from rdb$database
    ) r134
    cross join lateral (
        select r134.i134+1 as i135
        from rdb$database
    ) r135
    cross join lateral (
        select r135.i135+1 as i136
        from rdb$database
    ) r136
    cross join lateral (
        select r136.i136+1 as i137
        from rdb$database
    ) r137
    cross join lateral (
        select r137.i137+1 as i138
        from rdb$database
    ) r138
    cross join lateral (
        select r138.i138+1 as i139
        from rdb$database
    ) r139
    cross join lateral (
        select r139.i139+1 as i140
        from rdb$database
    ) r140
    cross join lateral (
        select r140.i140+1 as i141
        from rdb$database
    ) r141
    cross join lateral (
        select r141.i141+1 as i142
        from rdb$database
    ) r142
    cross join lateral (
        select r142.i142+1 as i143
        from rdb$database
    ) r143
    cross join lateral (
        select r143.i143+1 as i144
        from rdb$database
    ) r144
    cross join lateral (
        select r144.i144+1 as i145
        from rdb$database
    ) r145
    cross join lateral (
        select r145.i145+1 as i146
        from rdb$database
    ) r146
    cross join lateral (
        select r146.i146+1 as i147
        from rdb$database
    ) r147
    cross join lateral (
        select r147.i147+1 as i148
        from rdb$database
    ) r148
    cross join lateral (
        select r148.i148+1 as i149
        from rdb$database
    ) r149
    cross join lateral (
        select r149.i149+1 as i150
        from rdb$database
    ) r150
    cross join lateral (
        select r150.i150+1 as i151
        from rdb$database
    ) r151
    cross join lateral (
        select r151.i151+1 as i152
        from rdb$database
    ) r152
    cross join lateral (
        select r152.i152+1 as i153
        from rdb$database
    ) r153
    cross join lateral (
        select r153.i153+1 as i154
        from rdb$database
    ) r154
    cross join lateral (
        select r154.i154+1 as i155
        from rdb$database
    ) r155
    cross join lateral (
        select r155.i155+1 as i156
        from rdb$database
    ) r156
    cross join lateral (
        select r156.i156+1 as i157
        from rdb$database
    ) r157
    cross join lateral (
        select r157.i157+1 as i158
        from rdb$database
    ) r158
    cross join lateral (
        select r158.i158+1 as i159
        from rdb$database
    ) r159
    cross join lateral (
        select r159.i159+1 as i160
        from rdb$database
    ) r160
    cross join lateral (
        select r160.i160+1 as i161
        from rdb$database
    ) r161
    cross join lateral (
        select r161.i161+1 as i162
        from rdb$database
    ) r162
    cross join lateral (
        select r162.i162+1 as i163
        from rdb$database
    ) r163
    cross join lateral (
        select r163.i163+1 as i164
        from rdb$database
    ) r164
    cross join lateral (
        select r164.i164+1 as i165
        from rdb$database
    ) r165
    cross join lateral (
        select r165.i165+1 as i166
        from rdb$database
    ) r166
    cross join lateral (
        select r166.i166+1 as i167
        from rdb$database
    ) r167
    cross join lateral (
        select r167.i167+1 as i168
        from rdb$database
    ) r168
    cross join lateral (
        select r168.i168+1 as i169
        from rdb$database
    ) r169
    cross join lateral (
        select r169.i169+1 as i170
        from rdb$database
    ) r170
    cross join lateral (
        select r170.i170+1 as i171
        from rdb$database
    ) r171
    cross join lateral (
        select r171.i171+1 as i172
        from rdb$database
    ) r172
    cross join lateral (
        select r172.i172+1 as i173
        from rdb$database
    ) r173
    cross join lateral (
        select r173.i173+1 as i174
        from rdb$database
    ) r174
    cross join lateral (
        select r174.i174+1 as i175
        from rdb$database
    ) r175
    cross join lateral (
        select r175.i175+1 as i176
        from rdb$database
    ) r176
    cross join lateral (
        select r176.i176+1 as i177
        from rdb$database
    ) r177
    cross join lateral (
        select r177.i177+1 as i178
        from rdb$database
    ) r178
    cross join lateral (
        select r178.i178+1 as i179
        from rdb$database
    ) r179
    cross join lateral (
        select r179.i179+1 as i180
        from rdb$database
    ) r180
    cross join lateral (
        select r180.i180+1 as i181
        from rdb$database
    ) r181
    cross join lateral (
        select r181.i181+1 as i182
        from rdb$database
    ) r182
    cross join lateral (
        select r182.i182+1 as i183
        from rdb$database
    ) r183
    cross join lateral (
        select r183.i183+1 as i184
        from rdb$database
    ) r184
    cross join lateral (
        select r184.i184+1 as i185
        from rdb$database
    ) r185
    cross join lateral (
        select r185.i185+1 as i186
        from rdb$database
    ) r186
    cross join lateral (
        select r186.i186+1 as i187
        from rdb$database
    ) r187
    cross join lateral (
        select r187.i187+1 as i188
        from rdb$database
    ) r188
    cross join lateral (
        select r188.i188+1 as i189
        from rdb$database
    ) r189
    cross join lateral (
        select r189.i189+1 as i190
        from rdb$database
    ) r190
    cross join lateral (
        select r190.i190+1 as i191
        from rdb$database
    ) r191
    cross join lateral (
        select r191.i191+1 as i192
        from rdb$database
    ) r192
    cross join lateral (
        select r192.i192+1 as i193
        from rdb$database
    ) r193
    cross join lateral (
        select r193.i193+1 as i194
        from rdb$database
    ) r194
    cross join lateral (
        select r194.i194+1 as i195
        from rdb$database
    ) r195
    cross join lateral (
        select r195.i195+1 as i196
        from rdb$database
    ) r196
    cross join lateral (
        select r196.i196+1 as i197
        from rdb$database
    ) r197
    cross join lateral (
        select r197.i197+1 as i198
        from rdb$database
    ) r198
    cross join lateral (
        select r198.i198+1 as i199
        from rdb$database
    ) r199
    cross join lateral (
        select r199.i199+1 as i200
        from rdb$database
    ) r200
    cross join lateral (
        select r200.i200+1 as i201
        from rdb$database
    ) r201
    cross join lateral (
        select r201.i201+1 as i202
        from rdb$database
    ) r202
    cross join lateral (
        select r202.i202+1 as i203
        from rdb$database
    ) r203
    cross join lateral (
        select r203.i203+1 as i204
        from rdb$database
    ) r204
    cross join lateral (
        select r204.i204+1 as i205
        from rdb$database
    ) r205
    cross join lateral (
        select r205.i205+1 as i206
        from rdb$database
    ) r206
    cross join lateral (
        select r206.i206+1 as i207
        from rdb$database
    ) r207
    cross join lateral (
        select r207.i207+1 as i208
        from rdb$database
    ) r208
    cross join lateral (
        select r208.i208+1 as i209
        from rdb$database
    ) r209
    cross join lateral (
        select r209.i209+1 as i210
        from rdb$database
    ) r210
    cross join lateral (
        select r210.i210+1 as i211
        from rdb$database
    ) r211
    cross join lateral (
        select r211.i211+1 as i212
        from rdb$database
    ) r212
    cross join lateral (
        select r212.i212+1 as i213
        from rdb$database
    ) r213
    cross join lateral (
        select r213.i213+1 as i214
        from rdb$database
    ) r214
    cross join lateral (
        select r214.i214+1 as i215
        from rdb$database
    ) r215
    cross join lateral (
        select r215.i215+1 as i216
        from rdb$database
    ) r216
    cross join lateral (
        select r216.i216+1 as i217
        from rdb$database
    ) r217
    cross join lateral (
        select r217.i217+1 as i218
        from rdb$database
    ) r218
    cross join lateral (
        select r218.i218+1 as i219
        from rdb$database
    ) r219
    cross join lateral (
        select r219.i219+1 as i220
        from rdb$database
    ) r220
    cross join lateral (
        select r220.i220+1 as i221
        from rdb$database
    ) r221
    cross join lateral (
        select r221.i221+1 as i222
        from rdb$database
    ) r222
    cross join lateral (
        select r222.i222+1 as i223
        from rdb$database
    ) r223
    cross join lateral (
        select r223.i223+1 as i224
        from rdb$database
    ) r224
    cross join lateral (
        select r224.i224+1 as i225
        from rdb$database
    ) r225
    cross join lateral (
        select r225.i225+1 as i226
        from rdb$database
    ) r226
    cross join lateral (
        select r226.i226+1 as i227
        from rdb$database
    ) r227
    cross join lateral (
        select r227.i227+1 as i228
        from rdb$database
    ) r228
    cross join lateral (
        select r228.i228+1 as i229
        from rdb$database
    ) r229
    cross join lateral (
        select r229.i229+1 as i230
        from rdb$database
    ) r230
    cross join lateral (
        select r230.i230+1 as i231
        from rdb$database
    ) r231
    cross join lateral (
        select r231.i231+1 as i232
        from rdb$database
    ) r232
    cross join lateral (
        select r232.i232+1 as i233
        from rdb$database
    ) r233
    cross join lateral (
        select r233.i233+1 as i234
        from rdb$database
    ) r234
    cross join lateral (
        select r234.i234+1 as i235
        from rdb$database
    ) r235
    cross join lateral (
        select r235.i235+1 as i236
        from rdb$database
    ) r236
    cross join lateral (
        select r236.i236+1 as i237
        from rdb$database
    ) r237
    cross join lateral (
        select r237.i237+1 as i238
        from rdb$database
    ) r238
    cross join lateral (
        select r238.i238+1 as i239
        from rdb$database
    ) r239
    cross join lateral (
        select r239.i239+1 as i240
        from rdb$database
    ) r240
    cross join lateral (
        select r240.i240+1 as i241
        from rdb$database
    ) r241
    cross join lateral (
        select r241.i241+1 as i242
        from rdb$database
    ) r242
    cross join lateral (
        select r242.i242+1 as i243
        from rdb$database
    ) r243
    cross join lateral (
        select r243.i243+1 as i244
        from rdb$database
    ) r244
    cross join lateral (
        select r244.i244+1 as i245
        from rdb$database
    ) r245
    cross join lateral (
        select r245.i245+1 as i246
        from rdb$database
    ) r246
    cross join lateral (
        select r246.i246+1 as i247
        from rdb$database
    ) r247
    cross join lateral (
        select r247.i247+1 as i248
        from rdb$database
    ) r248
    cross join lateral (
        select r248.i248+1 as i249
        from rdb$database
    ) r249
    cross join lateral (
        select r249.i249+1 as i250
        from rdb$database
    ) r250
    cross join lateral (
        select r250.i250+1 as i251
        from rdb$database
    ) r251
    cross join lateral (
        select r251.i251+1 as i252
        from rdb$database
    ) r252
    cross join lateral (
        select r252.i252+1 as i253
        from rdb$database
    ) r253
    cross join lateral (
        select r253.i253+1 as i254
        from rdb$database
    ) r254
    cross join lateral (
        select r254.i254+1 as i255
        from rdb$database
    ) r255
    ;




"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('===.*', '')])

expected_stdout = """
    MSG            CODE MODEL        SPEED          RAM                      HD                 PRICE       SCREEN             MAX_PRICE             MIN_PRICE
    test-1            1 1298           350           32       4.000000000000000                700.00           11               1150.00                700.00
    test-1            2 1321           500           64       8.000000000000000                970.00           12                970.00                970.00
    test-1            3 1750           750          128       12.00000000000000               1200.00           14               1200.00               1200.00
    test-1            4 1298           600           64       10.00000000000000               1050.00           15               1150.00                700.00
    test-1            5 1752           750          128       10.00000000000000               1150.00           14               1150.00                700.00
    test-1            6 1298           450           64       10.00000000000000                950.00           12               1150.00                700.00

    MSG       CURR_CORE    LEAD_CODE
    test-2            1            4
    test-2            4            6
    test-2            6            2
    test-2            2            3
    test-2            3            5
    test-2            5       <null>

    MSG    MAKER      MODEL TYPE
    test-3 A          1298  Laptop
    test-3 C          1321  Laptop
    test-3 B          1750  Laptop
    test-3 B          1121  PC
    test-3 A          1232  PC
    test-3 A          1233  PC
    test-3 A          1276  Printer
    test-3 D          1288  Printer
    test-3 A          1401  Printer

    MSG            CODE MODEL        SPEED                 PRICE MODEL_FOR_NEXT_CODE SPEED_FOR_NEXT_CODE   PRICE_FOR_NEXT_CODE
    test-4            1 1298           350                700.00 1321                                500                970.00
    test-4            2 1321           500                970.00 1750                                750               1200.00
    test-4            3 1750           750               1200.00 1298                                600               1050.00
    test-4            4 1298           600               1050.00 1752                                750               1150.00
    test-4            5 1752           750               1150.00 1298                                450                950.00
    test-4            6 1298           450                950.00 <null>                           <null>                <null>


    MSG               I                   FACTORIALS_RUNNING_TOTAL
    test-5           10                                    4037913
    test-5            9                                     409113
    test-5            8                                      46233
    test-5            7                                       5913
    test-5            6                                        873
    test-5            5                                        153
    test-5            4                                         33
    test-5            3                                          9
    test-5            2                                          3
    test-5            1                                          1

    I0 1
    I1 2
    I2 3
    I3 4
    I4 5
    I5 6
    I6 7
    I7 8
    I8 9
    I9 10
    I10 11
    I11 12
    I12 13
    I13 14
    I14 15
    I15 16
    I16 17
    I17 18
    I18 19
    I19 20
    I20 21
    I21 22
    I22 23
    I23 24
    I24 25
    I25 26
    I26 27
    I27 28
    I28 29
    I29 30
    I30 31
    I31 32
    I32 33
    I33 34
    I34 35
    I35 36
    I36 37
    I37 38
    I38 39
    I39 40
    I40 41
    I41 42
    I42 43
    I43 44
    I44 45
    I45 46
    I46 47
    I47 48
    I48 49
    I49 50
    I50 51
    I51 52
    I52 53
    I53 54
    I54 55
    I55 56
    I56 57
    I57 58
    I58 59
    I59 60
    I60 61
    I61 62
    I62 63
    I63 64
    I64 65
    I65 66
    I66 67
    I67 68
    I68 69
    I69 70
    I70 71
    I71 72
    I72 73
    I73 74
    I74 75
    I75 76
    I76 77
    I77 78
    I78 79
    I79 80
    I80 81
    I81 82
    I82 83
    I83 84
    I84 85
    I85 86
    I86 87
    I87 88
    I88 89
    I89 90
    I90 91
    I91 92
    I92 93
    I93 94
    I94 95
    I95 96
    I96 97
    I97 98
    I98 99
    I99 100
    I100 101
    I101 102
    I102 103
    I103 104
    I104 105
    I105 106
    I106 107
    I107 108
    I108 109
    I109 110
    I110 111
    I111 112
    I112 113
    I113 114
    I114 115
    I115 116
    I116 117
    I117 118
    I118 119
    I119 120
    I120 121
    I121 122
    I122 123
    I123 124
    I124 125
    I125 126
    I126 127
    I127 128
    I128 129
    I129 130
    I130 131
    I131 132
    I132 133
    I133 134
    I134 135
    I135 136
    I136 137
    I137 138
    I138 139
    I139 140
    I140 141
    I141 142
    I142 143
    I143 144
    I144 145
    I145 146
    I146 147
    I147 148
    I148 149
    I149 150
    I150 151
    I151 152
    I152 153
    I153 154
    I154 155
    I155 156
    I156 157
    I157 158
    I158 159
    I159 160
    I160 161
    I161 162
    I162 163
    I163 164
    I164 165
    I165 166
    I166 167
    I167 168
    I168 169
    I169 170
    I170 171
    I171 172
    I172 173
    I173 174
    I174 175
    I175 176
    I176 177
    I177 178
    I178 179
    I179 180
    I180 181
    I181 182
    I182 183
    I183 184
    I184 185
    I185 186
    I186 187
    I187 188
    I188 189
    I189 190
    I190 191
    I191 192
    I192 193
    I193 194
    I194 195
    I195 196
    I196 197
    I197 198
    I198 199
    I199 200
    I200 201
    I201 202
    I202 203
    I203 204
    I204 205
    I205 206
    I206 207
    I207 208
    I208 209
    I209 210
    I210 211
    I211 212
    I212 213
    I213 214
    I214 215
    I215 216
    I216 217
    I217 218
    I218 219
    I219 220
    I220 221
    I221 222
    I222 223
    I223 224
    I224 225
    I225 226
    I226 227
    I227 228
    I228 229
    I229 230
    I230 231
    I231 232
    I232 233
    I233 234
    I234 235
    I235 236
    I236 237
    I237 238
    I238 239
    I239 240
    I240 241
    I241 242
    I242 243
    I243 244
    I244 245
    I245 246
    I246 247
    I247 248
    I248 249
    I249 250
    I250 251
    I251 252
    I252 253
    I253 254
    I254 255
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

