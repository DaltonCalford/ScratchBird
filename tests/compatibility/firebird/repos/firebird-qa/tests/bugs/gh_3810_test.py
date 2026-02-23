#coding:utf-8

"""
ID:          issue-3810
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/3810
TITLE:       Wrong or missing IS NULL optimization (regression) [CORE3449]
NOTES:
    [14.04.2023] pzotov
        Confirmed poor performance on 3.0.11.33678 (num of fetches = 10'099).
        Checked on 3.0.11.33681 -- all fine, fetches differ for less than 20.
    [22.09.2023] pzotov
        Changed name of table to simpler. 
        Removed usage of index test_fld2 after discuss with Anton Zuev (Red Soft) and dimitr,
        see: https://github.com/FirebirdSQL/firebird-qa/pull/19
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    create table test (fld1 integer, fld2 integer, fld3 integer);
    create index test_fld123 on test(fld1, fld2, fld3);
    -- create index test_fld2 on test(fld2);
    commit;
    set term ^;
    create or alter procedure tmp_sp1 as
        declare i integer;
    begin
        i=0;
        while ( i < 10000 ) do begin
            i=i+1;
            insert into test values (1, :i, 2);
        end
    end
    ^
    set term ;^
    commit;
    execute procedure tmp_sp1;
    commit;
    SET STATISTICS INDEX test_fld123;
    -- SET STATISTICS INDEX test_FLD2;
    commit;

    recreate view v_check as
    select i.mon$page_fetches as pg_fetches
    from mon$attachments a
    left join mon$io_stats i on a.mon$stat_id = i.mon$stat_id and i.mon$stat_group = 1
    where
        a.mon$attachment_id = current_connection
    ;

    -- set planonly;
    set list on;
    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION', 'fetches_a', (select pg_fetches from v_check));
    end
    ^
    set plan on
    ^
    select count(*) from test where fld1=1 and fld2 is null
    ^
    set plan off
    ^
    commit
    ^
    execute block as
    begin
        rdb$set_context('USER_SESSION', 'fetches_b', (select pg_fetches from v_check));
    end
    ^
    set term ;^

    select
        iif( fetches_b - fetches_a < max_threshold
            ,'OK, expected'
            ,'POOR: ' || cast(fetches_b - fetches_a as varchar(12)) || ' - exdeeds threshold = ' || cast(max_threshold as varchar(12))
           ) as fetches_diff
    from (
        select 
             cast(rdb$get_context('USER_SESSION', 'fetches_b') as int) as fetches_b
            ,cast(rdb$get_context('USER_SESSION', 'fetches_a') as int) as fetches_a
            ,50 as max_threshold
        --    ^
        --    |
        --    |   ###########################
        --    +-- ###  T H R E S H O L D  ###
        --        ###########################
        from rdb$database
    );
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN (TEST INDEX (TEST_FLD123))
    COUNT                           0
    FETCHES_DIFF                    OK, expected
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."TEST" INDEX ("PUBLIC"."TEST_FLD123"))
    COUNT                           0
    FETCHES_DIFF                    OK, expected
"""

@pytest.mark.version('>=3.0.11')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
