#coding:utf-8

"""
ID:          issue-7217
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7217
TITLE:       user with DROP ANY PACKAGE privilege can not execute DROP PACKAGE BODY
NOTES:
    [25.02.2023] pzotov
    Confirmed bug on 5.0.0.520.
    Checked on 5.0.0.959, 4.0.3.2903 - all OK.
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create package pg_test_1 as
    begin
          procedure pg_sp_1 returns(id int);
    end
    ^
    create package pg_test_2 as
    begin
          procedure pg_sp_2 returns(id int);
    end
    ^
    create package body pg_test_1 as
    begin
      	procedure pg_sp_1 returns(id int) as
      	begin
      	    id = 1;
      	    suspend;
      	end
    end
    ^
    create package body pg_test_2 as
    begin
      	procedure pg_sp_2 returns(id int) as
      	begin
      	    id = 1;
      	    suspend;
      	end
    end
    ^
    set term ;^
    commit;
"""
db = db_factory(init = init_script)

tmp_usr1 = user_factory('db', name = 'tmp_user_7217_foo', password = '123')
tmp_usr2 = user_factory('db', name = 'tmp_user_7217_bar', password = '456')
tmp_role = role_factory('db', name = 'tmp_role_7217')

act = python_act('db')

expected_stdout = """
    RDB$PACKAGE_BODY_SOURCE         <null>
    RDB$PACKAGE_BODY_SOURCE         <null>
    Records affected: 2
"""

@pytest.mark.version('>=4.0.2')
def test_1(act: Action, tmp_usr1: User, tmp_usr2: User, tmp_role: Role):

    test_script = f"""
        set bail on;

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
        grant DROP ANY PACKAGE to {tmp_usr1.name};
        grant DROP ANY PACKAGE to {tmp_role.name};
        grant {tmp_role.name} to {tmp_usr2.name};
        commit;
        set autoddl off;
        set bail off;
        -----------------------------------------------------
        -- 1. Check that user can drop package when this permission was granted to HIMSELF:
        connect '{act.db.dsn}' user {tmp_usr1.name} password '{tmp_usr1.password}';
        drop package body pg_test_1;
        rollback;

        set term ^;
        execute block as
        begin
            execute statement 'drop package body pg_test_1';
        end
        ^
        set term ;^
        commit;
        -----------------------------------------------------
        -- 2. Check that user can drop package when this permission was granted to ROLE which, in turn, was granted to that user:
        connect '{act.db.dsn}' user {tmp_usr2.name} password '{tmp_usr2.password}' role {tmp_role.name};
        drop package body pg_test_2;
        rollback;

        set term ^;
        execute block as
        begin
            execute statement 'drop package body pg_test_2';
        end
        ^
        set term ;^
        commit;
        -----------------------------------------------------
        -- 3. Check that both packages now have no bodies:
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
        set count on;
        set list on;
        set blob all;
        -- must issue two rows, both with <null> literals:
        select p.rdb$package_body_source from rdb$packages p where p.rdb$package_name in ( upper('pg_test_1'),upper('pg_test_2') );
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_script, connect_db = False, credentials = False, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
