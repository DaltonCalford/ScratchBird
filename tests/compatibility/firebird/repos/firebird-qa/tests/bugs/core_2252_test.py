#coding:utf-8

"""
ID:          issue-2678
ISSUE:       2678
TITLE:       EXECUTE STATEMENT on EXTERNAL SOURCE does not check the status of the transaction
DESCRIPTION:
JIRA:        CORE-2252
FBTEST:      bugs.core_2252
"""

import pytest
from firebird.qa import *

substitutions = [('Data source : Firebird.*', 'Data source : Firebird'),
                 ('[-]{0,1}At block line: [\\d]+, col: [\\d]+', 'At block line')]

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns (tran_id integer) as
    begin

        execute statement 'select sign(current_transaction) from rdb$database'
        on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
        as user 'sysdba' password 'masterkey'
        into :tran_id;

        suspend;

        execute statement 'commit'
        on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
        as user 'sysdba' password 'masterkey';

        execute statement 'select sign(current_transaction) from rdb$database'
        on external 'localhost:'  || rdb$get_context('SYSTEM','DB_NAME')
        as user 'sysdba' password 'masterkey'
        into :tran_id;

        suspend;
    end
    ^
    set term ;^
    rollback;

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

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """
    TRAN_ID                         1
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Execute statement error at isc_dsql_prepare :
    335544986 : Explicit transaction control is not allowed
    Statement : commit
    Data source : Firebird::localhost:C:\\FBTESTING\\QA\\FBT-REPO\\TMP\\BUGS.CORE_2252.FDB
    -At block line: 11, col: 9
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

