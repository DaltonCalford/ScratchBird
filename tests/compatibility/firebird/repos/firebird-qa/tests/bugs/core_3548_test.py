#coding:utf-8

"""
ID:          issue-3904
ISSUE:       3904
TITLE:       GFIX returns an error after correctly shutting down a database
DESCRIPTION: Affects only local connections
JIRA:        CORE-3548
FBTEST:      bugs.core_3548
"""

import pytest
from firebird.qa import *
from firebird.driver import SrvStatFlag

db = db_factory()

act = python_act('db', substitutions=[('^((?!Attribute|connection).)*$', '')])

expected_stdout = """
    Attributes		full shutdown
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.gfix(switches=['-user', act.db.user, '-password', act.db.password,
                         '-shut', 'full', '-force', '0', str(act.db.db_path)])
    with act.connect_server() as srv:
        srv.database.get_statistics(database=act.db.db_path, flags=SrvStatFlag.HDR_PAGES)
        stats = srv.readlines()
    act.gfix(switches=['-user', act.db.user, '-password', act.db.password,
                         '-online', str(act.db.db_path)])
    act.stdout = '\n'.join(stats)
    act.expected_stdout = expected_stdout
    assert act.clean_stdout == act.clean_expected_stdout

