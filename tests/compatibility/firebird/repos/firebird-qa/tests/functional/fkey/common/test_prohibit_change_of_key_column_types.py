#coding:utf-8

"""
ID:          n/a
TITLE:       Attempt to change type of any column involved in PK/FK constraints must fail.
DESCRIPTION:
    Single- and multi-segmented PK/UK are checked.
    Work within a single transaction.
NOTES:
    [21.06.2025] pzotov
    ::: NB :::
    SQL schema name (6.x+), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

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

substitutions = [('[ \t]+', ' ')]
for p in addi_subst_tokens.split():
    substitutions.append( (p, '') )

db = db_factory()
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    
    test_sql = """
        set bail OFF;
        set autoddl off;
        set list on;

        -- all following statements shoudl FAIL:

        -- case-1a: single segment PK
        -- -------------------------
        recreate table tmain_single_pk(id_pk int primary key using index tmain_pk);
        recreate table tdetl_single_pk(id_pk int primary key, pid2pk int, constraint tdetl_fk foreign key(pid2pk) references tmain_single_pk(id_pk));
        commit;
        alter table tmain_single_pk alter column id_pk type bigint;
        alter table tdetl_single_pk alter column pid2pk type bigint;
        commit;
        drop table tdetl_single_pk;
        drop table tmain_single_pk;

        -- case-1b: single segment UK
        -- -------------------------
        recreate table tmain_single_uk(id_uk int unique using index tmain_pk);
        recreate table tdetl_single_uk(id int primary key, pid2uk int, constraint tdetl_fk foreign key(pid2uk) references tmain_single_uk(id_uk));
        commit;
        alter table tmain_single_uk alter column id_uk type bigint;
        alter table tdetl_single_uk alter column pid2uk type bigint;
        commit;
        drop table tdetl_single_uk;
        drop table tmain_single_uk;

        -- case-2a: multi segment PK
        -- -------------------------
        recreate table tmain_multi_pk(pk_id1 int, pk_id2 int, primary key(pk_id1, pk_id2) using index tmain_pk);
        recreate table tdetl_multi_pk(id_pk int primary key, fk_id1 int, fk_id2 int, constraint tdetl_fk foreign key(fk_id1, fk_id2) references tmain_multi_pk(pk_id1, pk_id2));
        commit;
        alter table tmain_multi_pk alter column pk_id2 type bigint;
        alter table tdetl_multi_pk alter column fk_id2 type bigint;
        commit;
        drop table tdetl_multi_pk;
        drop table tmain_multi_pk;
       
        -- case-2b: multi segment UK
        -- -------------------------
        recreate table tmain_multi_uk(pk_id1 int, pk_id2 int, unique(pk_id1, pk_id2) using index tmain_uk);
        recreate table tdetl_multi_uk(id_pk int primary key, fk_id1 int, fk_id2 int, constraint tdetl_fk foreign key(fk_id1, fk_id2) references tmain_multi_uk(pk_id1, pk_id2));
        commit;
        alter table tmain_multi_uk alter column pk_id2 type bigint;
        alter table tdetl_multi_uk alter column fk_id2 type bigint;
        commit;
        drop table tdetl_multi_uk;
        drop table tmain_multi_uk;
    """

    expected_stdout_3x = """
        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TMAIN_SINGLE_PK failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TDETL_SINGLE_PK failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TMAIN_SINGLE_UK failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TDETL_SINGLE_UK failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TMAIN_MULTI_PK failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TDETL_MULTI_PK failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TMAIN_MULTI_UK failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TDETL_MULTI_UK failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint
    """


    expected_stdout_6x = """
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TMAIN_SINGLE_PK failed
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TDETL_SINGLE_PK failed
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TMAIN_SINGLE_UK failed
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TDETL_SINGLE_UK failed
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TMAIN_MULTI_PK failed
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TDETL_MULTI_PK failed
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TMAIN_MULTI_UK failed
        -Cannot update index segment used by an Integrity Constraint

        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TDETL_MULTI_UK failed
        -Cannot update index segment used by an Integrity Constraint
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches=['-q'], combine_output = True, input = test_sql)

    assert act.clean_stdout == act.clean_expected_stdout
