#coding:utf-8

"""
ID:          n/a
ISSUE:       8649
TITLE:       AV when ON CONNECT triggers uses EXECUTE STATEMENT ON EXTERNAL
DESCRIPTION:
    Auxiliary database is created here ('db2') in order to make ES/EDS.
NOTES:
    [26.07.2025] pzotov
    1. By default, any test database is created with flag 'do_not_drop = False' which means that such DB
       will be dropped at teardown phase of pytest.
       Appropriate method of Database class does following:
           1.   Changes test DB linger to 0 (using services API);
           2.   Establishes new common connection (i.e. no_db_triggers = False) which:
           2.1.   Issues 'delete from mon$attachments', with suppressing any exceptions
           2.2.   Calls drop_database(), also with suppressing any exceptions
           3.   If DB file still exists - calls self.db_path.unlink(missing_ok=True)
       This means that on step "2." DB-level triggers will fire, even if they are invalid or cause problems.
       We *ourselves* have to drop such triggers, BEFORE teardown - see 'con_kill_db_level_trigger'.
    2. Name of trigger must be adjusted on FB 6.x because of SQL schemas introduction since 6.0.0.834
    3. One need to suppress message test of error, it can differ on Windows vs Linux in case if 'Win_sspi'
       presents in AuthClient parameter. In that case on Windows error text is "335545060 : Missing security context"
       vs Linux: "335544472 : Your user name and password are not defined". Thanks to Alex for explanation.

    Confirmed bug (crash) on 6.0.0.949; 5.0.3.1668; 4.0.6.3214.
    Checked on 6.0.0.967; 5.0.3.1683; 4.0.6.3221
"""
import os
import locale
from pathlib import Path
import pytest
from firebird.qa import *

db = db_factory()
db2 = db_factory(filename = 'tmp_gh_8649_aux.fdb')

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

substitutions = [  ('^((?!(SQLSTATE|attach|source|trigger)).)*$', '')
                  ,('Data source : Firebird::.*', 'Data source : Firebird::')
                  ,(r'line(:)?\s+\d+.*', '')
                ]

act = python_act('db', substitutions = substitutions)
act2 = python_act('db2')

@pytest.mark.version('>=4.0.6')
def test_1(act: Action, act2: Action):

    test_script = f"""
        set term ^;
        create trigger trg_connect on connect as
            declare id int;
        begin
            execute statement 'select 1 from rdb$database'
            on external '{act2.db.dsn}'
        	into :id;
        end
        ^
        set term ;^
        commit;
        connect '{act.db.dsn}';
    """

    TEST_TRIGGER_NAME = "'TRG_CONNECT'" if act.is_version('<6') else '"PUBLIC"."TRG_CONNECT"'

    act.expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        Execute statement error at attach :
        Data source : Firebird::
        -At trigger {TEST_TRIGGER_NAME}
    """

    act.isql(switches=['-q'], charset = 'utf8', input = test_script, combine_output = True, io_enc = locale.getpreferredencoding())
    
    # ::: ACHTUNG :::
    # Special connection must be done here, with ignoring DB-level triggers.
    # We have to drop such trigger otherwise problem will raise at teardown phase!
    #
    with act.db.connect(no_db_triggers = True) as con_kill_db_level_trigger:
        con_kill_db_level_trigger.execute_immediate('drop trigger trg_connect')
        con_kill_db_level_trigger.commit()

    assert act.clean_stdout == act.clean_expected_stdout
