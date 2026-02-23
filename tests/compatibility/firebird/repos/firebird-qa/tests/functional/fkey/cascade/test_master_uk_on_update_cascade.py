#coding:utf-8

"""
ID:          n/a
TITLE:       Updating PK column(s) in master must cause changes in appropriate detail column(s) if 'ON UPDATE CASCADE' option is used
DESCRIPTION:
    Test verifies RI mechanism when ON UPDATE CASCADE option is used: updating record in master should cause appropriate updates in detail.
    Single- and multi-segmented PK/FK are checked.
    Work within a single transaction.
NOTES:
    [17.06.2025] pzotov
    1. Extended 'subsitutions' list is used here to suppress "PUBLIC" schema prefix and remove single/double quotes from all object names. Need since 6.0.0.834.
       ::: NB :::
       File act.files_dir/'test_config.ini' must contain section:
           [schema_n_quotes_suppress]
           addi_subst="PUBLIC". " '
       (this file is used in qa/plugin.py, see QA_GLOBALS dictionary).

       Value of parameter 'addi_subst' is splitted on tokens using space character and we add every token to 'substitutions' list which
       eventually will be like this:
           substitutions = [ ( <optional: previous tuples>, ('"PUBLIC".', ''), ('"', ''), ("'", '') ]
    2. Adjusted expected output: removed single quotes from DB object name(s).

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
    violation of FOREIGN KEY constraint TDETL_UK on table TDETL
    -Foreign key references are present for the record
    -Problematic key value is (ID = -1)

    DETAIL_ID 100
    DETAIL_PID -1
    DETAIL_ID 101
    DETAIL_PID -1

    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint TDETL_FK on table TDETL
    -Foreign key references are present for the record
    -Problematic key value is (ID1 = -1, ID2 = -1, ID3 = -1)

    DETAIL_ID 200
    DETAIL_PID1 -1
    DETAIL_PID2 -1
    DETAIL_PID3 -1
    DETAIL_ID 201
    DETAIL_PID1 -1
    DETAIL_PID2 -1
    DETAIL_PID3 -1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    
    test_sql = """
        set bail OFF;
        set list on;

        -- case-1: single segment UK:
        recreate table tmain(id int unique using index tmain_uk);
        recreate table tdetl(
            id int primary key
            ,pid int
            ,constraint tdetl_uk foreign key(pid) references tmain(id)
                ON UPDATE CASCADE
        );
        insert into tmain(id) values(1);
        insert into tdetl(id, pid) values(100, 1);
        insert into tdetl(id, pid) values(101, 1);
        update tmain set id = -1 where id = 1; -- must PASS and cause update in tdetl
        update tmain set id = null where id = -1; -- must FAIL: 'on update SET NULL' must be for this case!
        select id as detail_id, pid as detail_pid from tdetl order by id;
        commit;
        drop table tdetl;
        drop table tmain;

        --------------------------------------------

        -- case-2: multi-segment UK:
        recreate table tmain(id1 int, id2 int, id3 int, unique(id1, id2, id3) using index tmain_uk);
        recreate table tdetl(
            id int primary key
            ,pid1 int
            ,pid2 int
            ,pid3 int
            ,constraint tdetl_fk foreign key(pid1, pid2, pid3) references tmain(id1, id2, id3)
                ON UPDATE CASCADE
        );
        insert into tmain(id1, id2, id3) values(1,1,1);
        insert into tdetl(id, pid1, pid2, pid3) values(200, 1, 1, 1);
        insert into tdetl(id, pid1, pid2, pid3) values(201, 1, 1, 1);
        update tmain set id1 = -id1, id2 = -id2, id3 = -1 where id1 = 1 and id2 = 1 and id3 = 1; -- must PASS and cause update in tdetl
        update tmain set id3 = null where id3 = -1;
        select id as detail_id, pid1 as detail_pid1, pid2 as detail_pid2, pid3 as detail_pid3  from tdetl order by id;
        commit;
        drop table tdetl;
        drop table tmain;

    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], combine_output = True, input = test_sql)

    assert act.clean_stdout == act.clean_expected_stdout
