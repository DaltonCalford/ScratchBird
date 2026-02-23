#coding:utf-8

"""
ID:          issue-5052
ISSUE:       5052
TITLE:       Error "invalid BLOB ID" can occur when retrieving MON$STATEMENTS.MON$SQL_TEXT using ES/EDS and db_connect argument is not specified
DESCRIPTION:
JIRA:        CORE-4747
FBTEST:      bugs.core_4747
NOTES:
    [01.12.2023] pzotov
    1. Comments between statements are now handled by ISQL as *part* of some statement <S> that follow after.
       This causes these comment be reflected in mon$sql_statement.mon$sql_text if we want to search <S>.
       Because of this, it was decided to take out such comment from executable code (see 'test_sql' content:
       previously it contained "21.05.2017: 4.0 Classic now has..." - but now this comment removed from there).

       This (new) behaviour of ISQL was introduced after implementation of PR #7868
       See example with 'strange appearance of comments': https://github.com/FirebirdSQL/firebird/pull/7868#issuecomment-1826727278

       Currently this is not considered as a bug, see note by Adriano: https://groups.google.com/g/firebird-devel/c/AM8vlA3YJws
    2. QA does not allow to check too ancient FB version which is mentioned in the ticket (3.0.0 Beta 1, date: 12-apr-2015).

    Checked on 6.0.0.157, 5.0.0.1284, 4.0.5.3033
"""

import pytest
from firebird.qa import *

db = db_factory()

substitutions=[ ('RUNNING_STT_ID[ ]+[0-9]+', 'RUNNING_STT_ID'),
                ('RUNNING_TRN_ID[ ]+[0-9]+', 'RUNNING_TRN_ID'),
                ('RUNNING_STT_TEXT.*', '')
              ]

act = python_act('db', substitutions = substitutions)

@pytest.mark.es_eds
@pytest.mark.version('>=3')
def test_1(act: Action):

    TEST_DML = "insert into test(sid, tid, txt) select s.mon$statement_id, s.mon$transaction_id, s.mon$sql_text from mon$statements s where s.mon$sql_text containing 'test' rows 1"

    # 21.05.2017: 4.0 Classic now has record in mon$statements with data from RDB$AUTH_MAPPING table.
    # We have to prevent appearance of this row in resultset which is to be analyzed, thus adding clause:
    # "where s.mon$sql_text containing 'test' ..."
    #
    TEST_QRY = "select s.mon$statement_id, s.mon$transaction_id, s.mon$sql_text from mon$statements s where s.mon$sql_text containing 'test' and s.mon$transaction_id = current_transaction rows 1"

    test_sql = f"""
        set list on;
        set blob all;

        recreate table test(sid int, tid int, txt blob);
        commit;

        {TEST_DML};
        commit;

        set term ^;
        execute block returns( msg varchar(10), running_stt_id int, running_trn_id int, running_stt_text blob) as
            declare v_dbname varchar(255);
            declare v_stt1 varchar(1024) = 'select t.sid, t.tid, t.txt from test t';
            declare v_stt2 varchar(1024) = q'!{TEST_QRY}!';
            declare v_usr rdb$user = '{act.db.user}';
            declare v_pwd varchar(20) = '{act.db.password}';
            declare v_trn int;
        begin
            -- NOTE: v_dbname is NOT initialized with database connection string.

            msg = 'point-1';
            execute statement (v_stt1)
            on external (v_dbname)
            as user :v_usr password :v_pwd
            into running_stt_id, running_trn_id, running_stt_text;
            suspend;

            msg = 'point-2';
            execute statement (v_stt2)
            on external (v_dbname)
            as user :v_usr password :v_pwd
            into running_stt_id, running_trn_id, running_stt_text ;
            suspend;
        end
        ^
        set term ;^
        commit;

    """

    expected_stdout = f"""
        MSG                             point-1
        RUNNING_STT_ID                  111
        RUNNING_TRN_ID                  219
        RUNNING_STT_TEXT                91:0
        {TEST_DML}

        MSG                             point-2
        RUNNING_STT_ID                  140
        RUNNING_TRN_ID                  224
        RUNNING_STT_TEXT                0:1
        {TEST_QRY}
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

