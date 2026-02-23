#coding:utf-8

"""
ID:          issue-6195
ISSUE:       6195
TITLE:       Crash for "gbak -se -b database nul"
DESCRIPTION:
JIRA:        CORE-5939
FBTEST:      bugs.core_5939
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stderr = """
    gbak: ERROR:service name parameter missing
    gbak:Exiting before completion due to errors
"""

fbk_file = temp_file('tmp_core_5939.fbk')

@pytest.mark.version('>=2.5.9')
def test_1(act: Action, fbk_file: Path):
    act.expected_stderr = expected_stderr
    act.gbak(switches=['-b', act.db.dsn, str(fbk_file), '-se'])
    assert act.clean_stderr == act.clean_expected_stderr
