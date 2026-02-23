#coding:utf-8

"""
ID:          issue-3057
ISSUE:       3057
TITLE:       Improve sorting performance when long VARCHARs are involved
DESCRIPTION:
    Test verifies trivial queries with persistent and computed columns, predicates, views,
    expressions without reference to any column and datatypes which have no symmetrical
    transformation from value to a key (decfloat, time-with-timezone and varchar with non-default collation).

    It is supposed that default value of InlineSortThreshold parameter is 1000.
    No changes in the firebird.conf reuired.

    This test most probably will be added by some new examples later.
    Thanks to dimitr for lot of explanations (e-mail discussion was 28.12.2020).
JIRA:        CORE-2650
FBTEST:      bugs.core_2650
NOTES:
    [02.09.2024] pzotov
    1. Test was fully re-implemented in order to have ability to see both query and its comment in case of mismatch.
       The 'f'-notation is used in expected output with substitution query text and comments to it, e.g.:
           {query_map[1000][0]} // output will be compared with: "select txt_short from test a01 order by id"
           {query_map[1000][1]} // output will be compared with: "Must NOT use refetch because length of non-key column is less than threshold"
    2. Explained plans for FB 4.x and 5.x/6.x not equal.
       Because of this, expected output is stored differently (fb4x_expected_out, fb5x_expected_out).
    3. Indentation between 'Refetch' and subsequent 'Sort' was removed (only 'master' was affected).
       This change was caused by (or related to) profiler, discussed with Adriano.
       See letters since 24-aug-2023, subj:
           "Test for gh-7687: can't understand the output for trivial .sql (from the ticket)"
       See also:
           https://groups.google.com/g/firebird-devel/c/dWIgSIemys4/m/TzUWYwmVAQAJ (18-JUL-2023)
           https://github.com/FirebirdSQL/firebird/issues/7687 (28-JUL-2023)
    4  Need to preserve indentation was explained by dimitr 28-aug-2024 12:57, subj:
       'Explained plan, "Refetch": should the indentation be after it at line with subsequent "-> Sort (record ...)" ?'
    5. Indentation was restored 01-sep-2024 by fix https://github.com/FirebirdSQL/firebird/issues/8235
           https://github.com/FirebirdSQL/firebird/commit/901b4ced9a3615929e0027d42ebb2392e943b205

    Checked on 6.0.0.447-901b4ce, 5.0.2.1487, 4.0.6.3142

    [13.01.2025] pzotov
    6. Separated expected_out for FB 6.x after b2d03 2025.01.10 ("More correct plan output for subqueries generated during NOT IN transformation")
    7. Parameter OptimizeForFirstRows must have default value fcor this  test (i.e. false). To prevent test fail in case of occasional changing of
       this parameter, session-level command is used for FB 5.x+: 'set optimize for all rows'.

    [16.01.2025] pzotov
    8. Changed expected_out for FB 5.x after e24e0 2025.01.13 ("More correct plan output for subqueries generated during NOT IN transformation").
       Despite that now expected_out strings for 5.x and 6.x become equal, they remain separated in case of future changes in 6.x+

    Checked 6.0.0.573-c20f37a; 5.0.2.1592-2d11769
"""

import pytest
from firebird.qa import *

init_sql = """
    set bail on;
    recreate view v_unioned as select 1 id from rdb$database;
    commit;

    recreate table test(
        id int
        ,f01 int
        ,f02 int
        ,txt_short varchar(998)
        ,txt_broad varchar(999)
        ,computed_id_dup computed by( id*2 )
        ,computed_ts_dup computed by ( txt_short || txt_short )
        ,computed_tb_dup computed by ( txt_broad || txt_broad )
        ,computed_guid   computed by ( lpad('', 2000, uuid_to_char(gen_uuid()) ) )
        ,computed_ts_left computed by( left(txt_short,10) )
        ,computed_tb_left computed by( left(txt_broad,10) )
    );

    recreate table test_ns_01(
        id decfloat
        ,txt_short varchar(983)
    );

    recreate table test_ns_02(
        id decfloat
        ,txt_short varchar(982)
    );

    recreate table test_ns_03(
        id time with time zone
        ,txt_short varchar(991)
    );

    recreate table test_ns_04(
        id time with time zone
        ,txt_short varchar(990)
    );

    recreate table test_ns_05(
        id varchar(1) character set utf8 collate unicode_ci_ai
        ,txt_short varchar(993)
    );

    recreate table test_ns_06(
        id varchar(1) character set utf8 collate unicode_ci_ai
        ,txt_short varchar(992)
    );
    commit;

    recreate view v_unioned as
    select id, txt_broad from test
    union all
    select -1, 'qwerty'
    from rdb$database rows 0;
    commit;
"""
db = db_factory(init = init_sql)

query_map = {
     ##########################  p e r s i s t e n t    c o l u m n s   ##################################

     1000 : ( 'select txt_short from test a01 order by id' , 'Must NOT use refetch because length of non-key column is less than threshold' )
    ,1010 : ( 'select txt_broad from test a02 order by id' , 'MUST use refetch because length of non-key column is greater than threshold' )
    ,1020 : ( 'select txt_short from test a03 order by id rows 1' , 'MUST use refetch regardless on length of column because ROWS <N> presents' )

     ##########################  c o m p u t e d    c o l u m n s   #####################################

    ,2000 : ( 'select id, computed_ts_dup from test order by id' , 'Must NOT use refetch because computed column is based on txt_short with length < threshold' )
    ,2010 : ( 'select id, computed_tb_dup from test order by id' , 'MUST use refetch because computed column is based on txt_broad which has length >= threshold' )

    ######################   p r e d i c a t e s  [N O T]  I N,   A L L,   A N Y   ########################

    ,3000 : ( "select id from test a04 where '' in (select txt_short from test x04 where txt_short = '' order by id)" , '*** not [yet] commented ***' )
    ,3010 : ( "select id from test a05 where '' in (select txt_broad from test x05 where txt_broad = '' order by id)" , '*** not [yet] commented ***' )
    ,3020 : ( "select id from test a06 where '' not in (select txt_short from test x06 where txt_short>'' order by id)" , '*** not [yet] commented ***' )
    ,3030 : ( "select id from test a07 where '' not in (select txt_broad from test x07 where txt_broad>'' order by id)" , '*** not [yet] commented ***' )
    ,3040 : ( "select id from test a08 where '' > all (select id from test x08 where txt_short>'' order by id)" , '*** not [yet] commented ***' )
    ,3050 : ( "select id from test a09 where '' > all (select id from test x09 where txt_broad>'' order by id)" , '*** not [yet] commented ***' )
    ,3060 : ( "select id from test a10 where '' <> any (select id from test x10 where txt_short>'' order by id)" , '*** not [yet] commented ***' )
    ,3070 : ( "select id from test a11 where '' <> any (select id from test x11 where txt_broad>'' order by id)" , '*** not [yet] commented ***' )

    ########################################   e x i s t s   ###########################################
    # Predicate "EXISTS" must turn on refetching regardless of record length
    # but only when "WHERE" has column which not present in "ORDER BY"

    ,4000 : (  "select id,txt_short from test a12 where exists(select 1 from test x12 where txt_short>'' order by id)"
              ,"MUST use refetch: column x12.txt_short not present in order by"
            )
    ,4010 : (  "select id,txt_short from test a13 where exists(select 1 from test x13 where computed_id_dup > 0  order by id)"
              ,"Must NOT use refetch: ORDER BY list contains the single element: ID, and it is base for x13.computed_id_dup column"
            )
    ,4020 : (  "select id,txt_short from test a14 where exists(select 1 from test x14 where computed_id_dup > 0  order by computed_id_dup)"
              ,"""
                  MUST use refetch! See letter from dimitr 28.12.2020 14:49
                  Sort procedure will get:
                  a KEY = result of evaluating 'computed_id_dup';
                  a VAL = value of the field 'ID' which is base for computing 'computed_id_dup'
                  Thus sorter will have a field which not equals to a key, which leads to refetch.
               """
            )  
    ,4030 : (  "select id,txt_short from test a15 where exists(select 1 from test x15 where f02>0 and f01>0 order by f01, f02)"
              ,"Must NOT use refetch: all persistent columns from WHERE expression (f01, f02) belong to ORDER BY list"
            )
    ,4040 : (  "select id,txt_short from test a16 where exists(select 1 from test x16 where id>0 and f01>0 order by f01, f02)"
              ,"Must use refetch: one of columns from WHERE expr (id) does not belong to ORDER BY list"
            )
    ,4050 : (  "select id,txt_short from test a17 where exists(select 1 from test x17 where computed_id_dup > 0 order by f01)"
              ,"Must use refetch: computed column in WHERE expr does not belong to ORDER BY list"
            )
    ,4060 : (  "select id,txt_short from test a18 where exists(select 1 from test x18 where computed_guid > '' order by f01)"
              ,"Must NOT use refetch: computed column x18.computed_guid does is evaluated via GUID and does not refer to any columns"
            )
    ,4070 : (
               """
                  with recursive
                  r as (
                      select a19.id, a19.txt_short
                      from test a19
                      where not exists(select * from test x where x.txt_short < a19.txt_short order by id)
                      UNION ALL
                      select i.id, i.txt_short
                      from test i
                      join r on i.id > r.id
                      and not exists( select * from test x where x.txt_short between r.txt_short and i.txt_short order by id )
                  )
                  select * from r
               """
              ,"MUST use refetch both in anchor and recursive parts"
            )

     ######################################   v i e w s    ###########################################

    ,5000 : ( 'select txt_broad from v_unioned v01 order by id' , 'Must NOT use refetch because view DDL includes UNION' )

     #################################   e x p r e s s i o n s    #####################################

    ,6000 : (  'select left(txt_broad, 50) as txt from test a21 order by id'
              ,"""
                   MUST use refetch because expression is based on column which has length >= threshold
                   (even if final length of expression result is much less than threshold)
               """ 
            )
    ,6010 : (  'select left( txt_short || txt_short, 2000) as txt from test a22 order by id'
              ,"""
                   Must NOT use refetch because expression is based on column which has length < threshold
                   (even if final length of expression result is much bigger than threshold)
               """ 
            )

     ###########  n o n - s y m m e t r i c a l     k e y - v a l u e     d a t a t y p e s   #########

     # Following data types in common case have no ability to get column value from a key:
     # * International type text has a computed key
     # * Different decimal float values sometimes have same keys
     # * Date/time with time zones too.
     # Because of this, a field of any such datatype that is specified in "order by" list, must also be involved
     # in the non-key fields and sort will deal with such "concatenated" list.
     # If total length of such list not exceeds InlineSortThreshold then sort will be done without refetch.
     # Otherwise refetch will occur.
     # See src/jrd/opt.cpp, OPT_gen_sort() and explanation from dimitr, letter 28.12.2020 16:44
    ,7000 : ( 'select * from test_ns_01 a23 order by id' , 'MUST use refetch' )
    ,7010 : ( 'select * from test_ns_02 a24 order by id' , 'Must NOT refetch' )
    ,7020 : ( 'select * from test_ns_03 order by id' , 'MUST use refetch' )
    ,7030 : ( 'select * from test_ns_04 order by id' , 'Must NOT use refetch' )
    ,7040 : ( 'select * from test_ns_05 order by id' , 'MUST use refetch' )
    ,7050 : ( 'select * from test_ns_06 order by id' , 'Must NOT use refetch' )
}


###############################################################################

fb4x_expected_out = f"""
    1000
    {query_map[1000][0]}
    {query_map[1000][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Table "TEST" as "A01" Full Scan

    1010
    {query_map[1010][0]}
    {query_map[1010][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Table "TEST" as "A02" Full Scan

    1020
    {query_map[1020][0]}
    {query_map[1020][1]}
    Select Expression
    ....-> First N Records
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Table "TEST" as "A03" Full Scan

    2000
    {query_map[2000][0]}
    {query_map[2000][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Table "TEST" Full Scan

    2010
    {query_map[2010][0]}
    {query_map[2010][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Table "TEST" Full Scan

    3000
    {query_map[3000][0]}
    {query_map[3000][1]}
    Select Expression
    ....-> Filter
    ........-> Sort (record length: 1036, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X04" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A04" Full Scan

    3010
    {query_map[3010][0]}
    {query_map[3010][1]}
    Select Expression
    ....-> Filter
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Filter
    ....................-> Table "TEST" as "X05" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A05" Full Scan

    3020
    {query_map[3020][0]}
    {query_map[3020][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Filter
    ............-> Table "TEST" as "X06" Full Scan
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Filter
    ............-> Table "TEST" as "X06" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A06" Full Scan

    3030
    {query_map[3030][0]}
    {query_map[3030][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X07" Full Scan
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X07" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A07" Full Scan

    3040
    {query_map[3040][0]}
    {query_map[3040][1]}
    Select Expression
    ....-> Filter
    ........-> Sort (record length: 1036, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X08" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A08" Full Scan

    3050
    {query_map[3050][0]}
    {query_map[3050][1]}
    Select Expression
    ....-> Filter
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Filter
    ....................-> Table "TEST" as "X09" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A09" Full Scan

    3060
    {query_map[3060][0]}
    {query_map[3060][1]}
    Select Expression
    ....-> Filter
    ........-> Sort (record length: 1036, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X10" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A10" Full Scan

    3070
    {query_map[3070][0]}
    {query_map[3070][1]}
    Select Expression
    ....-> Filter
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Filter
    ....................-> Table "TEST" as "X11" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A11" Full Scan

    4000
    {query_map[4000][0]}
    {query_map[4000][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X12" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A12" Full Scan

    4010
    {query_map[4010][0]}
    {query_map[4010][1]}
    Select Expression
    ....-> Sort (record length: 28, key length: 8)
    ........-> Filter
    ............-> Table "TEST" as "X13" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A13" Full Scan

    4020
    {query_map[4020][0]}
    {query_map[4020][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 12)
    ............-> Filter
    ................-> Table "TEST" as "X14" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A14" Full Scan

    4030
    {query_map[4030][0]}
    {query_map[4030][1]}
    Select Expression
    ....-> Sort (record length: 36, key length: 16)
    ........-> Filter
    ............-> Table "TEST" as "X15" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A15" Full Scan

    4040
    {query_map[4040][0]}
    {query_map[4040][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 16)
    ............-> Filter
    ................-> Table "TEST" as "X16" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A16" Full Scan

    4050
    {query_map[4050][0]}
    {query_map[4050][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X17" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A17" Full Scan

    4060
    {query_map[4060][0]}
    {query_map[4060][1]}
    Select Expression
    ....-> Sort (record length: 28, key length: 8)
    ........-> Filter
    ............-> Table "TEST" as "X18" Full Scan
    Select Expression
    ....-> Filter
    ........-> Table "TEST" as "A18" Full Scan

    4070
    {query_map[4070][0]}
    {query_map[4070][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "R X" Full Scan
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "R X" Full Scan
    Select Expression
    ....-> Recursion
    ........-> Filter
    ............-> Table "TEST" as "R A19" Full Scan
    ........-> Filter
    ............-> Table "TEST" as "R I" Full Scan

    5000
    {query_map[5000][0]}
    {query_map[5000][1]}
    Select Expression
    ....-> Sort (record length: 4044, key length: 8)
    ........-> First N Records
    ............-> Union
    ................-> Table "TEST" as "V01 TEST" Full Scan
    ................-> Table "RDB$DATABASE" as "V01 RDB$DATABASE" Full Scan

    6000
    {query_map[6000][0]}
    {query_map[6000][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Table "TEST" as "A21" Full Scan

    6010
    {query_map[6010][0]}
    {query_map[6010][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Table "TEST" as "A22" Full Scan

    7000
    {query_map[7000][0]}
    {query_map[7000][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 44, key length: 24)
    ............-> Table "TEST_NS_01" as "A23" Full Scan

    7010
    {query_map[7010][0]}
    {query_map[7010][1]}
    Select Expression
    ....-> Sort (record length: 1052, key length: 24)
    ........-> Table "TEST_NS_02" as "A24" Full Scan

    7020
    {query_map[7020][0]}
    {query_map[7020][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 12)
    ............-> Table "TEST_NS_03" Full Scan

    7030
    {query_map[7030][0]}
    {query_map[7030][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 12)
    ........-> Table "TEST_NS_04" Full Scan

    7040
    {query_map[7040][0]}
    {query_map[7040][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 12)
    ............-> Table "TEST_NS_05" Full Scan

    7050
    {query_map[7050][0]}
    {query_map[7050][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 12)
    ........-> Table "TEST_NS_06" Full Scan
"""

###############################################################################

fb5x_expected_out = f"""
    1000
    select txt_short from test a01 order by id
    Must NOT use refetch because length of non-key column is less than threshold
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Table "TEST" as "A01" Full Scan
    1010
    select txt_broad from test a02 order by id
    MUST use refetch because length of non-key column is greater than threshold
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Table "TEST" as "A02" Full Scan
    1020
    select txt_short from test a03 order by id rows 1
    MUST use refetch regardless on length of column because ROWS <N> presents
    Select Expression
    ....-> First N Records
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Table "TEST" as "A03" Full Scan
    2000
    select id, computed_ts_dup from test order by id
    Must NOT use refetch because computed column is based on txt_short with length < threshold
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Table "TEST" Full Scan
    2010
    select id, computed_tb_dup from test order by id
    MUST use refetch because computed column is based on txt_broad which has length >= threshold
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Table "TEST" Full Scan
    3000
    select id from test a04 where '' in (select txt_short from test x04 where txt_short = '' order by id)
    *** not [yet] commented ***
    Sub-query (invariant)
    ....-> Filter
    ........-> Sort (record length: 1036, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X04" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A04" Full Scan
    3010
    select id from test a05 where '' in (select txt_broad from test x05 where txt_broad = '' order by id)
    *** not [yet] commented ***
    Sub-query (invariant)
    ....-> Filter
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Filter
    ....................-> Table "TEST" as "X05" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A05" Full Scan
    3020
    select id from test a06 where '' not in (select txt_short from test x06 where txt_short>'' order by id)
    *** not [yet] commented ***
    Sub-query (invariant)
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Filter
    ............-> Table "TEST" as "X06" Full Scan
    Sub-query (invariant)
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Filter
    ............-> Table "TEST" as "X06" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A06" Full Scan
    3030
    select id from test a07 where '' not in (select txt_broad from test x07 where txt_broad>'' order by id)
    *** not [yet] commented ***
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X07" Full Scan
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X07" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A07" Full Scan
    3040
    select id from test a08 where '' > all (select id from test x08 where txt_short>'' order by id)
    *** not [yet] commented ***
    Sub-query (invariant)
    ....-> Filter
    ........-> Sort (record length: 1036, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X08" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A08" Full Scan
    3050
    select id from test a09 where '' > all (select id from test x09 where txt_broad>'' order by id)
    *** not [yet] commented ***
    Sub-query (invariant)
    ....-> Filter
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Filter
    ....................-> Table "TEST" as "X09" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A09" Full Scan
    3060
    select id from test a10 where '' <> any (select id from test x10 where txt_short>'' order by id)
    *** not [yet] commented ***
    Sub-query (invariant)
    ....-> Filter
    ........-> Sort (record length: 1036, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X10" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A10" Full Scan
    3070
    select id from test a11 where '' <> any (select id from test x11 where txt_broad>'' order by id)
    *** not [yet] commented ***
    Sub-query (invariant)
    ....-> Filter
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Filter
    ....................-> Table "TEST" as "X11" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A11" Full Scan
    4000
    select id,txt_short from test a12 where exists(select 1 from test x12 where txt_short>'' order by id)
    MUST use refetch: column x12.txt_short not present in order by
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X12" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A12" Full Scan
    4010
    select id,txt_short from test a13 where exists(select 1 from test x13 where computed_id_dup > 0  order by id)
    Must NOT use refetch: ORDER BY list contains the single element: ID, and it is base for x13.computed_id_dup column
    Sub-query (invariant)
    ....-> Sort (record length: 28, key length: 8)
    ........-> Filter
    ............-> Table "TEST" as "X13" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A13" Full Scan
    4020
    select id,txt_short from test a14 where exists(select 1 from test x14 where computed_id_dup > 0  order by computed_id_dup)
    MUST use refetch! See letter from dimitr 28.12.2020 14:49
    Sort procedure will get:
    a KEY = result of evaluating 'computed_id_dup';
    a VAL = value of the field 'ID' which is base for computing 'computed_id_dup'
    Thus sorter will have a field which not equals to a key, which leads to refetch.
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 12)
    ............-> Filter
    ................-> Table "TEST" as "X14" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A14" Full Scan
    4030
    select id,txt_short from test a15 where exists(select 1 from test x15 where f02>0 and f01>0 order by f01, f02)
    Must NOT use refetch: all persistent columns from WHERE expression (f01, f02) belong to ORDER BY list
    Sub-query (invariant)
    ....-> Sort (record length: 36, key length: 16)
    ........-> Filter
    ............-> Table "TEST" as "X15" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A15" Full Scan
    4040
    select id,txt_short from test a16 where exists(select 1 from test x16 where id>0 and f01>0 order by f01, f02)
    Must use refetch: one of columns from WHERE expr (id) does not belong to ORDER BY list
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 16)
    ............-> Filter
    ................-> Table "TEST" as "X16" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A16" Full Scan
    4050
    select id,txt_short from test a17 where exists(select 1 from test x17 where computed_id_dup > 0 order by f01)
    Must use refetch: computed column in WHERE expr does not belong to ORDER BY list
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "X17" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A17" Full Scan
    4060
    select id,txt_short from test a18 where exists(select 1 from test x18 where computed_guid > '' order by f01)
    Must NOT use refetch: computed column x18.computed_guid does is evaluated via GUID and does not refer to any columns
    Sub-query (invariant)
    ....-> Sort (record length: 28, key length: 8)
    ........-> Filter
    ............-> Table "TEST" as "X18" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "TEST" as "A18" Full Scan
    4070
    with recursive
    r as (
    select a19.id, a19.txt_short
    from test a19
    where not exists(select * from test x where x.txt_short < a19.txt_short order by id)
    UNION ALL
    select i.id, i.txt_short
    from test i
    join r on i.id > r.id
    and not exists( select * from test x where x.txt_short between r.txt_short and i.txt_short order by id )
    )
    select * from r
    MUST use refetch both in anchor and recursive parts
    Sub-query
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "R X" Full Scan
    Sub-query
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "TEST" as "R X" Full Scan
    Select Expression
    ....-> Recursion
    ........-> Filter
    ............-> Table "TEST" as "R A19" Full Scan
    ........-> Filter
    ............-> Table "TEST" as "R I" Full Scan
    5000
    select txt_broad from v_unioned v01 order by id
    Must NOT use refetch because view DDL includes UNION
    Select Expression
    ....-> Sort (record length: 4044, key length: 8)
    ........-> First N Records
    ............-> Union
    ................-> Table "TEST" as "V01 TEST" Full Scan
    ................-> Table "RDB$DATABASE" as "V01 RDB$DATABASE" Full Scan
    6000
    select left(txt_broad, 50) as txt from test a21 order by id
    MUST use refetch because expression is based on column which has length >= threshold
    (even if final length of expression result is much less than threshold)
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Table "TEST" as "A21" Full Scan
    6010
    select left( txt_short || txt_short, 2000) as txt from test a22 order by id
    Must NOT use refetch because expression is based on column which has length < threshold
    (even if final length of expression result is much bigger than threshold)
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Table "TEST" as "A22" Full Scan
    7000
    select * from test_ns_01 a23 order by id
    MUST use refetch
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 44, key length: 24)
    ............-> Table "TEST_NS_01" as "A23" Full Scan
    7010
    select * from test_ns_02 a24 order by id
    Must NOT refetch
    Select Expression
    ....-> Sort (record length: 1052, key length: 24)
    ........-> Table "TEST_NS_02" as "A24" Full Scan
    7020
    select * from test_ns_03 order by id
    MUST use refetch
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 12)
    ............-> Table "TEST_NS_03" Full Scan
    7030
    select * from test_ns_04 order by id
    Must NOT use refetch
    Select Expression
    ....-> Sort (record length: 1036, key length: 12)
    ........-> Table "TEST_NS_04" Full Scan
    7040
    select * from test_ns_05 order by id
    MUST use refetch
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 12)
    ............-> Table "TEST_NS_05" Full Scan
    7050
    select * from test_ns_06 order by id
    Must NOT use refetch
    Select Expression
    ....-> Sort (record length: 1036, key length: 12)
    ........-> Table "TEST_NS_06" Full Scan
"""


###############################################################################

fb6x_expected_out = f"""
    1000
    {query_map[1000][0]}
    {query_map[1000][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Table "PUBLIC"."TEST" as "A01" Full Scan
    1010
    {query_map[1010][0]}
    {query_map[1010][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Table "PUBLIC"."TEST" as "A02" Full Scan
    1020
    {query_map[1020][0]}
    {query_map[1020][1]}
    Select Expression
    ....-> First N Records
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Table "PUBLIC"."TEST" as "A03" Full Scan
    2000
    {query_map[2000][0]}
    {query_map[2000][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Table "PUBLIC"."TEST" Full Scan
    2010
    {query_map[2010][0]}
    {query_map[2010][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Table "PUBLIC"."TEST" Full Scan
    3000
    {query_map[3000][0]}
    {query_map[3000][1]}
    Sub-query (invariant)
    ....-> Filter
    ........-> Sort (record length: 1036, key length: 8)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "X04" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A04" Full Scan
    3010
    {query_map[3010][0]}
    {query_map[3010][1]}
    Sub-query (invariant)
    ....-> Filter
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Filter
    ....................-> Table "PUBLIC"."TEST" as "X05" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A05" Full Scan
    3020
    {query_map[3020][0]}
    {query_map[3020][1]}
    Sub-query (invariant)
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Filter
    ............-> Table "PUBLIC"."TEST" as "X06" Full Scan
    Sub-query (invariant)
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Filter
    ............-> Table "PUBLIC"."TEST" as "X06" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A06" Full Scan
    3030
    {query_map[3030][0]}
    {query_map[3030][1]}
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "X07" Full Scan
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "X07" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A07" Full Scan
    3040
    {query_map[3040][0]}
    {query_map[3040][1]}
    Sub-query (invariant)
    ....-> Filter
    ........-> Sort (record length: 1036, key length: 8)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "X08" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A08" Full Scan
    3050
    {query_map[3050][0]}
    {query_map[3050][1]}
    Sub-query (invariant)
    ....-> Filter
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Filter
    ....................-> Table "PUBLIC"."TEST" as "X09" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A09" Full Scan
    3060
    {query_map[3060][0]}
    {query_map[3060][1]}
    Sub-query (invariant)
    ....-> Filter
    ........-> Sort (record length: 1036, key length: 8)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "X10" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A10" Full Scan
    3070
    {query_map[3070][0]}
    {query_map[3070][1]}
    Sub-query (invariant)
    ....-> Filter
    ........-> Refetch
    ............-> Sort (record length: 28, key length: 8)
    ................-> Filter
    ....................-> Table "PUBLIC"."TEST" as "X11" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A11" Full Scan
    4000
    {query_map[4000][0]}
    {query_map[4000][1]}
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "X12" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A12" Full Scan
    4010
    {query_map[4010][0]}
    {query_map[4010][1]}
    Sub-query (invariant)
    ....-> Sort (record length: 28, key length: 8)
    ........-> Filter
    ............-> Table "PUBLIC"."TEST" as "X13" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A13" Full Scan
    4020
    {query_map[4020][0]}
    {query_map[4020][1]}
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 12)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "X14" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A14" Full Scan
    4030
    {query_map[4030][0]}
    {query_map[4030][1]}
    Sub-query (invariant)
    ....-> Sort (record length: 36, key length: 16)
    ........-> Filter
    ............-> Table "PUBLIC"."TEST" as "X15" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A15" Full Scan
    4040
    {query_map[4040][0]}
    {query_map[4040][1]}
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 16)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "X16" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A16" Full Scan
    4050
    {query_map[4050][0]}
    {query_map[4050][1]}
    Sub-query (invariant)
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "X17" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A17" Full Scan
    4060
    {query_map[4060][0]}
    {query_map[4060][1]}
    Sub-query (invariant)
    ....-> Sort (record length: 28, key length: 8)
    ........-> Filter
    ............-> Table "PUBLIC"."TEST" as "X18" Full Scan
    Select Expression
    ....-> Filter (preliminary)
    ........-> Table "PUBLIC"."TEST" as "A18" Full Scan
    4070
    {query_map[4070][0]}
    {query_map[4070][1]}
    Sub-query
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "R" "X" Full Scan
    Sub-query
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Filter
    ................-> Table "PUBLIC"."TEST" as "R" "X" Full Scan
    Select Expression
    ....-> Recursion
    ........-> Filter
    ............-> Table "PUBLIC"."TEST" as "R" "A19" Full Scan
    ........-> Filter
    ............-> Table "PUBLIC"."TEST" as "R" "I" Full Scan
    5000
    {query_map[5000][0]}
    {query_map[5000][1]}
    Select Expression
    ....-> Sort (record length: 4044, key length: 8)
    ........-> First N Records
    ............-> Union
    ................-> Table "PUBLIC"."TEST" as "V01" "PUBLIC"."TEST" Full Scan
    ................-> Table "SYSTEM"."RDB$DATABASE" as "V01" "SYSTEM"."RDB$DATABASE" Full Scan
    6000
    {query_map[6000][0]}
    {query_map[6000][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 28, key length: 8)
    ............-> Table "PUBLIC"."TEST" as "A21" Full Scan
    6010
    {query_map[6010][0]}
    {query_map[6010][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 8)
    ........-> Table "PUBLIC"."TEST" as "A22" Full Scan
    7000
    {query_map[7000][0]}
    {query_map[7000][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 44, key length: 24)
    ............-> Table "PUBLIC"."TEST_NS_01" as "A23" Full Scan
    7010
    {query_map[7010][0]}
    {query_map[7010][1]}
    Select Expression
    ....-> Sort (record length: 1052, key length: 24)
    ........-> Table "PUBLIC"."TEST_NS_02" as "A24" Full Scan
    7020
    {query_map[7020][0]}
    {query_map[7020][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 12)
    ............-> Table "PUBLIC"."TEST_NS_03" Full Scan
    7030
    {query_map[7030][0]}
    {query_map[7030][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 12)
    ........-> Table "PUBLIC"."TEST_NS_04" Full Scan
    7040
    {query_map[7040][0]}
    {query_map[7040][1]}
    Select Expression
    ....-> Refetch
    ........-> Sort (record length: 36, key length: 12)
    ............-> Table "PUBLIC"."TEST_NS_05" Full Scan
    7050
    {query_map[7050][0]}
    {query_map[7050][1]}
    Select Expression
    ....-> Sort (record length: 1036, key length: 12)
    ........-> Table "PUBLIC"."TEST_NS_06" Full Scan
"""

act = python_act('db')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:

        # 13.01.2025: test will FAIL if config parameter OptimizeForFirstRows differs from default value (i.e. is set to true).
        # To prevent this, we have to explicitly change appropriate session-level value:
        if act.is_version('<5'):
            pass
        else:
            con.execute_immediate('set optimize for all rows')

        cur = con.cursor()
        for q_idx, q_tuple in query_map.items():
            test_sql, qry_comment = q_tuple[:2]
            ps = cur.prepare(test_sql)
            print(q_idx)
            print(test_sql)
            print(qry_comment)
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
            ps.free()

    act.expected_stdout = fb4x_expected_out if act.is_version('<5') else fb5x_expected_out if act.is_version('<6') else fb6x_expected_out 
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
