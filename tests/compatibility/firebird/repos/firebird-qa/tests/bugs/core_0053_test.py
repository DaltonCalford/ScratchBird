#coding:utf-8

"""
ID:          issue-377
ISSUE:       377
TITLE:       FIRST 1 vs ORDER DESC vs explicit plan (ODS11)
DESCRIPTION:
    Test uses pre-created database which has several procedures for analyzing performance by with the help of MON$ tables.
    Performance results are gathered in the table STAT_LOG, each odd run will save mon$ counters with "-" sign and next
    (even) run will save them with "+" -- see SP_GATHER_STAT.
    Aggegation of results is done in the view V_AGG_STAT (negative values relate to start, positive to the end of measure,
    difference between them means performance expenses which we want to evaluate).
    NOTE. Before each new measure we have to set generator G_GATHER_STAT to zero in order to make it produce proper values
    starting with 1 (odd --> NEGATIVE sign for counters). This is done in SP_TRUNCATE_STAT.

    :::::::::::::::::::::::::::::::::::::::: NB ::::::::::::::::::::::::::::::::::::
    18.08.2020. FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
    statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus next call
    gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
    See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d
    ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt

    Because of this, it was decided to change code of SP_TRUNCATE_STAT: instead of 'alter sequence restart...' we do
    reset like this: c = gen_id(g_gather_stat, -gen_id(g_gather_stat, 0));
JIRA:        CORE-53
FBTEST:      bugs.core_0053
NOTES:
    [22.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='mon-stat-gathering-2_5.fbk')

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [ ('line: \\d+, col: \\d+', '') ]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

test_script = """
    set list on;

    create or alter procedure gendata as begin end;
    recreate table test (F1 integer, F2 date);
    commit;

    set term ^;
    create or alter procedure GenData as
      declare i integer;
    begin
      i= 0;
      while (i < 100000) do begin
        insert into test(F1, F2) values (:i, 'yesterday');
        i= i+1;
      end
    end
    ^
    set term ;^
    commit;

    execute procedure gendata;
    commit;

    create desc index test_f1_f2 on test(F1, F2);
    commit;

    execute procedure sp_truncate_stat;
    commit;

    -- #################### MEASURE-1 #################

    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    set plan on;
    select first 1 f1
    from test t
    where t.f1=17 and f2 <= 'today'
    plan (T order test_f1_f2)
    order by F1 desc, F2 desc;
    set plan off;

    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    -- #################### MEASURE-2 #################


    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    set plan on;
    select first 1 f1
    from test t
    where t.f1=17 and f2 <= 'today'
    plan (t order test_f1_f2 index (test_f1_f2))
    order by F1 desc, F2 desc;
    set plan off;

    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    -- #################### ANALYZING RESULTS #################

    set list on;
    select
        iif( idx_1 / idx_2 > max_ratio, 'PLAN (T ORDER <idx_name>) is slow! Ratio > ' || max_ratio,
             iif( idx_2 / idx_1 > max_ratio, 'PLAN (T ORDER <idx_name> INDEX(<idx_name>)) is slow! Ratio > '|| max_ratio,
                                             'PERFORMANCE IS THE SAME.'
                )
           ) result
    from (
        select
            cast(min(idx_1) as double precision) as idx_1,
            cast( min(idx_2)  as double precision) as idx_2,
            3.00 as max_ratio
        from (
            select iif(rowset=1, indexed_reads, null) idx_1, iif(rowset=2, indexed_reads, null) idx_2
            from v_agg_stat
        ) g
    );
    -- Difference of indexed reads that is reported by MON$ tables:
    -- on 2.5 = {5, 5}, on 3.0 = {5, 3} ==> ratio 3.00 should be always enough.
"""

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    PLAN (T ORDER TEST_F1_F2)
    F1                              17
    PLAN (T ORDER TEST_F1_F2 INDEX (TEST_F1_F2))
    F1                              17
    RESULT                          PERFORMANCE IS THE SAME.
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

