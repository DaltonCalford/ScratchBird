#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8795
TITLE:       GENERATE_SERIES function - basic test
DESCRIPTION:
    Only basic features are checked here.
    More complex test(s) will be implemented later.
NOTES:
    [21.11.2025] pzotov
    Checked on 6.0.0.1357-d37c931.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t_limits(
        id int
       ,i016 smallint
       ,i032 int
       ,i064 bigint
       ,i128 int128
       ,n022 numeric(2,2)
       ,d184 decimal(18,4)
    );
    insert into t_limits
    values (
        1
       ,-32768
       ,-2147483648
       ,-9223372036854775808
       ,-170141183460469231731687303715884105728
       ,-327.68
       ,-922337203685477.5808
    );

    insert into t_limits
    values (
        2
       ,-32768 + 1
       ,-2147483648 + 1
       ,-9223372036854775808 + 1
       ,-170141183460469231731687303715884105728 + 1
       ,-327.68 + 1
       ,-922337203685477.5808 + 1
    );

    insert into t_limits
    select
        id+1
       ,abs(i016)
       ,abs(i032)
       ,abs(i064)
       ,abs(i128)
       ,327.67
       ,922337203685477.5807
    from t_limits
    where id = 2;

    insert into t_limits
    select
        id+1
       ,abs(i016)-1
       ,abs(i032)-1
       ,abs(i064)-1
       ,abs(i128)-1
       ,327.67-1
       ,922337203685477.5807-1
    from t_limits
    where id = 3;

    with
    a as (
        select
            min(iif(i016<0, i016, null)) as i016_neg_start
           ,max(iif(i016<0, i016, null)) as i016_neg_finish
           ,min(iif(i032<0, i032, null)) as i032_neg_start
           ,max(iif(i032<0, i032, null)) as i032_neg_finish
           ,min(iif(i064<0, i064, null)) as i064_neg_start
           ,max(iif(i064<0, i064, null)) as i064_neg_finish
           ,min(iif(i128<0, i128, null)) as i128_neg_start
           ,max(iif(i128<0, i128, null)) as i128_neg_finish
           ,min(iif(n022<0, n022, null)) as n022_neg_start
           ,max(iif(n022<0, n022, null)) as n022_neg_finish
           ,min(iif(d184<0, d184, null)) as d184_neg_start
           ,max(iif(d184<0, d184, null)) as d184_neg_finish
           ------------------------------------------------
           ,min(iif(i016>0, i016, null)) as i016_pos_start
           ,max(iif(i016>0, i016, null)) as i016_pos_finish
           ,min(iif(i032>0, i032, null)) as i032_pos_start
           ,max(iif(i032>0, i032, null)) as i032_pos_finish
           ,min(iif(i064>0, i064, null)) as i064_pos_start
           ,max(iif(i064>0, i064, null)) as i064_pos_finish
           ,min(iif(i128>0, i128, null)) as i128_pos_start
           ,max(iif(i128>0, i128, null)) as i128_pos_finish
           ,min(iif(n022>0, n022, null)) as n022_pos_start
           ,max(iif(n022>0, n022, null)) as n022_pos_finish
           ,min(iif(d184>0, d184, null)) as d184_pos_start
           ,max(iif(d184>0, d184, null)) as d184_pos_finish
        from t_limits
    )
    select
        i016_neg_start
       ,i016_neg_finish
       ,(select listagg(n) within group(order by n) from generate_series(i016_neg_start, i016_neg_finish) as s(n)) as blob_id_i016_neg
       ,i032_neg_start
       ,i032_neg_finish
       ,(select listagg(n) within group(order by n) from generate_series(i032_neg_start, i032_neg_finish) as s(n)) as blob_id_i032_neg
       ,i064_neg_start
       ,i064_neg_finish
       ,(select listagg(n) within group(order by n) from generate_series(i064_neg_start, i064_neg_finish) as s(n)) as blob_id_i064_neg
       ,i128_neg_start
       ,i128_neg_finish
       ,(select listagg(n) within group(order by n) from generate_series(i128_neg_start, i128_neg_finish) as s(n)) as blob_id_i128_neg
       ,n022_neg_start
       ,n022_neg_finish
       ,(select listagg(n) within group(order by n) from generate_series(n022_neg_start, n022_neg_finish) as s(n)) as blob_id_n022_neg
       ,d184_neg_start
       ,d184_neg_finish
       ,(select listagg(n) within group(order by n) from generate_series(d184_neg_start, d184_neg_finish) as s(n)) as blob_id_d184_neg

       ,i016_pos_start
       ,i016_pos_finish
       ,(select listagg(n) within group(order by n desc) from generate_series(i016_pos_finish, i016_pos_start, -1) as s(n)) as blob_id_i016_pos
       ,i032_pos_start
       ,i032_pos_finish
       ,(select listagg(n) within group(order by n desc) from generate_series(i032_pos_finish, i032_pos_start, -1) as s(n)) as blob_id_i032_pos
       ,i064_pos_start
       ,i064_pos_finish
       ,(select listagg(n) within group(order by n desc) from generate_series(i064_pos_finish, i064_pos_start, -1) as s(n)) as blob_id_i064_pos
       ,i128_pos_start
       ,i128_pos_finish
       ,(select listagg(n) within group(order by n desc) from generate_series(i128_pos_finish, i128_pos_start, -1) as s(n)) as blob_id_i128_pos
       ,n022_pos_start
       ,n022_pos_finish
       ,(select listagg(n) within group(order by n desc) from generate_series(n022_pos_finish, n022_pos_start, -1) as s(n)) as blob_id_n022_pos
       ,d184_pos_start
       ,d184_pos_finish
       ,(select listagg(n) within group(order by n desc) from generate_series(d184_pos_finish, d184_pos_start, -1) as s(n)) as blob_id_d184_pos
    from a
    ;

    -- SELECT n FROM GENERATE_SERIES(1, 3) AS S(n);
"""

substitutions = [('BLOB_ID_.*', ''), ('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        I016_NEG_START                  -32768
        I016_NEG_FINISH                 -32767
        -32768,-32767
        I032_NEG_START                  -2147483648
        I032_NEG_FINISH                 -2147483647
        -2147483648,-2147483647
        I064_NEG_START                  -9223372036854775808
        I064_NEG_FINISH                 -9223372036854775807
        -9223372036854775808,-9223372036854775807
        I128_NEG_START                       -170141183460469231731687303715884105728
        I128_NEG_FINISH                      -170141183460469231731687303715884105727
        -170141183460469231731687303715884105728,-170141183460469231731687303715884105727
        N022_NEG_START                  -327.68
        N022_NEG_FINISH                 -326.68
        -327.68,-326.68
        D184_NEG_START                  -922337203685477.5808
        D184_NEG_FINISH                 -922337203685476.5808
        -922337203685477.5808,-922337203685476.5808
        I016_POS_START                  32766
        I016_POS_FINISH                 32767
        32767,32766
        I032_POS_START                  2147483646
        I032_POS_FINISH                 2147483647
        2147483647,2147483646
        I064_POS_START                  9223372036854775806
        I064_POS_FINISH                 9223372036854775807
        9223372036854775807,9223372036854775806
        I128_POS_START                        170141183460469231731687303715884105726
        I128_POS_FINISH                       170141183460469231731687303715884105727
        170141183460469231731687303715884105727,170141183460469231731687303715884105726
        N022_POS_START                  326.67
        N022_POS_FINISH                 327.67
        327.67,326.67
        D184_POS_START                  922337203685476.5807
        D184_POS_FINISH                 922337203685477.5807
        922337203685477.5807,922337203685476.5807
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
