#coding:utf-8

"""
ID:          issue-6738
ISSUE:       6738
TITLE:       Segfault when gfix requests for database page buffer more memory than available from OS
DESCRIPTION:
  Confirmed crash on 4.0.0.2377 (Windows and Linux)
  Checked on 4.0.0.2384 - all OK, get STDERR: "unable to allocate memory from operating system"
  NB: currently acceptable value for '-buffers' is limited from 50 to 2147483646.
JIRA:        CORE-6509
FBTEST:      bugs.core_6509
NOTES:
    [22.12.2021] pcisar
    Crashes v4.0.0.2496 and 4.0.1.2692 64-bit Linux
    [22.09.2022] pzotov
    No crashes on Windows and Linux (CentOS 7.9.2009).
    Returned test to be allowed for performing.
    In case of crash on other systems dump + debug info will be sent to FB team.
"""

import pytest
import re
from firebird.qa import *

db = db_factory(page_size=32768)

act = python_act('db')

expected_stdout = """
    Buffers value was not changed (expected)
    STDERR in gfix: unable to allocate memory from operating system
"""

pattern_for_page_buffers = re.compile('\\s*Page\\s+buffers\\s+\\d+', re.IGNORECASE)

@pytest.mark.version('>=9.0')
def test_1(act: Action, capsys):
    act.gstat(switches=['-h'])
    print(act.stdout)
    act.reset()
    act.expected_stderr = "We expect errors"
    act.gfix(switches=[act.db.dsn, '-buffers', '2147483646'])
    gfix_err = act.stderr
    act.reset()
    act.gstat(switches=['-h'])
    print(act.stdout)
    #
    buffers_set = set()
    for line in capsys.readouterr().out.splitlines():
        if pattern_for_page_buffers.search(line):
            buffers_set.add(line.split()[2])
    result = 'not changed (expected)' if len(buffers_set) == 1 else 'UNEXPECTEDLY changed: ' + ', '.join(buffers_set)
    print(f'Buffers value was {result}')
    for line in gfix_err.splitlines():
        print('STDERR in gfix:', line)
    # Check
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
