#coding:utf-8

"""
ID:          issue-8418
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8418
TITLE:       UNLIST function. Check output when <input> and/or <separator> are evaluated in SELECT expression or passed as parameter, with [optional] RETURNING clause.
DESCRIPTION: Provided by red-soft. Original file name: "unlist.test_expression.py"
NOTES:
    [09.04.2025] pzotov
    Checked on 6.0.0.725
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table test_table(text varchar(15));
    insert into test_table (text) values('123,123,123');

    select a.test_01 from unlist( (select * from test_table) ) as a(test_01);

    select a.test_02 from unlist( (select * from test_table), ',' returning varchar(15)) as a(test_02);

    set term ^ ;
    -------------------------------------------------------------------------------------------------------
    recreate procedure test_proc(a_text blob sub_type text) returns ( test_03 int128)
    as
    begin
      for
          select *
          -- from unlist( :a_text ) as a ==> "SQLSTATE = 22001 / -string right truncation / -expected length 32, actual 40"
          from unlist(:a_text, ',' returning int128) as a
      into
          :test_03
      do
          suspend;
    end
    ^
    select a.test_03 from test_proc(',-170141183460469231731687303715884105728,170141183460469231731687303715884105727') as a
    ^
    -------------------------------------------------------------------------------------------------------
    recreate function test_func returns blob sub_type text as
    begin
        return
               '-9.999999999999999E+384'
            || '`-1.0E-383'
            || '`1.0E-383'
            || '`9.999999999999999E+384'
            || '`-9.999999999999999999999999999999999E+6144'
            || '`-1.0E-6143'
            || '`1.0E-6143'
            || '`9.999999999999999999999999999999999E+6144'
            || '`0E+369'
            || '`0E-384'
            || '`0E-384'
            || '`0E+369'
            || '`0E+6111'
            || '`0E-6144'
            || '`0E-6144'
            || '`0E+6111'
        ;
        -- suspend;
    end
    ^
    select a.test_04 from unlist( (select test_func() from rdb$database), '`' returning decfloat ) as a(test_04)
    ^
    -------------------------------------------------------------------------------------------------------
    set term ;^

    select a.* from unlist( (select '111,555,999' from rdb$database), (select ascii_char(44) from rdb$database) ) as a(test_05);

    select a.* from unlist( (select cast('###1@@@1$$$' as blob sub_type text) from rdb$database), (select row_number()over() from rdb$database) ) as a(test_06);

    select a.* from unlist( ('741' || ',' || '852' || ',' || '963') ) as a(test_07);

    select a.* from unlist( lpad('444,555,333' , 12, '') ) as a(test_08);

    select a.* from unlist( (select blob_append('987', '#', '654', '#', '321') from rdb$database),  (select blob_append(null, '#') from rdb$database)) as a(test_09);

"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

expected_stdout = """
    TEST_01 123
    TEST_01 123
    TEST_01 123
    TEST_02 123
    TEST_02 123
    TEST_02 123
    TEST_03 -170141183460469231731687303715884105728
    TEST_03 170141183460469231731687303715884105727
    TEST_04 -9.999999999999999E+384
    TEST_04 -1.0E-383
    TEST_04 1.0E-383
    TEST_04 9.999999999999999E+384
    TEST_04 -9.999999999999999999999999999999999E+6144
    TEST_04 -1.0E-6143
    TEST_04 1.0E-6143
    TEST_04 9.999999999999999999999999999999999E+6144
    TEST_04 0E+369
    TEST_04 0E-384
    TEST_04 0E-384
    TEST_04 0E+369
    TEST_04 0E+6111
    TEST_04 0E-6144
    TEST_04 0E-6144
    TEST_04 0E+6111
    TEST_05 111
    TEST_05 555
    TEST_05 999
    TEST_06 ###
    TEST_06 @@@
    TEST_06 $$$
    TEST_07 741
    TEST_07 852
    TEST_07 963
    TEST_08 444
    TEST_08 555
    TEST_08 333
    TEST_09 987
    TEST_09 654
    TEST_09 321
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
