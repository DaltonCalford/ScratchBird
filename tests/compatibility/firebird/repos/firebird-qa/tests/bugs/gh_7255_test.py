#coding:utf-8

"""
ID:          issue-7255
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7255
TITLE:       READ COMMITTED READ CONSISTENCY mode is broken in Classic / SuperClassic on Linux (the newly created users are not visible to execute statement)
DESCRIPTION:
    Issue can be reproduced only if firebird.conf contains ReadConsistency = 1.
    Problem will NOT appear if this parameter is set to 0 and we start transactions as 'READ COMMITTED READ CONSISTENCY'.
    Because of that, it was decided to make copy of firebird.conf, read its content and remote any line with 'ReadConsistency'.
    Then we add new line with ReadConsistency = 1 and OVERWRITE firebird.conf.
    After this, we run ISQL with script similar to provided in the ticket.
    Output must display name of new user and TIL of his transaction ('read committed read consistency') that is seen in procedure sp_main.
    Finally, we revert changes in firebir.conf using its original copy.
    All these actions are enclosed in the try/except/finally block.
NOTES:
    ### ACHTUNG ###
    Test tries temporary to change content of firebird.conf. In any outcome, this content will be restored at final point.

    Confirmed bug on 5.0.0.599, Classic. LINUX only.
    Checked on 5.0.0.1397, 4.0.5.3098.

    Command switch '--disable-db-cache' must be used if this test is running under 5.0.0.999 after some fresh FB with same ODS was tested.
    Otherwise "internal Firebird consistency check (decompression overran buffer (179), file: sqz.cpp line: 293)" will raise.
    Example:
        /opt/distr/venv/bin/pytest --disable-db-cache -vv --tb=long --server qa_rundaily_FB50 tests/bugs/gh_7255_test.py
"""
import shutil
import pytest
import locale
import re
import time
import platform
from pathlib import Path
from firebird.qa import *

db = db_factory(async_write = True)
act = python_act('db', substitutions = [('[ \t]+', ' ')])

fbcfg_bak = temp_file('firebird.conf')
p_read_consist_param = re.compile('ReadConsistency\\s*=\\s*(0|1)', re.IGNORECASE)
TMP_USR_NAME = 'tmp$7255'

@pytest.mark.skipif(platform.system() == 'Windows', reason='Reproduced on Linux only.')
@pytest.mark.version('>=4.0.3')
def test_1(act: Action, fbcfg_bak: Path, capsys):

    if act.vars['server-arch'].lower() != 'classic':
        pytest.skip('Can be reproduced only for Servermode = Classic.')

    fbcfg_file = act.vars['home-dir'] / 'firebird.conf'
    shutil.copy2(fbcfg_file, fbcfg_bak)

    try:
        fbcfg_ini = fbcfg_file.read_text(encoding='utf-8').splitlines()
        fbcfg_new = []
        for x in fbcfg_ini:
            if p_read_consist_param.search(x):
                pass
            else:
                fbcfg_new.append(x)

        fbcfg_new.append('ReadConsistency = 1')
        fbcfg_file.write_text('\n'.join(fbcfg_new), encoding='utf-8' )

        test_sql = f"""
            set list on;
            commit;
            SET KEEP_TRAN_PARAMS ON;
            SET TRANSACTION READ COMMITTED READ CONSISTENCY;

            create or alter user {TMP_USR_NAME} password '123';
            commit;

            set term ^;
            create or alter procedure sp_main(
                a_usr varchar(31), a_pwd varchar(31)
            ) returns(
                who varchar(50)
               ,til varchar(50)
            ) as
            begin
              for
                  execute statement
                         q'#select #'
                      || q'#    a.mon$user as who #'
                      || q'#    ,decode( #'
                      || q'#        t.mon$isolation_mode #'
                      || q'#        ,0, 'snapshot table stability' #'
                      || q'#        ,1, 'concurrency (snapshot)' #'
                      || q'#        ,2, 'read committed record version' #'
                      || q'#        ,3, 'read committed no record version' #'
                      || q'#        ,4, 'read committed read consistency' #'
                      || q'#    ) as til #'
                      || q'#from mon$attachments a #'
                      || q'#join mon$transactions t on a.mon$attachment_id = t.mon$attachment_id #'
                      || q'#where a.mon$attachment_id = current_connection and t.mon$state = 1 #'
                  as user a_usr password a_pwd
              into who, til
              do
                  suspend;
            end
            ^
            set term ;^
            commit;

            grant execute on procedure sp_main to {TMP_USR_NAME};
            commit;

            -- wait 10 seconds and it will work

            set term ^;
            execute block returns(
                who varchar(50)
               ,til varchar(50)
            ) as
            begin
                for
                    execute statement ('select who, til from sp_main(:u, :p)') ( u := '{TMP_USR_NAME}', p := '123' )
                into who, til
                do
                    suspend;
            end
            ^
            set term ;^
            commit;

            drop user {TMP_USR_NAME};
            commit;
        """

        act.isql(switches = ['-q'], input = test_sql, combine_output = True, io_enc = locale.getpreferredencoding())

    except OSError as e:
        print(e)
    finally:
        shutil.copy2(fbcfg_bak, act.vars['home-dir'] / 'firebird.conf')

    act.expected_stdout = f"""
        WHO {TMP_USR_NAME.upper()}
        TIL read committed read consistency
    """
    # act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
