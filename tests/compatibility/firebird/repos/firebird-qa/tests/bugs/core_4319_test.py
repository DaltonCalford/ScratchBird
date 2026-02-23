#coding:utf-8

"""
ID:          issue-4642
ISSUE:       4642
TITLE:       Engine crashes when trace config contains line "connection_id=NN" and we attempt to connect to non-existent database/alias
DESCRIPTION:
    Test stores content of firebird.log before any actions, prepares config for trace (with intentional assigning connection_id to
    some positive value and time_threshold = 0) and launches ISQL with requirement to make connection to some non-existing alias.
    Output of ISQL must contan line 'SQLSTATE = 08001' and name of selected alias. It must *not* contain phrase about error reading/writing
    data from/to connection.
    Trace content must remain EMPTY, i.e. no any STDOUT or STDERR must be in it.
    Finally, we get content of firebird.log and compare it with initially stored one.
    Difference between them must not have any messages related to possible crash (see 'problematic_patterns').
JIRA:        CORE-4319
FBTEST:      bugs.core_4319
NOTES:
    [15.1.2022] pcisar
        This test fails on localized Windows due to encoding error and other
        expected output differences, so we skip it for now.

    [17.09.2022] pzotov
        One need to specify 'io_enc=locale.getpreferredencoding()' when invoke ISQL
        if we want this message to appear at console in readable view:
            act.isql(switches = ..., input = ..., charset = ..., io_enc=locale.getpreferredencoding())
        NOTE: specifying 'encoding_errors = ignore' in the DEFAULT section of firebird-driver.conf
        does not prevent from UnicodeDecode error in this case.

    Checked on Linux and Windows (localized, cp1251): 3.0.8.33535, 4.0.1.2692, 5.0.0.730
"""

import locale
import re
from difflib import unified_diff

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

trace_conf = """
    database
    {
        enabled = true
        log_connections = true
        log_errors = true
        time_threshold = 0
        connection_id = 1234
    }
""".split('\n')

NO_SUCH_ALIAS = 'n0_$uch_f1le'

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    log_before = act.get_firebird_log()
    
    sql_txt = f"""
        connect 'localhost:{NO_SUCH_ALIAS}' user {act.db.user} password '{act.db.password}';
        set list on;
        select mon$database_name from mon$database;
    """

    with act.trace(config = trace_conf, encoding = locale.getpreferredencoding(), encoding_errors='utf8'):
        act.isql(switches = ['-q'], input = sql_txt, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())

        possible_crash_pattern = re.compile(r'error\s+(reading|writing)\s+data\s+(from|to)(\s+the)?\s+connection', re.IGNORECASE)
        assert 'SQLSTATE = 08001' in act.stdout and NO_SUCH_ALIAS in act.stdout and not possible_crash_pattern.search(act.stdout)
        act.reset()
    
    # 04.03.2023. Trace in recent FB 4.x and 5.x contains FAILED ATTACH_DATABASE when we attempt to make connection to non-existnng alias.
    # Because of that, requirement about empty trace must be removed from here:
    # >>> DISABLED 04.03.2023 >>> assert len(act.trace_log) == 0

    # Get Firebird log after test
    log_after = act.get_firebird_log()

    problematic_patterns = [re.compile( r'access\s+violation',re.IGNORECASE),
                            re.compile( r'terminate\S+ abnormally',re.IGNORECASE),
                            re.compile( r'error\s+(reading|writing)\s+data',re.IGNORECASE)
                           ]
    for line in unified_diff(log_before, log_after):
        # ::: NB :::
        # filter(None, [p.search(line) for p in problematic_patterns]) will be None only in Python 2.7.x!
        # In Python 3.x this will retrun "<filter object at 0xNNNNN>" ==> we must NOT use this statement!
        if line.startswith('+') and act.match_any(line, problematic_patterns):
            print(f'Problematic message in firebird.log: {line.upper()}\n')

    assert '' == capsys.readouterr().out

