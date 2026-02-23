#coding:utf-8

"""
ID:          tabloid.dml-privileges-sufficiency
TITLE:       Verify sufficiency of privileges for performing DML actions. Verify that
  RETURNING clause can not be used without SELECT privilege.
DESCRIPTION:
  Test creates three users (for I/U/D) and gives them initial privileges for INSERT, UPDATE and DELETE (but without SELECT).
     Then we check that each user:
     1) can do "his" DML without using RETURNING clause and this action must pass;
     2) can NOT do "his" DML with using RETURNING clause because of absense of SELETC privilege.
     After this we add SELECT privilege for all of them and repeat. All actions must pased in this case.

     Created by request of dimitr, letter 16.06.2020 13:54.
     Checked on 4.0.0.2066.
     ::: NB :::
     Do NOT use this test on 4.0.0.2046 and 4.0.0.2048 - these snapshots have bug and will crash on this test.
FBTEST:      functional.tabloid.dml_privileges_sufficiency
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set wng off;
    set list on;

    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop user tmp$modifier_ins' with autonomous transaction;
            when any do begin end
        end

        begin
        execute statement 'drop user tmp$modifier_upd' with autonomous transaction;
            when any do begin end
        end

        begin
        execute statement 'drop user tmp$modifier_del' with autonomous transaction;
            when any do begin end
        end
    end^
    set term ;^
    commit; --------------- 4.0.0.2046, 4.0.0.2048: crash here

    set bail off;

    create user tmp$modifier_ins password '123';
    create user tmp$modifier_upd password '123';
    create user tmp$modifier_del password '123';
    revoke all on all from tmp$modifier_ins;
    revoke all on all from tmp$modifier_upd;
    revoke all on all from tmp$modifier_del;


    create sequence g;
    recreate table test(id int primary key, x int);
    insert into test(id,x) values( gen_id(g,1), 100);
    commit;

    grant usage on sequence g to tmp$modifier_ins;
    grant usage on sequence g to tmp$modifier_upd;

    grant insert on test to tmp$modifier_ins;
    grant update on test to tmp$modifier_upd;
    grant delete on test to tmp$modifier_del;
    commit;

    create or alter view v_privs as
    select
         rdb$user                        -- tmp$c6317
        ,rdb$relation_name               -- test
        ,rdb$privilege                   -- S
        ,rdb$grant_option                -- 0
        ,rdb$field_name                  -- <null>
        ,rdb$object_type                 -- 0
    from rdb$user_privileges p
    where rdb$user in ( upper('tmp$modifier_ins'), upper('tmp$modifier_upd'), upper('tmp$modifier_del') )
    order by rdb$user, rdb$privilege
    ;
    commit;


    -- #######  P R I V I L E G E    O N L Y   F O R    I N S E R T    ########

    connect '$(DSN)' user tmp$modifier_ins password '123';

    select current_user as whoami, 'Has only INSERT privilege' as msg from rdb$database;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_ins':
    select * from test;

    -- no exception must be here:
    insert into test(id,x) values(gen_id(g,1), 200);
    rollback;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_ins':
    insert into test(id,x) values(gen_id(g,1), 1234567) returning id,x;
    rollback;


    -- must PASS: we have privilege to add record and RETURNING clause
    -- does NOT contain anything from *table*:
    insert into test(id,x) values(gen_id(g,1), 1234567) returning pi();
    rollback;

    -- #######  P R I V I L E G E    O N L Y   F O R    U P D A T E    ########

    connect '$(DSN)' user tmp$modifier_upd password '123';

    select current_user as whoami, 'Has only UPDATE privilege' as msg from rdb$database;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_upd':
    select * from test;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_upd'
    -- NB: "set x = -x" - this statement DOES query to old value of 'x',
    -- i.e. is SELECTS table thust must fail:
    update test set x = -x;
    rollback;

    -- must PASS, no exception must be here: we do NOT interesting on old value of 'x':
    update test set x = 0;
    rollback;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_upd':
    update test set x = 0 order by id rows 1;
    rollback;


    -- must fail because no 'select' privilege was granted to 'tmp$modifier_upd':
    update test set x = 111 where id = 1;
    rollback;

    -- must PASS: we have privilege to change column and RETURNING clause
    -- does NOT contain anything from *table*:
    update test set x = 0 returning pi();
    rollback;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_upd':
    update test set x = 0 returning x;
    rollback;


    -- #######  P R I V I L E G E    O N L Y   F O R    D E L E T E    ########

    connect '$(DSN)' user tmp$modifier_del password '123';

    select current_user as whoami, 'Has only DELETE privilege' as msg from rdb$database;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_del':
    select * from test;

    -- must PASS: user has privilege to DELETE rows from this table:
    delete from test;
    rollback;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_del':
    delete from test where id > 0;
    rollback;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_del':
    delete from test order by id rows 1;
    rollback;

    -- must fail because no 'select' privilege was granted to 'tmp$modifier_del':
    delete from test returning id;
    rollback;


    -- ##############################################################
    -- ###   A D D    P R I V I L E G E    F O R    S E L E C T   ###
    -- ##############################################################
    connect '$(DSN)' user SYSDBA password 'masterkey';

    grant select on test to tmp$modifier_ins;
    grant select on test to tmp$modifier_upd;
    grant select on test to tmp$modifier_del;
    commit;


    -- ####  P R I V I L E G E    F O R    I N S E R T   +   S E L E C T   #####

    connect '$(DSN)' user tmp$modifier_ins password '123';

    select current_user as whoami, 'Has INSERT + SELECT privileges' as msg from rdb$database;

    -- All subsequent statements must PASS:
    select * from test;

    insert into test(id,x) values(-1, 1234567) returning id,x;
    rollback;


    -- ####  P R I V I L E G E    F O R    U P D A T E   +   S E L E C T   #####

    connect '$(DSN)' user tmp$modifier_upd password '123';

    select current_user as whoami, 'Has UPDATE  + SELECT privileges' as msg from rdb$database;

    -- All subsequent statements must PASS:
    select * from test;
    update test set x = -x;
    rollback;
    update test set x = 0;
    rollback;
    update test set x = 0 order by id rows 1;
    rollback;
    update test set x = 111 where id = 1;
    rollback;
    update test set x = 0 returning x;
    rollback;


    -- ####  P R I V I L E G E    F O R     D E L E T E  +   S E L E C T   #####

    connect '$(DSN)' user tmp$modifier_del password '123';

    select current_user as whoami, 'Has DELETE + SELECT privileges' as msg from rdb$database;

    -- All subsequent statements must PASS:
    select * from test;
    delete from test where id > 0;
    rollback;
    delete from test order by id rows 1;
    rollback;
    delete from test returning id;
    rollback;


    -- #########################
    -- ###   C L E A N U P   ###
    -- #########################
    connect '$(DSN)' user SYSDBA password 'masterkey';

    drop user tmp$modifier_ins;
    drop user tmp$modifier_upd;
    drop user tmp$modifier_del;
    commit;
"""

substitutions=[('[ \t]+', ' ')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)


expected_stdout = """
    WHOAMI                          TMP$MODIFIER_INS
    MSG                             Has only INSERT privilege

    PI                              3.141592653589793

    WHOAMI                          TMP$MODIFIER_UPD
    MSG                             Has only UPDATE privilege

    PI                              3.141592653589793

    WHOAMI                          TMP$MODIFIER_DEL
    MSG                             Has only DELETE privilege

    WHOAMI                          TMP$MODIFIER_INS
    MSG                             Has INSERT + SELECT privileges

    ID                              1
    X                               100

    ID                              -1
    X                               1234567

    WHOAMI                          TMP$MODIFIER_UPD
    MSG                             Has UPDATE  + SELECT privileges

    ID                              1
    X                               100

    X                               0

    WHOAMI                          TMP$MODIFIER_DEL
    MSG                             Has DELETE + SELECT privileges

    ID                              1
    X                               100

    ID                              1
"""

expected_stderr = """
    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_INS

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_INS

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_UPD

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_UPD

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_UPD

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_UPD

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_UPD

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_DEL

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_DEL

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_DEL

    Statement failed, SQLSTATE = 28000
    no permission for SELECT access to TABLE TEST
    -Effective user is TMP$MODIFIER_DEL
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)
