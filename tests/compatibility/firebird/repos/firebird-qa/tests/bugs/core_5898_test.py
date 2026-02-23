#coding:utf-8

"""
ID:          issue-6156
ISSUE:       6156
TITLE:       ROLE not passed in EXECUTE STATEMENT ... ON EXTERNAL
DESCRIPTION:
JIRA:        CORE-5898
FBTEST:      bugs.core_5898
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='tmp$c5898', password='123')
tmp_role = role_factory('db', name='boss')

test_script = """
    set bail on;
    set list on;

    grant boss to tmp$c5898;
    commit;

    connect '$(DSN)' user 'tmp$c5898' password '123' role 'BOSS';
    select
        'BEFORE CHECKS:' as msg,
        mon$user as who_am_i,
        mon$role as whats_my_role,
        left(mon$remote_protocol,3) what_protocol_im_using
    from mon$attachments a
    where mon$attachment_id=current_connection;

    set echo off;
    set term ^;
    execute block
      returns ( check_no smallint, who_am_i varchar(32), whats_my_role varchar(32), what_protocol_im_using varchar(32))
    as
      declare v_dbnm varchar(255);
      declare v_sttm varchar(255);
    begin
      v_dbnm = rdb$get_context('SYSTEM', 'DB_NAME');
      v_sttm = 'select mon$user,mon$role, left(mon$remote_protocol,3) from mon$attachments a where mon$attachment_id=current_connection';

      check_no=1;
      execute statement v_sttm
          into who_am_i, whats_my_role, what_protocol_im_using;
      suspend;


      check_no=check_no+1;
      execute statement v_sttm
          on external :v_dbnm
          into who_am_i, whats_my_role, what_protocol_im_using;
      suspend;

    end
    ^
    set term ;^
    rollback;

    -- cleanup:
    -- ########
    connect '$(DSN)' user 'sysdba' password 'masterkey';

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
    MSG                             BEFORE CHECKS:
    WHO_AM_I                        TMP$C5898
    WHATS_MY_ROLE                   BOSS
    WHAT_PROTOCOL_IM_USING          TCP

    CHECK_NO                        1
    WHO_AM_I                        TMP$C5898
    WHATS_MY_ROLE                   BOSS
    WHAT_PROTOCOL_IM_USING          TCP

    CHECK_NO                        2
    WHO_AM_I                        TMP$C5898
    WHATS_MY_ROLE                   BOSS
    WHAT_PROTOCOL_IM_USING          <null>
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3.0.4')
def test_1(act: Action, tmp_user, tmp_role):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
