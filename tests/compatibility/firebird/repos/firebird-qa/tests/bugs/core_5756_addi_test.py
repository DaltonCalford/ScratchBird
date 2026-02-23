#coding:utf-8

"""
ID:          issue-6019-B
ISSUE:       6019
TITLE:       Regression: FB crashes when trying to recreate table that is in use by DML (3.0.3; 3.0.4; 4.0.0)
DESCRIPTION:
  A bug was detected during implementation of test for CORE-5754: FB crashed with currpupting database
  after we run test script two times (letter to dimitr et al 20-feb-18 13:52). Moreover, gfix -v -full
  did not produce any messages about database state, so test DB should be restored from copy.
  Decided to change name of this test after getting reply from Roman Simakov (21-feb-2018 12:19)
  that this bug was already fixed by dimitr in:
  https://github.com/FirebirdSQL/firebird/commit/9afef198c17368276ccd7a428e159c1ca5684a60
  (Postfix for CORE-2284/CORE-5677, fixes regression CORE-5756)
NOTES:
  Script for test is launched here TWO times ("pass I", "pass II").
JIRA:        CORE-5756
FBTEST:      bugs.core_5756_addi
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    commit;

    ------------------------------------------------- PASS I --------------------------------

    connect '$(DSN)' user sysdba password 'masterkey';

    create or alter user tmp$c5754 password '123';
    commit;

    create or alter trigger ddl_log_afte inactive after any ddl statement as begin end;
    commit;
    drop trigger ddl_log_afte;
    commit;

    connect '$(DSN)' user sysdba password 'masterkey';

    recreate table ddl_log(id int);
    recreate table test(id int);
    recreate sequence g;
    commit;

    recreate table ddl_log (
        evn_type varchar(50)
        ,obj_type varchar(50)
        ,obj_name varchar(50)
        ,sql_text blob sub_type text
        ,last_ddl_author varchar(50) default current_user
    );
    commit;

    -- DDL operations for managing triggers and indices re-use table privileges.
    grant alter any table to tmp$c5754;
    commit;

    set term ^;

    create or alter trigger test_bi for test active before insert position 0 as
    begin
      new.id = coalesce(new.id, gen_id(g, 1) );
    end
    ^

    create or alter trigger ddl_log_afte active after any ddl statement as
    begin
      in autonomous transaction do
      insert into ddl_log(
        evn_type
        ,obj_type
        ,obj_name
        ,sql_text
      )
      values (
        rdb$get_context('DDL_TRIGGER', 'EVENT_TYPE')
        ,rdb$get_context('DDL_TRIGGER', 'OBJECT_TYPE')
        ,rdb$get_context('DDL_TRIGGER', 'OBJECT_NAME')
        ,rdb$get_context('DDL_TRIGGER', 'SQL_TEXT')
      );

    end
    ^

    set term ;^
    commit;

    connect '$(DSN)' user tmp$c5754 password '123';

    set term  ^;
    alter trigger test_bi as
    begin
      -- this trigger was updated by tmp$c5754
      if ( new.id is null ) then
          new.id = gen_id(g, 1);
    end
    ^
    set term ;^
    commit;

    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c5754;
    commit;

    set list on;
    set count on;
    --set echo on;
    select last_ddl_author
    from ddl_log
    where
        evn_type = upper('alter')
        and obj_type = upper('trigger')
        and sql_text containing 'tmp$c5754'
    ;
    rollback;

    ------------------------------------------------- PASS II --------------------------------

    connect '$(DSN)' user sysdba password 'masterkey';

    create or alter user tmp$c5754 password '123';
    commit;

    create or alter trigger ddl_log_afte inactive after any ddl statement as begin end;
    commit;
    drop trigger ddl_log_afte;
    commit;

    connect '$(DSN)' user sysdba password 'masterkey';

    recreate table ddl_log(id int);
    recreate table test(id int);
    recreate sequence g;
    commit;

    recreate table ddl_log (
        evn_type varchar(50)
        ,obj_type varchar(50)
        ,obj_name varchar(50)
        ,sql_text blob sub_type text
        ,last_ddl_author varchar(50) default current_user
    );
    commit;

    -- DDL operations for managing triggers and indices re-use table privileges.
    grant alter any table to tmp$c5754;
    commit;

    set term ^;

    create or alter trigger test_bi for test active before insert position 0 as
    begin
      new.id = coalesce(new.id, gen_id(g, 1) );
    end
    ^

    create or alter trigger ddl_log_afte active after any ddl statement as
    begin
      in autonomous transaction do
      insert into ddl_log(
        evn_type
        ,obj_type
        ,obj_name
        ,sql_text
      )
      values (
        rdb$get_context('DDL_TRIGGER', 'EVENT_TYPE')
        ,rdb$get_context('DDL_TRIGGER', 'OBJECT_TYPE')
        ,rdb$get_context('DDL_TRIGGER', 'OBJECT_NAME')
        ,rdb$get_context('DDL_TRIGGER', 'SQL_TEXT')
      );

    end
    ^

    set term ;^
    commit;

    connect '$(DSN)' user tmp$c5754 password '123';

    set term ^;
    alter trigger test_bi as
    begin
      -- this trigger was updated by tmp$c5754
      if ( new.id is null ) then
          new.id = gen_id(g, 1);
    end
    ^
    set term ;^
    commit;

    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c5754;
    commit;

    set list on;
    set count on;

    select last_ddl_author
    from ddl_log
    where
        evn_type = upper('alter')
        and obj_type = upper('trigger')
        and sql_text containing 'tmp$c5754'
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    LAST_DDL_AUTHOR                 TMP$C5754
    Records affected: 1
    LAST_DDL_AUTHOR                 TMP$C5754
    Records affected: 1
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
