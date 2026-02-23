#coding:utf-8

"""
ID:          issue-1192
ISSUE:       1192
TITLE:       Privileges of dynamic statements in SP
DESCRIPTION:
JIRA:        CORE-805
FBTEST:      bugs.core_0805
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set wng off;
    -- Drop old account if it remains from prevoius run:
    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop user tmp$c0805_senior' with autonomous transaction;
            when any do begin end
        end
        begin
            execute statement 'drop user tmp$c0805_junior' with autonomous transaction;
            when any do begin end
        end
        begin
            execute statement 'drop role tmp$r4junior';
            when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create user tmp$c0805_senior password 'qwe';
    create user tmp$c0805_junior password '123';
    commit;

    create role tmp$r4junior;
    commit;

    create or alter procedure sp_test as begin  end;

    recreate table test(id int, x int);
    commit;

    insert into test values(1, 100);
    commit;

    set term ^;
    create or alter procedure sp_test returns(id int, x int) as
    begin
      for select id, x from test into id, x do suspend;
    end
    ^

    create or alter procedure sp_main(a_usr varchar(31), a_pwd varchar(31), a_role varchar(31) = 'NONE')
      returns(who_am_i varchar(31), what_is_my_role varchar(31), id int, x int) as
    begin
      for
          execute statement
          'select current_user, current_role, id, x from sp_test'
          WITH CALLER PRIVILEGES
          as user a_usr password a_pwd role a_role
          into who_am_i, what_is_my_role, id, x
      do
          suspend;
    end
    ^
    set term ;^
    commit;

    revoke all on all from tmp$c0805_senior;
    revoke all on all from tmp$c0805_junior;
    revoke all on all from tmp$r4junior; --restored as uncommented statement, 05.03.2018
    commit;

    grant select on test to procedure sp_test;
    grant execute on procedure sp_main to tmp$c0805_senior;
    grant execute on procedure sp_main to tmp$r4junior;
    grant execute on procedure sp_test to procedure sp_main;
    grant tmp$r4junior to tmp$c0805_junior;
    commit;

    -- result:
    -- 1) table TEST can be queried only by procedure sp_test;
    -- 2) procedure sp_test is called only from another procedure - sp_main - and only via ES
    -- 3) procedure sp_main can be called:
    -- 2.1) directly by user tmp$c0805_senior (no role required to him);
    -- 2.2) indirectly by ROLE 'tmp$r4junior' which is granted to user 'tmp$c0805_junior'.

    -- Both these users should be able to see data of table 'TEST':

    set list on;

    set term ^;
    execute block returns(who_am_i varchar(31), what_is_my_role varchar(31), id int, x int) as
    begin
      for execute statement ('select * from sp_main( :u, :p)') ( u := 'tmp$c0805_senior', p := 'qwe' )
      into who_am_i, what_is_my_role, id, x
      do suspend;
    end
    ^
    execute block returns(who_am_i varchar(31), what_is_my_role varchar(31), id int, x int) as
    begin
      for execute statement ('select * from sp_main( :u, :p, :r)') ( u := 'tmp$c0805_junior', p := '123', r := 'tmp$r4junior' )
      into who_am_i, what_is_my_role, id, x
      do suspend;
    end
    ^
    set term ;^
    commit;
    --                                    ||||||||||||||||||||||||||||
    -- ###################################|||  FB 4.0+, SS and SC  |||##############################
    --                                    ||||||||||||||||||||||||||||
    -- If we check SS or SC and ExtConnPoolLifeTime > 0 (config parameter FB 4.0+) then current
    -- DB (bugs.core_NNNN.fdb) will be 'captured' by firebird.exe process and fbt_run utility
    -- will not able to drop this database at the final point of test.
    -- Moreover, DB file will be hold until all activity in firebird.exe completed and AFTER this
    -- we have to wait for <ExtConnPoolLifeTime> seconds after it (discussion and small test see
    -- in the letter to hvlad and dimitr 13.10.2019 11:10).
    -- This means that one need to kill all connections to prevent from exception on cleanup phase:
    -- SQLCODE: -901 / lock time-out on wait transaction / object <this_test_DB> is in use
    -- #############################################################################################
    delete from mon$attachments where mon$attachment_id != current_connection;
    commit;

    drop user tmp$c0805_senior;
    drop user tmp$c0805_junior;
    drop role tmp$r4junior;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    WHO_AM_I                        TMP$C0805_SENIOR
    WHAT_IS_MY_ROLE                 NONE
    ID                              1
    X                               100

    WHO_AM_I                        TMP$C0805_JUNIOR
    WHAT_IS_MY_ROLE                 TMP$R4JUNIOR
    ID                              1
    X                               100
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

