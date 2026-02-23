#coding:utf-8

"""
ID:          issue-6886
ISSUE:       6886
TITLE:       Differerent interfaces behaviour depending upon source of interface
DESCRIPTION:
  Appearance of this bug was received accidentally during implemenation of test for completely unrelated issue (gh-6935).
  First message about presence of query to RDB$AUTH_MAPPING in MON$STATEMENTS was sent to Alex, see letter 02-sep-2021 07:19.
  Only FB 4.0.1 Classic was affected. There was no problem with FB 3.x and 5.x.

  After some additional discussion it was concluded that apropriate script can serve as test for:
    https://github.com/FirebirdSQL/firebird/commit/11712f82039cb0318140593f433019297ae07407
    Branch: refs/heads/v4.0-release, 15.09.2021 17:41:
    "Fixed #6886: Differerent interfaces behaviour depending upon source of interface"

  Confirmed bug on 4.0.1.2592 CS (15.09.2021): got 1 record from mon$statements with following content:
    ATT_ID                          3
    ATT_USER                        SYSDBA
    SQL_TEXT_BLOB_ID                0:1
    SELECT RDB$MAP_USING, RDB$MAP_PLUGIN, RDB$MAP_DB, RDB$MAP_FROM_TYPE,  RDB$MAP_FROM, RDB$MAP_TO_TYPE, RDB$MAP_TO FROM RDB$AUTH_MAPPING
    STTM_STATE                      0
    Records affected: 1

  Checked 4.0.1.2602, 5.0.0.215 -- all OK, no records foudn in mon$statements.
FBTEST:      bugs.gh_6886
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;

    commit;
    connect '$(DSN)' user sysdba password 'masterkey';

    set term ^;
    execute block returns(
         att_id type of column mon$attachments.mon$attachment_id
        ,att_user type of column mon$attachments.mon$user
        ,sql_text_blob_id type of column mon$statements.mon$sql_text
        ,sttm_state type of column mon$statements.mon$state
    ) as
    begin
        for
            execute statement (
                q'{
                    select a.mon$attachment_id,a.mon$user, s.mon$sql_text, s.mon$state
                    from mon$attachments a
                    join mon$statements s using(mon$attachment_id)
                    where
                        s.mon$attachment_id <> current_connection
                        and s.mon$sql_text not containing 'execute block'
                }'
            ) on external
                'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
                as user 'SYSDBA' password 'masterkey' role left('R' || replace(uuid_to_char(gen_uuid()),'-',''),31)
        into
            att_id, att_user, sql_text_blob_id, sttm_state
        do
            suspend;
    end
    ^
    set term ;^
    commit;

    ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL; -- !! mandatory otherwise database file will be kept by engine and fbtest will not able to drop it !!
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.es_eds
@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
