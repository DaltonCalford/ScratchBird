#coding:utf-8

"""
ID:          issue-5505
ISSUE:       5505
TITLE:       Authentication end with first plugin that has the user but auth fails; should continue with next plugin
DESCRIPTION:
  We create two users with the same name, 1st using plugin Srp, 2nd - via Legacy.
  Then we try to establish subsequent attachments via ES/EDS for each of them.
  No error should occur.
JIRA:        CORE-5225
FBTEST:      bugs.core_5225
"""

import pytest
from firebird.qa import *

db = db_factory()

user_srp = user_factory('db', name='tmp$c5225', password='srp', plugin='Srp')
user_leg = user_factory('db', name='tmp$c5225', password='leg', plugin='Legacy_UserManager')

test_script = """
    set list on;

    set term ^;
    execute block returns(whoami_leg varchar(31)) as
    begin
       execute statement 'select current_user from rdb$database'
       on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
       as user 'tmp$c5225' password 'leg'
       into whoami_leg;
       suspend;
    end
    ^
    set term ;^
    commit;

    set term ^;
    execute block returns(whoami_srp varchar(31)) as
    begin
       execute statement 'select current_user from rdb$database'
       on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
       as user 'tmp$c5225' password 'srp'
       into whoami_srp;
       suspend;
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
"""

act = isql_act('db', test_script)

expected_stdout = """
    WHOAMI_LEG                      TMP$C5225
    WHOAMI_SRP                      TMP$C5225
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3.0.1')
def test_1(act: Action, user_srp: User, user_leg: User):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

