#coding:utf-8

"""
ID:          issue-4254
ISSUE:       4254
TITLE:       Improve SIMILAR TO performance
DESCRIPTION:
  Confirmed normal work on WI-T4.0.0.1598. Moreover, SIMILAR TO is about 5x faster than LIKE comparison in this test.

  CAUTION.
  This test must be run only on 4.0+, despite that its 'Fix version' = 3.0 Alpha 1.
  Performance of SIMILAR TO statement is extremely poor in comparison with LIKE operator:
  COUNT through the table of 102 records requires 27 seconds vs 16 ms (checked on WI-V6.3.6.33246).
JIRA:        CORE-3919
FBTEST:      bugs.core_3919
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3919.fbk')

test_script = """
    set heading off;
    set list on;
    set term ^;
    execute block returns(
        ratio_of_time varchar(255)
    ) as
        declare i int;
        declare j int;
        declare t0 timestamp;
        declare t1 timestamp;
        declare elap_ms_using_like int;
        declare elap_ms_using_similar_to int;
        declare s varchar(32761);
        declare ratio_similar_vs_like numeric(15,4);
        declare MAX_RATIO numeric(15,4) = 2;
        --            ^
        --      #############
        --      MAX THRESHOLD
        --      #############
        declare n_count int = 100; -- do not set it to values less than 10: duration should not be zero!
    begin

        t0 = cast('now' as timestamp);
        select count(*) as like_count, sum(char_length(b)) as like_sum_len
        from test t, (select 1 i from rdb$types rows (:n_count) ) n
        where
            t.b like '%a%' or
            t.b like '%b%' or
            t.b like '%c%' or
            t.b like '%d%' or
            t.b like '%e%' or
            t.b like '%f%' or
            t.b like '%g%' or
            t.b like '%h%' or
            t.b like '%i%' or
            t.b like '%j%' or
            t.b like '%k%' or
            t.b like '%l%' or
            t.b like '%m%' or
            t.b like '%n%' or
            t.b like '%o%' or
            t.b like '%p%' or
            t.b like '%q%' or
            t.b like '%r%' or
            t.b like '%s%' or
            t.b like '%t%' or
            t.b like '%u%' or
            t.b like '%v%' or
            t.b like '%w%' or
            t.b like '%x%' or
            t.b like '%y%' or
            t.b like '%z%'
        into i,j
        ;
        t1 = cast('now' as timestamp);
        elap_ms_using_like = datediff(millisecond from t0 to t1);

        t0 = cast('now' as timestamp);
        select count(*) as similar_to_count, sum(char_length(b)) as similar_to_sum_len
        from test t, (select 1 i from rdb$types rows  (:n_count) ) n
        where t.b similar to '%[a-z]%'
        into i,j
        ;
        t1 = cast('now' as timestamp);
        elap_ms_using_similar_to = datediff(millisecond from t0 to t1);

        ratio_similar_vs_like = 1.0000 * elap_ms_using_similar_to / elap_ms_using_like;

        ratio_of_time = iif( ratio_similar_vs_like < MAX_RATIO
                        ,'acceptable'
                        ,'TOO LONG: '|| ratio_similar_vs_like ||' times. This is more than max threshold = ' || MAX_RATIO || ' times'
                      )
        ;
        suspend;
    end
    ^
"""

act = isql_act('db', test_script)

expected_stdout = """
    RATIO_OF_TIME                   acceptable
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

