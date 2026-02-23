#coding:utf-8

"""
ID:          issue-5605
ISSUE:       5605
TITLE:       Database gets partially corrupted in the "no-reserve" mode
DESCRIPTION:
  Test uses .fbk which was created on 2.5.7.
  We restore this database and run validation using gfix (NOT fbsvcmgr!).
  Validation should not produce any output and new lines in firebird.log should contain
  only messages about start and finish of validation with zero errors and warnings.
JIRA:        CORE-5329
FBTEST:      bugs.core_5329
"""

import pytest
import re
from difflib import unified_diff
from firebird.qa import *

db = db_factory(from_backup='core5329.fbk')

act = python_act('db', substitutions=[('\t+', ' ')])

expected_stdout = """
    + VALIDATION STARTED
    + VALIDATION FINISHED: 0 ERRORS, 0 WARNINGS, 0 FIXED
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action, capsys):
    pattern  = re.compile('.*VALIDATION.*|.*ERROR:.*')
    # Get firebird.log content BEFORE running validation
    log_before = act.get_firebird_log()
    # Only 'gfix -v' did show errors.
    # Online validation ('fbsvcmgr action_validate ...') worked WITHOUT any error/warningin its output.
    act.gfix(switches=['-v', '-full', act.db.dsn])
    # Get firebird.log content AFTER running validation
    log_after = act.get_firebird_log()
    # Check-1. Log of 'gfix -v -full'should be EMPTY
    assert act.clean_stdout == act.clean_expected_stdout
    # Check-2. Difference betweenold and new firebird.log should contain
    # only lines about validation start and finish, without errors:
    for line in unified_diff(log_before, log_after):
        if line.startswith('+'):
            if pattern.match(line.upper()):
                print(line.upper())
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
