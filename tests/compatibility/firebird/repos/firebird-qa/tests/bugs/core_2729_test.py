#coding:utf-8

"""
ID:          issue-3124
ISSUE:       3124
TITLE:       Current connection may be used by EXECUTE STATEMENT instead of creation of new attachment
DESCRIPTION:
JIRA:        CORE-2729
FBTEST:      bugs.core_2729
"""

import pytest
from firebird.qa import *

db_1 = db_factory()

test_script = """
    -- Refactored 05-JAN-2016: removed dependency on recource 'test_user' because this lead to:
    -- UNTESTED: bugs.core_2729
    -- Add new user
    -- Unexpected stderr stream received from GSEC.
    -- (i.e. test remained in state "Untested" because of internal error in gsec while creating user 'test' from resource).
    -- Checked on WI-V2.5.5.26952 (SC), WI-V3.0.0.32266 (SS/SC/CS).

    set wng off;
    -- Drop old account if it remains from prevoius run:
    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop user tmp$c2729a' with autonomous transaction;
            when any do begin end
        end
        begin
            execute statement 'drop user tmp$c2729b' with autonomous transaction;
            when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create user tmp$c2729a password 'QweRtyUi';
    create user tmp$c2729b password 'AsdFghJk';
    commit;

    connect '$(DSN)' user 'TMP$C2729A' password 'QweRtyUi';

    -- Updated code: made STDOUT insensible to the IDs of attaches
    set list on;
    set term ^;
    execute block returns (
        user_on_start varchar(32),
        user_on_extds varchar(32),
        same_attach char(1)
    ) as
        declare v_att_ini int;
        declare v_att_eds int;
    begin

        user_on_start = current_user;
        v_att_ini = current_connection;

        execute statement 'select current_user, current_connection from rdb$database'
        into  :user_on_extds, :v_att_eds;


        same_attach = iif( v_att_ini = v_att_eds, 'Y', 'N');
        suspend;

        execute statement 'select current_user, current_connection from rdb$database'
                          as user 'TMP$C2729B' password 'AsdFghJk'
        into :user_on_extds, :v_att_eds;

        same_attach = iif( v_att_ini = v_att_eds, 'Y', 'N');
        suspend;

    end
    ^
    set term ;^
    commit;

    connect '$(DSN)' user 'SYSDBA' password 'masterkey';

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

    drop user tmp$c2729a;
    drop user tmp$c2729b;
    commit;
"""

act = isql_act('db_1', test_script)

expected_stdout = """
    USER_ON_START                   TMP$C2729A
    USER_ON_EXTDS                   TMP$C2729A
    SAME_ATTACH                     Y

    USER_ON_START                   TMP$C2729A
    USER_ON_EXTDS                   TMP$C2729B
    SAME_ATTACH                     N
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

