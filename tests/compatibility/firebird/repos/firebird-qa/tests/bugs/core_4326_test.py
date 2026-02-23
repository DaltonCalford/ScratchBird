#coding:utf-8

"""
ID:          issue-4649
ISSUE:       4649
TITLE:       Keyword SET should not be required in ALTER USER statement
DESCRIPTION:
JIRA:        CORE-4326
FBTEST:      bugs.core_4326
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='tmp$c4326', password='123')

test_script = """
    alter user tmp$c4326 password '456' firstname 'Deep' lastname 'Purple';
    commit;

    create or alter view v_user_info as
    select sec$user_name uname, sec$first_name fname, sec$last_name lname from sec$users
    where sec$user_name = upper('tmp$c4326');
    commit;

    set list on;

    select * from v_user_info;

    commit;
    set transaction read committed;
    set term ^;
    execute block returns(
      uname type of column sec$users.sec$user_name
      ,fname type of column sec$users.sec$first_name
      ,lname type of column sec$users.sec$last_name
    )  as
      declare v_usr type of column sec$users.sec$user_name = 'tmp$c4326';
      declare v_pwd varchar(20) = '456';

    begin
      execute statement 'alter current user password ''IronMan'' firstname ''Black'' lastname ''Sabbath'''
      with autonomous transaction
      on external ( 'localhost:' || rdb$get_context('SYSTEM','DB_NAME') )
         as user v_usr password v_pwd;

      for select uname, fname, lname from v_user_info into uname, fname, lname do suspend;

      execute statement 'alter current user password ''Kashmir'' firstname ''Led'' lastname ''Zeppelin'''
      on external ( 'localhost:' || rdb$get_context('SYSTEM','DB_NAME') )
         as user v_usr password 'IronMan';

    end
    ^
    set term ;^
    commit;

    select * from v_user_info;
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
"""

act = isql_act('db', test_script)

expected_stdout = """
    UNAME                           TMP$C4326
    FNAME                           Deep
    LNAME                           Purple

    UNAME                           TMP$C4326
    FNAME                           Black
    LNAME                           Sabbath

    UNAME                           TMP$C4326
    FNAME                           Led
    LNAME                           Zeppelin
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

