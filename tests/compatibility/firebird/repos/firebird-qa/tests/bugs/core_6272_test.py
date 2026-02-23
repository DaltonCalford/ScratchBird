#coding:utf-8

"""
ID:          issue-6514
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6514
TITLE:       Failed attach to database not traced
DESCRIPTION:
  NB: connect to services must be done using LOCAL protocol rather than remote.
  Otherwise trace log will have only records about connect/disconnect to security.db.
  NO messages about failed search of non-existing database will appear.
  This is known bug, see Alex's issue in the tracker, 07-apr-2020 10:39.
NOTES:
[04.03.2021]
  Adapted to be run both on Windows and Linux.
  NOTE-1. There is difference between Windows and Linux message for gdscode = 335544344:
    * WINDOWS: 335544344 : I/O error during "CreateFile (open)" operation ...
    * LINUX:   335544344 : I/O error during "open" operation ...
  NOTE-2. Some messages can appear in the trace log ONE or TWO times (SS/CS ?).
  Because of this, we are interested only for at least one occurence of each message
  rather than for each of them (see 'found_patterns', type: set()).
[09.02.2022] pcisar
  Fails on Windows (with 4.0.1) - no patterns are found, because one can't use trace with embedded
  version on this platform.

[19.04.2022] pzotov
  act.trace() must be called with passing encoding=locale.getpreferredencoding(), otherwise we get
  UnicodeDecodeError because trace log may contain localized messages about missed DB file/alias.
  Checked on 4.0.1.2692, Windows and Linux.

JIRA:        CORE-6272
FBTEST:      bugs.core_6272
"""

import pytest
import re
import platform
from pathlib import Path
from firebird.qa import *
from firebird.driver import DatabaseError, driver_config
import locale

db = db_factory()
db_nonexistent = db_factory(filename='does_not_exists', do_not_create=True, do_not_drop=True)
act = python_act('db')

expected_stdout = """
    FOUND pattern: 335544344\\s*(:)?\\s+I(/)?O\\s+error
    FOUND pattern: 335544734\\s*(:)\\s+?Error\\s+while
    FOUND pattern: ERROR\\s+AT\\s+JProvider(:){1,2}attachDatabase
    FOUND pattern: FAILED\\s+ATTACH_DATABASE
"""


trace = """
    database
    {
        enabled = true
        log_connections = true
        log_errors = true
        log_initfini = false
    }
    """.splitlines()

trace_conf = temp_file('trace.conf')

@pytest.mark.trace
@pytest.mark.version('>=4.0')
def test_1(act: Action, db_nonexistent: Database, trace_conf: Path, capsys):
    with ServerKeeper(act, None): # Use embedded server for trace
        with act.trace(config=trace, encoding=locale.getpreferredencoding()):
            try:
                with db_nonexistent.connect():
                    pass
            except DatabaseError:
                pass
    # Process trace
    # Every of following patterns must be found at *least* once in the trace log:
    allowed_patterns = [re.compile('FAILED\\s+ATTACH_DATABASE', re.IGNORECASE),
                        re.compile('ERROR\\s+AT\\s+JProvider(:){1,2}attachDatabase', re.IGNORECASE),
                        # ::: NB ::: windows and linux messages *differ* for this gdscode:
                        re.compile('335544344\\s*(:)?\\s+I(/)?O\\s+error', re.IGNORECASE),
                        re.compile('335544734\\s*(:)\\s+?Error\\s+while', re.IGNORECASE),
                        ]
    found_patterns = set()

    for line in act.trace_log:
        for p in allowed_patterns:
            if p.search(line):
                found_patterns.add(p.pattern)
    
    for p in sorted(found_patterns):
        print(f'FOUND pattern: {p}')

    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
