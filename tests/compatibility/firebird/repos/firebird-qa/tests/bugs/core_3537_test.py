#coding:utf-8

"""
ID:          issue-3894
ISSUE:       3894
TITLE:       There is no need to undo changes made in GTT created with ON COMMIT DELETE ROWS option when transaction is rolled back
DESCRIPTION:
    After discuss with hvlad it was decided to use fetches & marks values that are issued in trace
    ROLLBACK_TRANSACTION statistics and evaluate ratio of these values with:
    1) number of inserted rows(see 'NUM_ROWS_TO_BE_ADDED' constant);
    2) number of data pages that table occupies (it's retieved via 'gstat -t T_FIX_TAB').

    We use three tables with the same DDL: permanent ('t_fix_tab'), GTT PRESERVE and GTT DELETE rows.
    All these tables are subject to DML which does insert rows.
    Permanent table is used for retrieving statistics of data pages that are in use after this DML.
    Number of rows that we add into tables should not be very high, otherwise rollback will be done via TIP,
    i.e. without real undone actions ==> we will not see proper ratios.
    After serveral runs it was decided to use value = 45000 (rows).
    All ratios should belong to some range with +/-5% of possible difference from one run to another.
JIRA:        CORE-3537
FBTEST:      bugs.core_3537
NOTES:
    [24.07.2025] pzotov
    Changed THRESHOLD values after start usage DB with page_size = 8192 (see 'check_data' dict).
    Checked on 6.0.0.1061; 5.0.3.1686; 4.0.6.3223; 3.0.13.33818.
"""

import pytest
from firebird.qa import *

init_script = """
    set bail on;
    set echo on;
    create or alter procedure sp_fill_fix_tab as begin end;
    create or alter procedure sp_fill_gtt_del_rows as begin end;
    create or alter procedure sp_fill_gtt_sav_rows as begin end;

    recreate view v_field_len as
    select rf.rdb$relation_name as rel_name, f.rdb$field_length as fld_len
    from rdb$relation_fields rf
    join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
    ;

    recreate table t_fix_tab(
      s1 varchar(50)
      -- unique using index t_fix_tab_s1
    );

    recreate global temporary table t_gtt_del_rows(
      s1 varchar(50)
      -- unique using index t_gtt_del_rows_s1
    ) on commit DELETE rows;

    recreate global temporary table t_gtt_sav_rows(
      s1 varchar(50)
      -- unique using index t_gtt_sav_rows_s1
    ) on commit PRESERVE rows;

    commit;

    set term ^;
    create or alter procedure sp_fill_fix_tab(a_rows int) as
        declare k int;
        declare w int;
    begin
        k=a_rows;
        select v.fld_len from v_field_len v where v.rel_name=upper('t_fix_tab') into w;
        while(k>0) do
        begin
            insert into t_fix_tab(s1) values( rpad('', :w, uuid_to_char(gen_uuid()) ) );
            if (mod(k-1, 5000) = 0) then
                rdb$set_context('USER_SESSION','DBG_FILL_FIX_TAB',a_rows - k); -- to be watched in the trace log (4DEBUG)
            k = k - 1;
        end
    end
    ^
    create or alter procedure sp_fill_gtt_del_rows(a_rows int) as
        declare k int;
        declare w int;
    begin
        k=a_rows;
        select v.fld_len from v_field_len v where v.rel_name=upper('t_gtt_del_rows') into w;
        while(k>0) do
        begin
            insert into t_gtt_del_rows(s1) values( rpad('', :w, uuid_to_char(gen_uuid()) ) );
            if (mod(k-1, 5000) = 0) then
                rdb$set_context('USER_SESSION','DBG_FILL_GTT_DEL',a_rows - k); -- to be watched in the trace log (4DEBUG)
            k = k - 1;
        end
        rdb$set_context('USER_SESSION','DBG_FILL_GTT_DEL',a_rows);
    end
    ^
    create or alter procedure sp_fill_gtt_sav_rows(a_rows int) as
        declare k int;
        declare w int;
    begin
        k=a_rows;
        select v.fld_len from v_field_len v where v.rel_name=upper('t_gtt_sav_rows') into w;
        while(k>0) do
        begin
            insert into t_gtt_sav_rows(s1) values( rpad('', :w, uuid_to_char(gen_uuid()) ) );
            if (mod(k-1, 5000) = 0) then
                rdb$set_context('USER_SESSION','DBG_FILL_GTT_SAV',a_rows - k); -- to be watched in the trace log (4DEBUG)
            k = k - 1;
        end
        rdb$set_context('USER_SESSION','DBG_FILL_GTT_SAV',a_rows);
    end
    ^
    set term ;^
    commit;
"""

# NOTE: Calculation depend on page_size. Since 6.x min size = 8K.
db = db_factory(page_size = 8192, init=init_script)

act = python_act('db')

trace = ['log_transactions = true',
           'print_perf = true',
           'log_initfini = false',
           ]

@pytest.mark.trace
@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    
    NUM_ROWS_TO_BE_ADDED = 45000

    # Make initial data filling into PERMANENT table for retrieving later number of data pages
    # (it should be the same for any kind of tables, including GTTs):
    with act.db.connect() as con:
        c = con.cursor()
        c.call_procedure('sp_fill_fix_tab', [NUM_ROWS_TO_BE_ADDED])
        con.commit()
    #
    with act.trace(db_events=trace):
        with act.db.connect() as con1:
            c = con1.cursor()
            c.call_procedure('sp_fill_gtt_sav_rows', [NUM_ROWS_TO_BE_ADDED])
            con1.rollback()
        with act.db.connect() as con2:
            c = con2.cursor()
            c.call_procedure('sp_fill_gtt_del_rows', [NUM_ROWS_TO_BE_ADDED])
            con2.rollback()
    # Obtain statistics for table T_FIX_TAB in order to estimate numberof data pages
    dp_cnt = 0
    act.gstat(switches=['-a','-t', 'T_FIX_TAB', '-u', act.db.user, '-p', act.db.password])
    for line in act.stdout.splitlines():
        if 'data pages' in line.lower():
            # Data pages: 1098, data page slots: 1098, average fill: 74% ==> 1098
            dp_cnt = int(line.replace(',', ' ').split()[2])
    #
    gtt_sav_fetches = -1
    gtt_sav_marks = -1
    gtt_del_fetches = -1
    gtt_del_marks = -1
    gtt_del_trace = ''
    gtt_sav_trace = ''
    for line in act.trace_log:
        if 'fetch' in line:
            # 2.5.7:
            # ['370', 'ms,', '1100', 'read(s),', '1358', 'write(s),', '410489', 'fetch(es),', '93294', 'mark(s)']
            # ['2', 'ms,', '1', 'read(s),', '257', 'write(s),', '1105', 'fetch(es),', '1102', 'mark(s)']
            # 3.0.2:
            # 618 ms, 1 read(s), 2210 write(s), 231593 fetch(es), 92334 mark(s)
            # 14 ms, 1109 write(s), 7 fetch(es), 4 mark(s)
            words = line.split()
            for k in range(len(words)):
                if words[k].startswith('fetch'):
                    if gtt_sav_fetches == -1:
                        gtt_sav_fetches = int(words[k-1])
                        gtt_sav_trace = line.strip()
                    else:
                        gtt_del_fetches = int(words[k-1])
                        gtt_del_trace = line.strip()

                if words[k].startswith('mark'):
                    if gtt_sav_marks==-1:
                        gtt_sav_marks = int(words[k-1])
                    else:
                        gtt_del_marks = int(words[k-1])

    # Changed 24.07.2025 after several runs on 3.x ... 6.x:
    if act.is_version('<4'):
        check_data = {
            'ratio_fetches_to_row_count_for_GTT_PRESERVE_ROWS' : (1.00 * gtt_sav_fetches / NUM_ROWS_TO_BE_ADDED,    5.07240),
            'ratio_fetches_to_row_count_for_GTT_DELETE_ROWS' : (1.00 * gtt_del_fetches / NUM_ROWS_TO_BE_ADDED,      0.00011),
            'ratio_marks_to_row_count_for_GTT_PRESERVE_ROWS' : (1.00 * gtt_sav_marks / NUM_ROWS_TO_BE_ADDED,        2.02564),
            'ratio_marks_to_row_count_for_GTT_DELETE_ROWS' : (1.00 * gtt_del_marks / NUM_ROWS_TO_BE_ADDED,          0.0000667),
            'ratio_fetches_to_datapages_for_GTT_PRESERVE_ROWS' : (1.00 * gtt_sav_fetches / dp_cnt,                419.59191),
            'ratio_fetches_to_datapages_for_GTT_DELETE_ROWS' : (1.00 * gtt_del_fetches / dp_cnt,                    0.00919),
            'ratio_marks_to_datapages_for_GTT_PRESERVE_ROWS' : (1.00 * gtt_sav_marks / dp_cnt,                    167.56250),
            'ratio_marks_to_datapages_for_GTT_DELETE_ROWS' : (1.00 * gtt_del_marks / dp_cnt,                        0.00551),
        }
    else:
        check_data = {
            'ratio_fetches_to_row_count_for_GTT_PRESERVE_ROWS' : (1.00 * gtt_sav_fetches / NUM_ROWS_TO_BE_ADDED,    5.07707),
            'ratio_fetches_to_row_count_for_GTT_DELETE_ROWS' : (1.00 * gtt_del_fetches / NUM_ROWS_TO_BE_ADDED,      0.00011),
            'ratio_marks_to_row_count_for_GTT_PRESERVE_ROWS' : (1.00 * gtt_sav_marks / NUM_ROWS_TO_BE_ADDED,        2.02727),
            'ratio_marks_to_row_count_for_GTT_DELETE_ROWS' : (1.00 * gtt_del_marks / NUM_ROWS_TO_BE_ADDED,          0.0000667),
            'ratio_fetches_to_datapages_for_GTT_PRESERVE_ROWS' : (1.00 * gtt_sav_fetches / dp_cnt,                391.21233),
            'ratio_fetches_to_datapages_for_GTT_DELETE_ROWS' : (1.00 * gtt_del_fetches / dp_cnt,                    0.00856),
            'ratio_marks_to_datapages_for_GTT_PRESERVE_ROWS' : (1.00 * gtt_sav_marks / dp_cnt,                    156.21061),
            'ratio_marks_to_datapages_for_GTT_DELETE_ROWS' : (1.00 * gtt_del_marks / dp_cnt,                        0.00514),
        }


    MAX_DIFF_PERCENT = 5.0

    failed_flag = False
    for k, v in sorted(check_data.items()):
        msg = ('Check ' + k + ': ' +
               ('OK' if v[1] * ((100 - MAX_DIFF_PERCENT)/100) <= v[0] <= v[1] * (100+MAX_DIFF_PERCENT) / 100
                else 'value '+str(v[0])+' not in range '+str( v[1] ) + ' +/-' + str(MAX_DIFF_PERCENT) + '%')
               )
        print(msg)
        failed_flag = 'not in range' in msg

    if failed_flag:
        print('Trace for GTT PRESERVE rows: ' + gtt_sav_trace)
        print('Trace for GTT DELETE   rows: ' + gtt_del_trace)

    expected_stdout = """
        Check ratio_fetches_to_datapages_for_GTT_DELETE_ROWS: OK
        Check ratio_fetches_to_datapages_for_GTT_PRESERVE_ROWS: OK
        Check ratio_fetches_to_row_count_for_GTT_DELETE_ROWS: OK
        Check ratio_fetches_to_row_count_for_GTT_PRESERVE_ROWS: OK
        Check ratio_marks_to_datapages_for_GTT_DELETE_ROWS: OK
        Check ratio_marks_to_datapages_for_GTT_PRESERVE_ROWS: OK
        Check ratio_marks_to_row_count_for_GTT_DELETE_ROWS: OK
        Check ratio_marks_to_row_count_for_GTT_PRESERVE_ROWS: OK
    """

    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
