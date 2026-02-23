#coding:utf-8

"""
ID:          issue-4919
ISSUE:       4919
TITLE:       EXECUTE STATEMENT rise varchar char_length() size
DESCRIPTION:
JIRA:        CORE-4604
FBTEST:      bugs.core_4604
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns (str_size integer)
    as
        declare variable tmp_guid varchar(38);
        declare variable tmp_sqltext varchar(300);
        declare v_dbname type of column mon$database.mon$database_name;
        declare v_usr varchar(31) = 'SYSDBA';
        declare v_pwd varchar(31) = 'masterkey';
    begin

        v_dbname = 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME');

        ---------------------------------------------------------------

        tmp_guid = uuid_to_char(gen_uuid());

        str_size = char_length(tmp_guid);
        suspend;

        ---------------------------------------------------------------

        tmp_sqltext = 'select ''' || tmp_guid || ''' from rdb$database';

        execute statement (:TMP_SQLTEXT)
        into :tmp_guid;

        str_size = char_length(tmp_guid);
        suspend;

        ---------------------------------------------------------------

        execute statement (:TMP_SQLTEXT)
        on external v_dbname as user v_usr password v_pwd
        into :tmp_guid;

        str_size = char_length(tmp_guid);

        --  Confirmed on WI-T3.0.0.31374 Firebird 3.0 Beta 1/tcp (csprog)/P13
        -- STR_SIZE                        36
        -- STR_SIZE                        36
        -- STR_SIZE                        144

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
    STR_SIZE                        36
    STR_SIZE                        36
    STR_SIZE                        36
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

