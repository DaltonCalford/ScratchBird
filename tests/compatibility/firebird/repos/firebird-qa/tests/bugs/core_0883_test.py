#coding:utf-8

"""
ID:          issue-1276
ISSUE:       1276
TITLE:       The built-in BLR printer doesn't support all FB2 features
DESCRIPTION:
JIRA:        CORE-883
FBTEST:      bugs.core_0883
NOTES:
    [25.06.2025] pzotov
    Important change has performed vs previous version of this test: we *create* new database here instead of using .fbk
    Additional lines present in BLR if we restore DB:
        "blr_flags / blr_flags_search_system_cache, 0,0 / blr_end"
    Sent letter to Adriano, 23.06.2025 17:07
    (subj: "BLR for stored procedure differs in 6.x depending on whether we create this SP in empty DB or this database was restored ...")
    See replies from Adriano: 23.06.2025 17:28, 24.06.2025 14:39 (summary: these addtitional lines are *still* required).

    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure sp1 as
        declare v_time time;
        declare v_timestamp timestamp;
        declare v_sp_id int;
        declare v_src_text blob;
        declare v_blr_text blob;
        declare c_sttm cursor for (
            select p.rdb$procedure_id, p.rdb$procedure_source, p.rdb$procedure_blr
            from rdb$procedures p
        );
    begin
        v_time = current_time (3);
        v_timestamp = current_timestamp(3);
        open c_sttm;
        while (1=1) do
        begin
          fetch c_sttm into v_sp_id, v_src_text, v_blr_text;
          if ( row_count = 0 ) then leave;
        end
        close c_sttm;
    end ^
    set term ;^
    commit;

    set list on;
    set blob all;
    select rdb$procedure_blr
    from rdb$procedures
    where rdb$procedure_name = upper('sp1');
"""

#act = isql_act('db', test_script, substitutions = [('[ \t]+', ' '),  ('RDB\\$PROCEDURE_BLR.*', '')])
act = isql_act('db', test_script, substitutions = [ ('RDB\\$PROCEDURE_BLR.*', '')])

expected_stdout_5x = """
    RDB$PROCEDURE_BLR               1a:1e2
    blr_version5,
    blr_begin,
       blr_message, 1, 1,0,
          blr_short, 0,
       blr_begin,
          blr_declare, 0,0, blr_sql_time,
          blr_assignment,
             blr_null,
             blr_variable, 0,0,
          blr_declare, 1,0, blr_timestamp,
          blr_assignment,
             blr_null,
             blr_variable, 1,0,
          blr_declare, 2,0, blr_long, 0,
          blr_assignment,
             blr_null,
             blr_variable, 2,0,
          blr_declare, 3,0, blr_blob2, 0,0, 0,0,
          blr_assignment,
             blr_null,
             blr_variable, 3,0,
          blr_declare, 4,0, blr_blob2, 0,0, 0,0,
          blr_assignment,
             blr_null,
             blr_variable, 4,0,
          blr_dcl_cursor, 0,0,
             blr_rse, 1,
                blr_relation2, 14, 'R','D','B','$','P','R','O','C','E','D','U','R','E','S',
                   8, 'C','_','S','T','T','M',32,'P', 0,
                blr_end,
             3,0,
             blr_derived_expr, 1, 0,
                blr_field, 0, 16, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','I','D',
             blr_derived_expr, 1, 0,
                blr_field, 0, 20, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','S','O','U','R','C','E',
             blr_derived_expr, 1, 0,
                blr_field, 0, 17, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','B','L','R',
          blr_stall,
          blr_label, 0,
             blr_begin,
                blr_begin,
                   blr_assignment,
                      blr_current_time2, 3,
                      blr_variable, 0,0,
                   blr_assignment,
                      blr_current_timestamp,
                      blr_variable, 1,0,
                   blr_cursor_stmt, 0, 0,0,
                   blr_label, 1,
                      blr_loop,
                         blr_begin,
                            blr_if,
                               blr_eql,
                                  blr_literal, blr_long, 0, 1,0,0,0,
                                  blr_literal, blr_long, 0, 1,0,0,0,
                               blr_begin,
                                  blr_begin,
                                     blr_cursor_stmt, 2, 0,0,
                                     blr_begin,
                                        blr_assignment,
                                           blr_field, 0, 16, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','I','D',
                                           blr_variable, 2,0,
                                        blr_assignment,
                                           blr_field, 0, 20, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','S','O','U','R','C','E',
                                           blr_variable, 3,0,
                                        blr_assignment,
                                           blr_field, 0, 17, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','B','L','R',
                                           blr_variable, 4,0,
                                        blr_end,
                                     blr_if,
                                        blr_eql,
                                           blr_internal_info,
                                              blr_literal, blr_long, 0, 5,0,0,0,
                                           blr_literal, blr_long, 0, 0,0,0,0,
                                        blr_leave, 1,
                                        blr_end,
                                     blr_end,
                                  blr_end,
                               blr_leave, 1,
                            blr_end,
                   blr_cursor_stmt, 1, 0,0,
                   blr_end,
                blr_end,
          blr_end,
       blr_send, 1,
          blr_begin,
             blr_assignment,
                blr_literal, blr_short, 0, 0,0,
                blr_parameter, 1, 0,0,
             blr_end,
       blr_end,
    blr_eoc
"""

expected_stdout_6x = """
    RDB$PROCEDURE_BLR               1a:1e2
    blr_version5,
    blr_begin,
       blr_message, 1, 1,0,
          blr_short, 0,
       blr_begin,
          blr_declare, 0,0, blr_sql_time,
          blr_declare, 1,0, blr_timestamp,
          blr_declare, 2,0, blr_long, 0,
          blr_declare, 3,0, blr_blob2, 0,0, 0,0,
          blr_declare, 4,0, blr_blob2, 0,0, 0,0,
          blr_dcl_cursor, 0,0,
             blr_rse, 1,
                blr_relation3,
                   6, 'S','Y','S','T','E','M',
                   14, 'R','D','B','$','P','R','O','C','E','D','U','R','E','S',
                   12, 34,'C','_','S','T','T','M',34,32,34,'P',34,
                   0,
                blr_end,
             3,0,
             blr_derived_expr, 1, 0,
                blr_field, 0, 16, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','I','D',
             blr_derived_expr, 1, 0,
                blr_field, 0, 20, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','S','O','U','R','C','E',
             blr_derived_expr, 1, 0,
                blr_field, 0, 17, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','B','L','R',
          blr_assignment,
             blr_null,
             blr_variable, 0,0,
          blr_assignment,
             blr_null,
             blr_variable, 1,0,
          blr_assignment,
             blr_null,
             blr_variable, 2,0,
          blr_assignment,
             blr_null,
             blr_variable, 3,0,
          blr_assignment,
             blr_null,
             blr_variable, 4,0,
          blr_stall,
          blr_label, 0,
             blr_begin,
                blr_begin,
                   blr_assignment,
                      blr_current_time2, 3,
                      blr_variable, 0,0,
                   blr_assignment,
                      blr_current_timestamp,
                      blr_variable, 1,0,
                   blr_cursor_stmt, 0, 0,0,
                   blr_label, 1,
                      blr_loop,
                         blr_begin,
                            blr_if,
                               blr_eql,
                                  blr_literal, blr_long, 0, 1,0,0,0,
                                  blr_literal, blr_long, 0, 1,0,0,0,
                               blr_begin,
                                  blr_begin,
                                     blr_cursor_stmt, 2, 0,0,
                                     blr_begin,
                                        blr_assignment,
                                           blr_field, 0, 16, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','I','D',
                                           blr_variable, 2,0,
                                        blr_assignment,
                                           blr_field, 0, 20, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','S','O','U','R','C','E',
                                           blr_variable, 3,0,
                                        blr_assignment,
                                           blr_field, 0, 17, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','B','L','R',
                                           blr_variable, 4,0,
                                        blr_end,
                                     blr_if,
                                        blr_eql,
                                           blr_internal_info,
                                              blr_literal, blr_long, 0, 5,0,0,0,
                                           blr_literal, blr_long, 0, 0,0,0,0,
                                        blr_leave, 1,
                                        blr_end,
                                     blr_end,
                                  blr_end,
                               blr_leave, 1,
                            blr_end,
                   blr_cursor_stmt, 1, 0,0,
                   blr_end,
                blr_end,
          blr_end,
       blr_send, 1,
          blr_begin,
             blr_assignment,
                blr_literal, blr_short, 0, 0,0,
                blr_parameter, 1, 0,0,
             blr_end,
       blr_end,
    blr_eoc
"""
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
