#coding:utf-8

"""
ID:          issue-5586
ISSUE:       5586
TITLE:       Backup created under FB2.5 can be restored in FB3.0.0.32419 but not under current snapshot (FB3.0.1.32556)
DESCRIPTION:
  Test does trivial attempt to restore database which was given by ticket starter as attached file.
  This is done by Database fixture on its setup. No further actions required.
JIRA:        CORE-5309
FBTEST:      bugs.core_5309
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core5309.fbk')

act = python_act('db')

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    pass

