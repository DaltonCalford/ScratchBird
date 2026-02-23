#coding:utf-8

"""
ID:          n/a
TITLE:       Tables involved in referential integrity have to meet restrictions when their types differ (persistent vs GTT; connection-bound vs transaction-bound).
DESCRIPTION:
    Test verifies that:
        * GTTs and regular ("permanent") tables cannot reference one another.
        * A connection-bound ("PRESERVE ROWS") GTT cannot reference a transaction-bound ("DELETE ROWS") GTT
    Work within a single attachment.
NOTES:
    [21.06.2025] pzotov
    1. ::: NB :::
       SQL schema name (6.x+), single and double quotes are suppressed in the output.
       See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md
    2. See doc:
       https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref50/firebird-50-language-reference.html#fblangref50-ddl-tbl-gtt-restrictions

    Discussed with Vlad, letters 16.06.2025 13:54 (subj: "#8598: ...")
    Checked on 6.0.0.838; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' '), ('(-)?At line \\d+.*', '')]
for p in addi_subst_tokens.split():
    substitutions.append( (p, '') )

db = db_factory()
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    
    test_sql = """
        set bail OFF;
        set list on;
        set count on;

        recreate table tmain_fixed (id int primary key using index tmain_fixed_pk);
        
        -- case-1a: GTT of type 'on commit PRESERVE rows' can not refer to persistent table:
        recreate global temporary table tdetl_gtt_keep_rows(id int primary key using index tdetl_pk, pid int references tmain_fixed(id) using index tdetl_keep_fk) on commit preserve rows;
        select * from rdb$relations where rdb$relation_name = upper('tdetl_gtt_keep_rows');

        -- case-1b: GTT of type 'on commit DELETE rows' can not refer to persistent table:
        recreate global temporary table tdetl_gtt_kill_rows(id int primary key using index tdetl_pk, pid int references tmain_fixed(id) using index tdetl_kill_fk) on commit delete rows;
        select * from rdb$relations where rdb$relation_name = upper('tdetl_gtt_kill_rows');
        commit;
        drop table tmain_fixed;


        -- case-1c: persistent table can not refer to GTT of type 'on commit PRESERVE rows':
        recreate global temporary table tmain_gtt_keep_rows(id int primary key using index tmain_keep_pk) on commit preserve rows;
        recreate table tdetl_fixed(id int primary key using index tdetl_fixed_pk, pid int references tmain_gtt_keep_rows(id) using index tdetl_fixed_fk);
        select * from rdb$relations where rdb$relation_name = upper('tdetl_fixed');
        commit;
        drop table tmain_gtt_keep_rows;

        -- case-1d: persistent table can not refer to GTT of type 'on commit DELETE rows':
        recreate global temporary table tmain_gtt_kill_rows(id int primary key using index tmain_kill_pk) on commit delete rows;
        recreate table tdetl_fixed(id int primary key using index tdetl_fixed_pk, pid int references tmain_gtt_kill_rows(id) using index tdetl_fixed_fk);
        select * from rdb$relations where rdb$relation_name = upper('tdetl_fixed');
        commit;
        drop table tmain_gtt_kill_rows;


        -- case-2a: GTT of type 'on commit PRESERVE rows' cannot refer a GTT of type 'on commit DELETE rows'
        recreate global temporary table tmain_gtt_kill_rows(id int primary key using index tmain_kill_pk) on commit delete rows;
        recreate global temporary table tdetl_gtt_keep_rows(id int primary key using index tdetl_keep_pk, pid int references tmain_gtt_kill_rows(id) using index tdetl_keep_fk) on commit preserve rows;
        select * from rdb$relations where rdb$relation_name = upper('tdetl_gtt_keep_rows');
        commit;
        drop table tmain_gtt_kill_rows;


        -- case-2b: GTT of type 'on commit DELETE rows' *** CAN ** refer a GTT of type 'on commit PRESERVE rows'
        recreate global temporary table tmain_gtt_keep_rows(id int primary key using index tmain_keep_pk) on commit preserve rows;
        recreate global temporary table tdetl_gtt_kill_rows(id int primary key using index tdetl_kill_pk, pid int references tmain_gtt_keep_rows(id) using index tdetl_kill_fk) on commit delete rows;
        commit;
        set count off;
        select count(*) from rdb$relations where rdb$relation_name = upper('tdetl_gtt_kill_rows');
        commit;
    """

    expected_stdout = """
        Statement failed, SQLSTATE = HY000
        unsuccessful metadata update
        -RECREATE TABLE TDETL_GTT_KEEP_ROWS failed
        -global temporary table TDETL_GTT_KEEP_ROWS of type ON COMMIT PRESERVE ROWS cannot reference persistent table TMAIN_FIXED
        Records affected: 0

        Statement failed, SQLSTATE = HY000
        unsuccessful metadata update
        -RECREATE TABLE TDETL_GTT_KILL_ROWS failed
        -global temporary table TDETL_GTT_KILL_ROWS of type ON COMMIT DELETE ROWS cannot reference persistent table TMAIN_FIXED
        Records affected: 0

        Statement failed, SQLSTATE = HY000
        unsuccessful metadata update
        -RECREATE TABLE TDETL_FIXED failed
        -persistent table TDETL_FIXED cannot reference global temporary table TMAIN_GTT_KEEP_ROWS of type ON COMMIT PRESERVE ROWS
        Records affected: 0

        Statement failed, SQLSTATE = HY000
        unsuccessful metadata update
        -RECREATE TABLE TDETL_FIXED failed
        -persistent table TDETL_FIXED cannot reference global temporary table TMAIN_GTT_KILL_ROWS of type ON COMMIT DELETE ROWS
        Records affected: 0

        Statement failed, SQLSTATE = HY000
        unsuccessful metadata update
        -RECREATE TABLE TDETL_GTT_KEEP_ROWS failed
        -global temporary table TDETL_GTT_KEEP_ROWS of type ON COMMIT PRESERVE ROWS cannot reference global temporary table TMAIN_GTT_KILL_ROWS of type ON COMMIT DELETE ROWS
        Records affected: 0

        COUNT 1
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], combine_output = True, input = test_sql)

    assert act.clean_stdout == act.clean_expected_stdout
