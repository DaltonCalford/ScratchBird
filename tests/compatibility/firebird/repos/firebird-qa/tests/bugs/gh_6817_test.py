#coding:utf-8

"""
ID:          issue-6817
ISSUE:       6817
TITLE:       Command switch "-fetch_password <passwordfile>" does not work with gfix
DESCRIPTION:
FBTEST:      bugs.gh_6817
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

passfile = temp_file('tmp_gh_6817.dat')

@pytest.mark.version('>=3.0.7')
def test_1(act: Action, passfile: Path):
    passfile.write_text(act.db.password)
    act.gfix(switches=['-user', act.db.user, '-fetch_password', passfile, act.db.dsn, '-w', 'async'],
             credentials=False)
    act.gfix(switches=['-fetch_password', passfile, act.db.dsn, '-user', act.db.user, '-w', 'async'],
             credentials=False)
    act.gfix(switches=['-user', act.db.user, act.db.dsn, '-fetch_password', passfile, '-w', 'async'],
             credentials=False)
    act.gfix(switches=[act.db.dsn, '-fetch_password', passfile, '-user', act.db.user, '-w', 'async'],
             credentials=False)
