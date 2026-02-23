#coding:utf-8

"""
ID:          n/a
TITLE:       Cyclic references between PK and FK (declared with 'ON ... CASCADE') must not cause infinite loop when records are deleted or updated in any of involved tables.
DESCRIPTION:
    Test verifies RI mechanism when ON DELETE/UPDATE CASCADE option is used.
    Three tables are created which have PK and FK, and refer to each other as follows:
        T1 (id, fk3) <-- T2 (id, fk1) // "t1.id <-- t2.fk1"
        T2 (id, fk1) <-- T3 (id, fk2) // "t2.id <-- t3.fk2"
        T3 (id, fk2) <-- T1 (id, fk3) // "t3.id <-- t1.fk3"
    One record with values = (1,1) is added to each table.

    Deletion of this record from t1 (or any other table) must finish w/o errors and all table must become empty.
    
    But "update t1 set id = id + 1;" will cause infinite loop and engine has to stop it.
    One may see 'progress' of this loop by querying value of sequence 'g'.
    ::: ACHTUNG :::
    Only FB 3.x detects this problem when GEN_ID = 1003 (i.e. after iteration N 1000) and raises:
        Statement failed, SQLSTATE = 54001
        Too many concurrent executions of the same request
        -At trigger 'CHECK_3'
        At trigger 'CHECK_2'
        At trigger 'CHECK_1'
        At trigger 'CHECK_3'
        ...
    Other major versions (FB 4.x ... 6.x) will *hang*, without any CPU usage.
    Interrupting of ISQL using Ctrl-Break will not release DB, one need to restart FB.
    Values of GEN_ID will be different:
        4.0.6.3213: Generator G, current value: 2859
        5.0.3.1666: Generator G, current value: 2334
        6.0.0.838:  Generator PUBLIC.G, current value: 2335
    Because of this, it was decided to limit number of iterations for update, see 'LIMIT_ITERATIONS_FOR_UPDATE'
    (discussed with Vlad, letter 21.06.2025 15:01, subj: "#8598: how to check it ?")

    Only single segmented UK/FK are checked.
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

# ::: ACHTUNG :::
# Only FB3.x can detect infinite loop and interrupt it with raising
# "SQLSTATE = 54001 / Too many concurrent executions of the same request"
# FB 4.x ... 6.x will hang on update after ~2330 iterations.
# We have to limit number of iterations!
##################################
LIMIT_ITERATIONS_FOR_UPDATE = 2000
##################################

expected_stdout = """
    Records affected: 1
    Records affected: 0
    Records affected: 0
    Records affected: 0

    Records affected: 1

    ID 2003
    FK3 2005
    Records affected: 1
    
    ID 2004
    FK1 2003
    Records affected: 1
    
    ID 2005
    FK2 2004
    Records affected: 1

    GEN_ID  2003    
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
   
    test_sql = f"""
        -- #################
        -- ON DELETE CASCADE
        -- #################
        set bail off;
        set list on;
        create table t1 (id int unique, fk3 int default null);
        create table t2 (id int unique, fk1 int references t1 (id) on delete cascade);
        create table t3 (id int unique, fk2 int references t2 (id) on delete cascade);
        commit;

        insert into t1(id, fk3) values(1, 1);
        insert into t2(id, fk1) values(1, 1);
        insert into t3(id, fk2) values(1, 1);
        commit;

        alter table t1 add constraint t1_fk foreign key(fk3) references t3(id) on delete cascade;
        commit;

        set bail off;

        set list on;
        set count on;

        delete from t1;
        select * from t1;
        select * from t2;
        select * from t3;
        set count off;
        commit;
        alter table t1 drop constraint t1_fk;
        drop table t3;
        drop table t2;
        drop table t1;

        --------------------------------------------
        -- #################
        -- ON UPDATE CASCADE
        -- #################
        set bail on;
        create sequence g;
        create table t1 (id int unique, fk3 int default null);
        create table t2 (id int unique, fk1 int references t1 (id) on update cascade);
        create table t3 (id int unique, fk2 int references t2 (id) on update cascade);
        commit;

        insert into t1(id, fk3) values(1, 1);
        insert into t2(id, fk1) values(1, 1);
        insert into t3(id, fk2) values(1, 1);
        commit;

        alter table t1 add constraint t1_fk foreign key(fk3) references t3(id) on update cascade;
        set term ^;
        create trigger t1_bu for t1 before update as
        begin
            if (gen_id(g,0) > {LIMIT_ITERATIONS_FOR_UPDATE}) then
                exit;
            if (old.fk3 <> new.fk3) then
                new.id = new.fk3 + 1 + 0*gen_id(g,1);
        end
        ^
        create trigger t2_bu for t2 before update as
        begin
            new.id = new.fk1 + 1 + 0*gen_id(g,1);
        end
        ^
        create trigger t3_bu for t3 before update as
        begin
            new.id = new.fk2 + 1 + 0*gen_id(g,1);
        end
        ^
        set term ;^
        commit;
        set bail off;
        set count on;
        update t1 set id = id + 1;
        select * from t1;
        select * from t2;
        select * from t3;
        set count off;
        select gen_id(g,0) from rdb$database;
        commit;
        alter table t1 drop constraint t1_fk;
        drop table t3;
        drop table t2;
        drop table t1;
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], combine_output = True, input = test_sql)

    assert act.clean_stdout == act.clean_expected_stdout
                                         	