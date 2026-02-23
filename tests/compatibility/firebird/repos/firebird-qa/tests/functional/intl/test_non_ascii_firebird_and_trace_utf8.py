#coding:utf-8

"""
ID:          functional.intl.test_non_ascii_firebird_and_trace_utf8.py
TITLE:       Check ability to obtain non-ascii content from firebird.log and trace when use ON DISCONNECT trigger and exception with non-ascii names, charset = UTF8.
DESCRIPTION: 
    We can make engine to put something in firebird.log by creating ON DISCONNECT trigger which raises exception.
    (see https://github.com/FirebirdSQL/firebird/issues/4282).
    In order this trigger not fired when we just create DB, aux user ('TMP_WORKER') is created and trigger will fire only for him.
    Before making connection as user 'TMP_WORKER', we:
        * get content of firebird.log
        * launch trace
    After tmp_worker completes its work (running ISQL with single command: 'quit'), we get again content of
    firebird.log and trace for further parsing.

    Finally, we check that:
        * trace contains messages about raised exception (with message) and failed trigger (with call stack);
        * difference in firebird.log contains messages similar to trace
NOTES:
    [04.09.2024] pzotov
    Test makes connections to DB using charset = 'utf8' and uses io_enc = 'utf-8' when obtaining content of firebird.log and trace.
    Checked 6.0.0.450, 5.0.2.1493, 4.0.6.3142 (on Windows).

    [13.03.2025] pzotov
    LINUX, FB 4.x: error message do not contain 'ERROR AT purge_attachment'.
    It must be considered as 'Known Bug' with minor priority.
    Decided to separate expected out after discuss with Alex, 13.03.2025.
"""

import os
import pytest
from firebird.qa import *
import re
import locale
from pathlib import Path
from difflib import unified_diff
import time

tmp_worker = user_factory('db', name='tmp_worker', password='123')
#tmp_sql = temp_file('tmp_non_ascii.sql')

db = db_factory(charset = 'utf8')
substitutions = [ ('.* FAILED EXECUTE_TRIGGER_FINISH', 'FAILED EXECUTE_TRIGGER_FINISH'),
                  ('.* ERROR AT purge_attachment', 'ERROR AT purge_attachment'),
                  ('(,)?\\s+line(:)?\\s+\\d+(,)?\\s+col(:)?\\s+\\d+', '')
                ]

act = python_act('db', substitutions = substitutions)

@pytest.mark.intl
@pytest.mark.trace
@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_worker: User, capsys):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_EXC_NAME = 'paramètre non trouvé' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"paramètre non trouvé"'
    TEST_TRG_NAME = "'gâchette de déconnexion'" if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"gâchette de déconnexion"'

    init_sql  = f"""
        set list on;
        set bail on;
        create exception "paramètre non trouvé" q'#Paramètre "@1" a une valeur incorrecte ou n'a pas été trouvé dans#';
        set term ^;
        ^
        create trigger "gâchette de déconnexion" on disconnect as
        begin
            if ( current_user != '{act.db.user}' ) then
            begin
                exception "paramètre non trouvé" using ('fréquence fermée');
            end
        end
        ^
        set term ;^
        commit;
    """
    
    #tmp_sql.write_bytes( bytes(init_sql.encode('utf-8')) )
    #act.isql(switches=['-q'], input_file = tmp_sql, combine_output = True, charset = 'utf8', io_enc = 'utf-8')

    act.expected_stdout = ''
    act.isql(switches=['-q'], input = init_sql, combine_output = True, charset = 'utf8', io_enc = 'utf-8')
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # ----------------------------------------------------------------------------------------------
    with act.connect_server(encoding = 'utf-8') as srv:
        srv.info.get_log()
        fb_log_init = srv.readlines()
    # ----------------------------------------------------------------------------------------------

    trace_cfg_items = [
        'log_connections = true',
        'log_transactions = true',
        'time_threshold = 0',
        'log_errors = true',
        'log_statement_finish = true',
        'log_trigger_finish = true',
        'max_sql_length = 32768',
    ]

    with act.trace(db_events = trace_cfg_items, encoding='utf-8'):
        test_sql = f"""
            set names utf8;
            connect '{act.db.dsn}' user {tmp_worker.name} password '{tmp_worker.password}';
            quit;
        """
        act.isql(switches = ['-q'], input = test_sql, connect_db=False, credentials = False, combine_output = True, io_enc = 'utf-8')
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

    # ----------------------------------------------------------------------------------------------

    allowed_patterns = \
    (
         re.escape(') FAILED EXECUTE_TRIGGER_FINISH')
        ,re.escape(') ERROR AT purge_attachment')
        ,re.escape('335544382 :') # name of exception: paramètre non trouvé (without quotes)
        ,re.escape('335545016 :') # message of exception: Paramètre "fréquence fermée" a une valeur incorrecte ou n'a pas été trouvé dans
        ,re.escape('335544842 : At trigger')
    )
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    # Example of trace:
    # 2024-09-04T18:57:20.7950 (2184:00000000016B23C0) ERROR AT purge_attachment
    # ...
    # 335544517 : exception 1
    # 335544382 : paramètre non trouvé
    # 335545016 : Paramètre "fréquence fermée" a une valeur incorrecte ou n'a pas été trouvé dans
    # 335544842 : At trigger 'gâchette de déconnexion' line: 5, col: 17

    for line in act.trace_log:
        #print(line)
        if line.strip():
            if act.match_any(line.strip(), allowed_patterns):
                print(line.strip())

    if os.name != 'nt' and act.is_version('<5'):
        # LINUX, FB 4.x: error message do not contain 'ERROR AT purge_attachment'.
        # It must be considered as 'Known Bug' with minor priority.
        # Decided to separate expected out after discuss with Alex, 13.03.2025.
        expected_trace_log = """
            FAILED EXECUTE_TRIGGER_FINISH
            335544382 : paramètre non trouvé
            335545016 : Paramètre "fréquence fermée" a une valeur incorrecte ou n'a pas été trouvé dans
            335544842 : At trigger 'gâchette de déconnexion'
        """
    else:
        expected_trace_log = f"""
            FAILED EXECUTE_TRIGGER_FINISH
            ERROR AT purge_attachment
            335544382 : {TEST_EXC_NAME}
            335545016 : Paramètre "fréquence fermée" a une valeur incorrecte ou n'a pas été trouvé dans
            335544842 : At trigger {TEST_TRG_NAME}
        """

    act.expected_stdout = expected_trace_log
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # ----------------------------------------------------------------------------------------------

    with act.connect_server(encoding = 'utf-8') as srv:
        srv.info.get_log()
        fb_log_curr = srv.readlines()
        srv.wait()

    # Example of firebird.log:
    # <HOSTNAME>  <timestamp>
    # Error at disconnect:
    # exception 1
    # paramètre non trouvé
    # Paramètre "fréquence fermée" a une valeur incorrecte ou n'a pas été trouvé dans
    # At trigger 'gâchette de déconnexion'

    fb_log_diff_patterns = \
    (
         'Error at disconnect'
        ,'exception'
        ,'paramètre non trouvé'
        ,'Paramètre "fréquence fermée"'
        ,'At trigger'
    )
    fb_log_diff_patterns = [ re.compile(p, re.IGNORECASE) for p in fb_log_diff_patterns ]
       
    for line in unified_diff(fb_log_init, fb_log_curr):
        if line.startswith('+'):
            if act.match_any(line[1:].strip(), fb_log_diff_patterns):
                print(line[1:].strip())

    expected_log_diff = f"""
        Error at disconnect:
        exception 1
        {TEST_EXC_NAME}
        Paramètre "fréquence fermée" a une valeur incorrecte ou n'a pas été trouvé dans
        At trigger {TEST_TRG_NAME}
    """

    act.expected_stdout = expected_log_diff
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
