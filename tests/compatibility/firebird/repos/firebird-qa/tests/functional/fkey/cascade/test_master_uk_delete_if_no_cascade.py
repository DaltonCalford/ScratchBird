#coding:utf-8

"""
ID:          n/a
TITLE:       Attempt to delete record in master with UK must fail if there is FK record in detail and FK was declared without CASCADE option
DESCRIPTION:
    Test verifies RI mechanism when CASCADE option missed: record in master with UNIQUE constraint may be deleted 
    only if there is no appropriate record in detail (with value in FK equal to UK value from master) or all key fields are NULL.
    Single- and multi-segmented PK/FK are checked.
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

expected_stdout = """
    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_FK on table TDETL
    -Foreign key references are present for the record
    -Problematic key value is (ID = 1000)
    REMAINED_MASTER_ID 1000
    REMAINED_DETAIL_ID 100
    REMAINED_DETAIL_ID 101

    MON$VARIABLE_NAME FK_VIOLATION_335544466
    MON$VARIABLE_VALUE 7
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    
    test_sql = """
        set bail OFF;
        set list on;

        -- case-1: single segment UK:
        recreate table tmain(id int unique using index tmain_uk);
        recreate table tdetl(id int primary key, pid int, constraint tdetl_fk foreign key(pid) references tmain(id));
        insert into tmain(id) values(null);
        insert into tmain(id) values(1000);
        insert into tmain(id) values(2000);
        insert into tdetl(id, pid) values(100, 1000);
        insert into tdetl(id, pid) values(101, null);
        delete from tmain where id = 1000; -- must FAIL
        delete from tmain where id = 2000; -- must PASS
        delete from tmain where id is null; -- must PASS
        select id as remained_master_id from tmain order by id;
        select id as remained_detail_id from tdetl order by id;
        commit;
        drop table tdetl;
        drop table tmain;

        --######################################################

        -- case-2: multi-segment UK:
        recreate table tmain(id1 int, id2 int, id3 int, unique(id1, id2, id3) using index tmain_uk);
        recreate table tdetl(id int primary key, pid1 int, pid2 int, pid3 int, constraint tdetl_fk foreign key(pid1, pid2, pid3) references tmain(id1, id2, id3));

        insert into tmain(id1, id2, id3)
        with
        a as (
            select 1000 as v from rdb$database union all
            select 1000 as v from rdb$database union all
            select null as v from rdb$database
        )
        select distinct a1.v, a2.v, a3.v
        from a a1
        cross join a a2
        cross join a a3
        ;
        insert into tdetl(id, pid1, pid2, pid3)
        select row_number()over(), id1, id2, id3
        from tmain
        order by id1, id2, id3;

        -- tmain:
        -- ID1          ID2          ID3
        -- ============ ============ ============
        -- <null>       <null>       <null>
        -- <null>       <null>       1000
        -- <null>       1000         <null>
        -- <null>       1000         1000
        --   1000       <null>       <null>
        --   1000       <null>       1000
        --   1000       1000         <null>
        --   1000       1000         1000
        
        -- tdetl:
        -- ID           PID1         PID2         PID3
        -- ============ ============ ============ ============
        -- 1            <null>       <null>       <null>
        -- 2            <null>       <null>         1000
        -- 3            <null>         1000       <null>
        -- 4            <null>         1000         1000
        -- 5              1000       <null>       <null>
        -- 6              1000       <null>         1000
        -- 7              1000         1000       <null>
        -- 8              1000         1000         1000


        delete from tmain where coalesce(id1, id2, id3) is null; -- must PASS because all key fields are null

        -- Any records in tmain with at least one NOT null value among ID1 ... ID3 must not be deleted in following code.
        -- We try to delete every row with accumulating count of errors (for appropriate gdscode which must be the same: 335544466).
        -- Finally, we check content of mon$context_variables table: number of accumulated errors must be 7.
        set term ^;
        execute block as
            declare v_ctx_name varchar(255);
        begin
            for select id1, id2, id3 from tmain where coalesce(id1, id2, id3) is NOT null as cursor c
            do begin
                -- 335544466 : violation of FOREIGN KEY constraint "TDETL_FK" on table "PUBLIC"."TDETL"
                delete from tmain where current of c;
                when any do
                begin
                    v_ctx_name = 'FK_VIOLATION_' || gdscode;
                    rdb$set_context('USER_SESSION', v_ctx_name, coalesce(cast( rdb$get_context('USER_SESSION', v_ctx_name) as int), 0) + 1);
                end
            end
        end ^
        set term ;^
        select mon$variable_name, mon$variable_value from mon$context_variables where mon$variable_name starting with 'FK_VIOLATION_';
        commit;
        drop table tdetl;
        drop table tmain;
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], combine_output = True, input = test_sql)

    assert act.clean_stdout == act.clean_expected_stdout
