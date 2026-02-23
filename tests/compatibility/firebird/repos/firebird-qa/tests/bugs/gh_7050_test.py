#coding:utf-8

"""
ID:          issue-7050
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/7050
TITLE:       Add table MON$COMPILED_STATEMENTS and columns
NOTES:
    [18.01.2024] pzotov
    Test based on example provided in doc/README.monitoring_tables
    Probably much useful test will be implemened later (with join mon$memory_usage etc).
    Checked on 6.0.0.213, 5.0.1.1307.

    [04.07.2025] pzotov
    Added 'SQL_SCHEMA_*_PREFIX' variables to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.894; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set blob all;
    set term ^;
    create or alter procedure fact_static_psql(a_n smallint) returns (n_factorial int128) as
    begin
        
        rdb$set_context('USER_TRANSACTION', 'N_FACTORIAL_' || a_n, a_n);

        if (a_n > 1) then
            begin
                select n_factorial from fact_static_psql(:a_n - 1) into n_factorial;
                rdb$set_context( 'USER_TRANSACTION',
                                 'N_FACTORIAL_' || a_n,
                                 cast( rdb$get_context('USER_TRANSACTION', 'N_FACTORIAL_' || a_n) as bigint) * n_factorial
                               );
            end
        else
            select 1 from mon$database into n_factorial;

        n_factorial = rdb$get_context('USER_TRANSACTION', 'N_FACTORIAL_' || a_n);
        rdb$set_context('USER_TRANSACTION', 'N_FACTORIAL_' || (a_n-2), null);
        suspend;

    end
    ^
    set term ;^
    commit;

    select n_factorial from fact_static_psql(5);

    set count on;
    -- ###################################
    -- ### M O N   S T A T E M E N T S ###
    -- ###################################
    select
        mon$statement_id as mon_sttm_id
        ,mon$compiled_statement_id as mon_cstm_id
        ,mon$stat_id as mon_stat_id
        ,mon$state as mon_state
        ,mon$sql_text as mon_sql_text_blob_id
        ,mon$explained_plan as mon_explained_plan_blob_id
    from mon$statements order by mon$statement_id;

    -- #########################################
    -- ### M O N   C O M P I L E D _ S T T M ###
    -- #########################################
    select
        mon$compiled_statement_id as mon_cstm_id
        ,mon$sql_text as mon_sql_text_blob_id
        ,mon$explained_plan as mon_explained_plan_blob_id
        ,mon$object_name as mon_obj_name
        ,mon$object_type as mon_obj_type
        ,mon$package_name as mon_pkg_name
        ,mon$stat_id as mon_stat_id
    from mon$compiled_statements order by mon$compiled_statement_id;

    -- ####################################
    -- ### M O N   C A L L _ S T A C K  ###
    -- ####################################
    select
        mon$statement_id as mon_sttm_id
        ,mon$call_id as mon_call_id
        ,mon$caller_id as mon_caller_id
        ,mon$stat_id as mon_stat_id
        ,mon$compiled_statement_id as mon_cstm_id
        ,mon$object_name as mon_obj_name
        ,mon$object_type as mon_obj_type
        ,mon$source_line as mon_src_row
        ,mon$source_column as mon_src_col
    from mon$call_stack order by mon$statement_id, mon$call_id;

    -- select mon$stat_id, mon$stat_group, mon$memory_used, mon$memory_allocated from mon$memory_usage m join mon$compiled_statements c using(mon$stat_id) order by mon$stat_id;

"""

subs = \
    [
        ('[ \t]+', ' ')
        ,('MON_SQL_TEXT_BLOB_ID .*', 'MON_SQL_TEXT_BLOB_ID')
        ,('MON_EXPLAINED_PLAN_BLOB_ID .*', 'MON_EXPLAINED_PLAN_BLOB_ID')
        ,('MON_STTM_ID .*', 'MON_STTM_ID')
        ,('MON_CSTM_ID .*', 'MON_CSTM_ID')
        ,('MON_STAT_ID .*', 'MON_STAT_ID')
        ,('MON_CALL_ID .*', 'MON_CALL_ID')
        ,('MON_CALLER_ID .*', 'MON_CALLER_ID')
        ,('MON_SRC_ROW .*', 'MON_SRC_ROW')
        ,('MON_SRC_COL .*', 'MON_SRC_COL')
        ,('\\(line \\d+, column \\d+\\)', '')
        #,('', '')
    ]

act = isql_act('db', test_script, substitutions = subs)

@pytest.mark.version('>=5.0')
def test_1(act: Action):

    SQL_SCHEMA_PUBLIC_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    SQL_SCHEMA_SYSTEM_PREFIX = '' if act.is_version('<6') else '"SYSTEM".'
    expected_stdout = f"""
        N_FACTORIAL 120
        MON_STTM_ID
        MON_CSTM_ID
        MON_STAT_ID
        MON_STATE                       1
        MON_SQL_TEXT_BLOB_ID
        select n_factorial from fact_static_psql(5)
        MON_EXPLAINED_PLAN_BLOB_ID
        Select Expression
        -> Procedure {SQL_SCHEMA_PUBLIC_PREFIX}"FACT_STATIC_PSQL" Scan
        Records affected: 1


        MON_CSTM_ID
        MON_SQL_TEXT_BLOB_ID
        select n_factorial from fact_static_psql(5)
        MON_EXPLAINED_PLAN_BLOB_ID
        Select Expression
        -> Procedure {SQL_SCHEMA_PUBLIC_PREFIX}"FACT_STATIC_PSQL" Scan
        MON_OBJ_NAME <null>
        MON_OBJ_TYPE <null>
        MON_PKG_NAME <null>
        MON_STAT_ID

        MON_CSTM_ID
        MON_SQL_TEXT_BLOB_ID
        MON_EXPLAINED_PLAN_BLOB_ID
        Select Expression
        -> Singularity Check
        -> Procedure {SQL_SCHEMA_PUBLIC_PREFIX}"FACT_STATIC_PSQL" Scan
        Select Expression
        -> Singularity Check
        -> Table {SQL_SCHEMA_SYSTEM_PREFIX}"MON$DATABASE" Full Scan
        MON_OBJ_NAME FACT_STATIC_PSQL
        MON_OBJ_TYPE 5
        MON_PKG_NAME <null>
        MON_STAT_ID
        Records affected: 2


        MON_STTM_ID
        MON_CALL_ID 192
        MON_CALLER_ID <null>
        MON_STAT_ID
        MON_CSTM_ID
        MON_OBJ_NAME FACT_STATIC_PSQL
        MON_OBJ_TYPE 5
        MON_SRC_ROW 8
        MON_SRC_COL 17

        MON_STTM_ID
        MON_CALL_ID 193
        MON_CALLER_ID 192
        MON_STAT_ID
        MON_CSTM_ID
        MON_OBJ_NAME FACT_STATIC_PSQL
        MON_OBJ_TYPE 5
        MON_SRC_ROW 8
        MON_SRC_COL 17

        MON_STTM_ID
        MON_CALL_ID 194
        MON_CALLER_ID 193
        MON_STAT_ID
        MON_CSTM_ID
        MON_OBJ_NAME FACT_STATIC_PSQL
        MON_OBJ_TYPE 5
        MON_SRC_ROW 8
        MON_SRC_COL 17

        MON_STTM_ID
        MON_CALL_ID 195
        MON_CALLER_ID 194
        MON_STAT_ID
        MON_CSTM_ID
        MON_OBJ_NAME FACT_STATIC_PSQL
        MON_OBJ_TYPE 5
        MON_SRC_ROW 8
        MON_SRC_COL 17

        MON_STTM_ID
        MON_CALL_ID 196
        MON_CALLER_ID 195
        MON_STAT_ID
        MON_CSTM_ID
        MON_OBJ_NAME FACT_STATIC_PSQL
        MON_OBJ_TYPE 5
        MON_SRC_ROW 15
        MON_SRC_COL 13
        Records affected: 5
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
