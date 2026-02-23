#coding:utf-8

"""
ID:          issue-542
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/542
TITLE:       Count (DISTINCT ...) is too slow
DESCRIPTION:
JIRA:        CORE-214
FBTEST:      bugs.core_0214
NOTES:
    [29.11.2024] pzotov
    1. Fix was 31-may-2015, https://sourceforge.net/p/firebird/code/61671/ (3.0.0.31846)
       Query 'select count(distinct id) from <table_with_200000_rows>' (hereafter "q1") elapsed time:
         * before fix: ~840 ms
         * after fix: ~248 ms
          (and this value is close to the time for query 'select count(*) from (select distinct id from ...)', hereafter "q2")
    2. Test was fully re-implemented: we have to measure difference between cpu.user_time values instead of datediff().
       Each query for each datatype is done <N_MEASURES> times, with storing cpu.user_time difference as array element.
       Then we evaluate median value for this array and save it for further comparison with <MAX_RATIO>.
    3. It was encountered that for several datatypes ratio between CPU time for q1 and q2 remains unusually high:
         * for DECFLOAT it can be about 3...4;
         * for VARCHAR it depends on the column length and number of bytes in the charset:
             ** for single-byte charset (e.g. win1251 etc) ratio is ~2 for field size ~100 and more than 10 for size 1k ... 4k;
             ** for mylti-byte (utf8) ratio is 5...7 for field size ~100 and 35...60 for size 1k ...4k (depends on FB version)
       For all other datatypes ratio is about 0.9 ... 1.2.
       Time ratio for LIST(distinct ...) was also measured. Results are the same as for COUNT.
       Test is considered as passed if ratios for all datatypes are less than <MAX_RATIO>.
    4. Because of necessity to measure ratio for datatypes that absent in FB 3.x, it was decided to increase min_version to 4.0
       (plus, there won't be any niticeable changes in FB 3.x code).
    5. A new ticket has been created to describe problem with DECFLOAT and VARCHAR datatypes:
       https://github.com/FirebirdSQL/firebird/issues/8330
       (it contains two excel files with comparison for misc datatypes and different declared length of varchar column).
       Test for these datatypes will be added after fix of this ticked.

    Checked on Windiws (SS/CS): 6.0.0.535; 5.0.2.1569; 4.0.6.3169.

    [03.12.2024] pzotov
    Made MAX_RATIO different for Windows vs Linux. Increased its value on Linux: in some cases it can be more than 2.33

    [18.01.2025] pzotov
    Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
    in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
    Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
    This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
    The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
"""
import os
import psutil
import time
import pytest
from firebird.qa import *

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

###########################
###   S E T T I N G S   ###
###########################
N_MEASURES = 11
PAGE_SIZE = 8192
N_ROWS_CNT= 100000
MAX_RATIO = 2.0 if os.name == 'nt' else 3.0

#----------------------------------------------------
# NOT yet used. See #8330:
TXT_WIDTH_SINGLE_BYTE_ENCODING = int(PAGE_SIZE/2 + 1)
TXT_WIDTH_MULTI_BYTE_ENCODING = int(PAGE_SIZE/2 + 1)
SINGLE_BYTE_TEXT = 'u' * 4097
MULTI_BYTE_TEXT =  'λ' * 4097
#----------------------------------------------------

init_sql = f"""
    set bail on;
    recreate table test_one_unique_value(
        f_sml smallint                                                              --   0
       ,f_int int                                                                   --   1
       ,f_big bigint                                                                --   2
       ,f_i128 int128                                                               --   3
       ,f_bool boolean                                                              --   4
       ,f_dt date                                                                   --   5
       ,f_tm time                                                                   --   6
       ,f_ts timestamp                                                              --   7
       ,f_tmtz time with time zone                                                  --   8
       ,f_tstz timestamp with time zone                                             --   9
       ,f_num numeric(2,2)                                                          --  10
       ,f_dec decimal(2,2)                                                          --  11
       ,f_dbl double precision                                                      --  12

       -- commented out until #8330 remains unfixed:
       ---------------------------------------------
       --,f_decf decfloat                                                             --  13
       --,f_txt_1251 varchar({TXT_WIDTH_SINGLE_BYTE_ENCODING}) character set win1251  --  14
       --,f_txt_utf8 varchar({TXT_WIDTH_MULTI_BYTE_ENCODING}) character set utf8      --  15
       ---------------------------------------------
    );

    recreate table test_null_in_all_rows(
        nul_sml smallint
       ,nul_int int
       ,nul_big bigint
       ,nul_i128 int128
       ,nul_bool boolean
       ,nul_dt date
       ,nul_tm time
       ,nul_ts timestamp
       ,nul_tmtz time with time zone
       ,nul_tstz timestamp with time zone
       ,nul_num numeric(2,2)
       ,nul_dec decimal(2,2)
       ,nul_dbl double precision
       --,nul_decf decfloat
       --,nul_txt_1251 varchar({TXT_WIDTH_SINGLE_BYTE_ENCODING}) character set win1251
       --,nul_txt_utf8 varchar({TXT_WIDTH_MULTI_BYTE_ENCODING}) character set utf8
    );
    commit;

    set term ^;
    create or alter procedure sp_fill(a_cnt int) returns(id int) as
    begin
        id = 0;
        while (id < a_cnt) do
        begin
            suspend;
            id = id + 1;
        end
    end ^
    set term ;^
    commit;

    insert into test_one_unique_value (
        f_sml        --  0
       ,f_int        --  1
       ,f_big        --  2
       ,f_i128       --  3
       ,f_bool       --  4
       ,f_dt         --  5
       ,f_tm         --  6
       ,f_ts         --  7
       ,f_tmtz       --  8
       ,f_tstz       --  9
       ,f_num        -- 10
       ,f_dec        -- 11
       ,f_dbl        -- 12
       --,f_decf       -- 13
       --,f_txt_1251   -- 14
       --,f_txt_utf8   -- 15
    )
    select
         -32768                                                          --  0
        ,-2147483648                                                     --  1
        ,-9223372036854775808                                            --  2
        ,-170141183460469231731687303715884105728                        --  3
        ,true                                                            --  4
        ,date '19.12.2023'                                               --  5
        ,time '23:59:59'                                                 --  6
        ,timestamp '19.12.2023 23:59:59'                                 --  7
        ,time '11:11:11.111 Indian/Cocos'                                --  8
        ,timestamp '2018-12-31 12:31:42.543 Pacific/Fiji'                --  9
        ,-327.68                                                         -- 10
        ,-327.68                                                         -- 11
        ,pi()                                                            -- 12
        --,exp(1) -- cast(-9.999999999999999999999999999999999E6144 as decfloat(34)) -- 13
        --,lpad('', {TXT_WIDTH_SINGLE_BYTE_ENCODING}, '{SINGLE_BYTE_TEXT}') -- 14
        --,lpad('', {TXT_WIDTH_MULTI_BYTE_ENCODING},  '{MULTI_BYTE_TEXT}') -- 15
    from sp_fill({N_ROWS_CNT}) as p;

    insert into test_null_in_all_rows
    select
         null     --  0
        ,null     --  1
        ,null     --  2
        ,null     --  3
        ,null     --  4
        ,null     --  5
        ,null     --  6
        ,null     --  7
        ,null     --  8
        ,null     --  9
        ,null     -- 10
        ,null     -- 11
        ,null     -- 12
        --,null     -- 13
        --,null     -- 14
        --,null     -- 15
    from test_one_unique_value;
    commit;
"""
db = db_factory(init = init_sql, page_size = PAGE_SIZE)

act = python_act('db', substitutions = [('[ \t]+', ' ')])

@pytest.mark.slow
@pytest.mark.perf_measure
@pytest.mark.version('>=4')
def test_1(act: Action, capsys):

    with act.db.connect() as con:
        cur1=con.cursor()
        cur2=con.cursor()

        cur1.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur1.fetchone()[0])

        run_cpu_map = {}
        for t_name in ('test_one_unique_value', 'test_null_in_all_rows'):

            fields_qry = f"""
                select rf.rdb$field_name
                from rdb$relation_fields rf
                where rf.rdb$relation_name = upper('{t_name}')
                order by rf.rdb$field_position
            """
            cur1.execute(fields_qry)
            fields_lst = [x[0].strip() for x in cur1.fetchall()]

            for f_name in fields_lst:
                query1 = f'select /* trace_me */ count(*) from (select distinct {f_name} from {t_name})'
                query2 = f'select /* trace_me */ count(distinct {f_name}) from {t_name}'
                ps1 = cur1.prepare(query1)
                ps2 = cur2.prepare(query2)
                for c in (cur1, cur2):
                    cpu_usage_values = []
                    psc = ps1 if c == cur1 else ps2
                    for i in range(0, N_MEASURES):
                        fb_info_init = psutil.Process(fb_pid).cpu_times()

                        # ::: NB ::: 'psc' returns data, i.e. this is SELECTABLE expression.
                        # We have to store result of cur.execute(<psInstance>) in order to
                        # close it explicitly.
                        # Otherwise AV can occur during Python garbage collection and this
                        # causes pytest to hang on its final point.
                        # Explained by hvlad, email 26.10.24 17:42
                        rs = c.execute(psc)
                        c.fetchall()
                        rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS

                        fb_info_curr = psutil.Process(fb_pid).cpu_times()
                        cpu_usage_values.append( max(fb_info_curr.user - fb_info_init.user, 0.000001) )

                    v = run_cpu_map.get( (t_name,f_name),  [0,0,0, '',''])
                    if psc == ps1:
                        v[0] = median(cpu_usage_values) # 'select count(*) from (select distinct ...)'
                    else:
                        v[1] = median(cpu_usage_values) # 'select count(distinct ...) from ...'
                        v[2] = v[0] / v[1]
                        v[3] = query1
                        v[4] = query2
                    run_cpu_map[ (t_name,f_name) ] = v

                ps1.free()
                ps2.free()

    poor_ratios_lst = []

    #for k,v in run_cpu_map.items():
    #    print(':::',k,':::')
    #    cpu_median_1, cpu_median_2, cpu_medians_ratio, query_1, query_2 = v
    #    # f'{ra=:12.4f}'
    #    msg = '\n'.join(
    #                      (  f'{query_1=}'
    #                        ,f'{query_2=}'
    #                        ,f'{cpu_median_1=:12.4f} {cpu_median_2=:12.4f} {cpu_medians_ratio=:12.6f}'
    #                      )
    #                   )
    #    print(msg)
    #    print('-------------------------------------------------------------------------------------')


    msg_prefix = 'CPU time medians ratio: '
    msg_expected = msg_prefix + 'EXPECTED.'
    for k,v in run_cpu_map.items():
        if v[2] > MAX_RATIO:
            poor_ratios_lst.append( '\n'.join(
                                                 (  'query_1: ' + v[3]
                                                   ,'query_2: ' + v[4]
                                                   ,f'cpu_median_1: {v[0]:12.6f}'
                                                   ,f'cpu_median_2: {v[1]:12.6f}'
                                                   ,f'cpu_median_1 / cpu_median_2: {v[0]/v[1]:12.6f}'
                                                 )
                                             )
                                  )

    if poor_ratios_lst:
        print(f'{msg_prefix} /* perf_issue_tag */ UNEXPECTED. Following ratio(s) exceeds MAX_RATIO={MAX_RATIO}:')
        for x in poor_ratios_lst:
            print(x)
    else:
        print(msg_expected)


    act.expected_stdout = msg_expected
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
