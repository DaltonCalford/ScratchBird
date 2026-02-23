#coding:utf-8

"""
ID:    n/a
TITLE: FK-columns columns in the child table must either all be equal to appropriate columns in the parent or some/all of them must be null
DESCRIPTION:
    Single- and multi-segmented FK are checked.
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
    -Foreign key reference target does not exist
    -Problematic key value is (PID = 2)
    
    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_FK on table TDETL
    -Foreign key reference target does not exist
    -Problematic key value is (PID = 2)
    
    TDETL_ID 100
    TDETL_PID <null>
    TDETL_ID 101
    TDETL_PID 1
    Records affected: 2


    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_MULTI_FK on table TDETL_MULTI_FK
    -Foreign key reference target does not exist
    -Problematic key value is (FK_ID1 = 3333, FK_ID2 = 4444)

    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_MULTI_FK on table TDETL_MULTI_FK
    -Foreign key reference target does not exist
    -Problematic key value is (FK_ID1 = 3333, FK_ID2 = 2222)

    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_MULTI_FK on table TDETL_MULTI_FK
    -Foreign key reference target does not exist
    -Problematic key value is (FK_ID1 = 1111, FK_ID2 = 3333)

    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_MULTI_FK on table TDETL_MULTI_FK
    -Foreign key reference target does not exist
    -Problematic key value is (FK_ID1 = 3333, FK_ID2 = 1111)

    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_MULTI_FK on table TDETL_MULTI_FK
    -Foreign key reference target does not exist
    -Problematic key value is (FK_ID1 = 3333, FK_ID2 = 3333)

    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_MULTI_FK on table TDETL_MULTI_FK
    -Foreign key reference target does not exist
    -Problematic key value is (FK_ID1 = 9999, FK_ID2 = 3333)

    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_MULTI_FK on table TDETL_MULTI_FK
    -Foreign key reference target does not exist
    -Problematic key value is (FK_ID1 = 3333, FK_ID2 = 9999)

    TDETL_MULTI_ID 1
    TDETL_MULTI_FK1 <null>
    TDETL_MULTI_FK2 2222
    TDETL_MULTI_ID 2
    TDETL_MULTI_FK1 <null>
    TDETL_MULTI_FK2 2222
    TDETL_MULTI_ID 3
    TDETL_MULTI_FK1 1111
    TDETL_MULTI_FK2 <null>
    TDETL_MULTI_ID 4
    TDETL_MULTI_FK1 1111
    TDETL_MULTI_FK2 2222
    TDETL_MULTI_ID 5
    TDETL_MULTI_FK1 <null>
    TDETL_MULTI_FK2 7777
    TDETL_MULTI_ID 6
    TDETL_MULTI_FK1 8888
    TDETL_MULTI_FK2 <null>
    Records affected: 6

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    
    test_sql = """
        set bail OFF;
        set list on;

        -- case-1: single segment UK/FK:
        -- +++++++++++++++++++++++++++++
        recreate table tmain(id int unique using index tmain_uk);
        recreate table tdetl(id int unique using index tdetl_uk, pid int, constraint tdetl_fk foreign key(pid) references tmain(id));
        insert into tmain(id) values(1);
        insert into tdetl(id, pid) values(100, 1);    -- must PASS
        insert into tdetl(id, pid) values(101, null); -- must PASS
        insert into tdetl(id, pid) values(102, 2);    -- must FAIL // no record in tmain with ID = 2
        update tdetl set pid = 2 where id = 101; -- must FAIL // no record in tmain with ID = 2
        update tdetl set pid = 1 where id = 101; -- must PASS
        update tdetl set pid = null where id = 100; -- must PASS
        set count on;
        select d.id as tdetl_id, d.pid as tdetl_pid from tdetl d order by d.id;
        set count off;
        commit;
        drop table tdetl;
        drop table tmain;
        --------------------------------------------

        -- case-2: multi segment UK/FK:
        -- ++++++++++++++++++++++++++++
        recreate table tmain_multi_uk(id1 int, id2 int, unique(id1, id2) using index tmain_uk);
        recreate table tdetl_multi_fk(
            id int generated by default as identity
            ,fk_id1 int
            ,fk_id2 int
            ,constraint tdetl_multi_fk foreign key(fk_id1, fk_id2) references tmain_multi_uk(id1, id2)
        );
        commit;
        insert into tmain_multi_uk(id1, id2) values(1111, 2222);
        -- following six statements must PASS. First because of full match FK to parent UK, others because of nulls:
        insert into tdetl_multi_fk(id, fk_id1, fk_id2) values(1,1111, 2222);
        insert into tdetl_multi_fk(id, fk_id1, fk_id2) values(2,1111, null);
        insert into tdetl_multi_fk(id, fk_id1, fk_id2) values(3,null, 1111);
        insert into tdetl_multi_fk(id, fk_id1, fk_id2) values(4,null, null);
        insert into tdetl_multi_fk(id, fk_id1, fk_id2) values(5,9999, null);
        insert into tdetl_multi_fk(id, fk_id1, fk_id2) values(6,null, 9999);

        insert into tdetl_multi_fk(id,fk_id1, fk_id2) values(7,3333, 4444); -- must FAIL // no record in parent with (id1,id2) = (3333, 4444)

         -- Following must FAIL // no record in parent with appropriate UK which have not-null values
        update tdetl_multi_fk set fk_id1 = 3333 where id = 1;
        update tdetl_multi_fk set fk_id2 = 3333 where id = 2;
        update tdetl_multi_fk set fk_id1 = 3333 where id = 3;
        update tdetl_multi_fk set fk_id1 = 3333, fk_id2 = 3333 where id = 4;
        update tdetl_multi_fk set fk_id2 = 3333 where id = 5;
        update tdetl_multi_fk set fk_id1 = 3333 where id = 6;

        -- must PASS: we use null in at least one column values:
        update tdetl_multi_fk set fk_id1 = null where id = 1;
        update tdetl_multi_fk set fk_id1 = null, fk_id2 = 2222 where id = 2;
        update tdetl_multi_fk set fk_id1 = 1111, fk_id2 = null where id = 3;
        update tdetl_multi_fk set fk_id1 = 1111, fk_id2 = 2222 where id = 4; -- match to parent key
        update tdetl_multi_fk set fk_id1 = null, fk_id2 = 7777 where id = 5;
        update tdetl_multi_fk set fk_id1 = 8888, fk_id2 = null where id = 6;
        set count on;
        select d.id as tdetl_multi_id, d.fk_id1 as tdetl_multi_fk1, d.fk_id2 as tdetl_multi_fk2 from tdetl_multi_fk d order by d.id;
        set count off;
        commit;
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], combine_output = True, input = test_sql)

    assert act.clean_stdout == act.clean_expected_stdout
