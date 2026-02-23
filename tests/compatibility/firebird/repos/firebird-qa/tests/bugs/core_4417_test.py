#coding:utf-8

"""
ID:          issue-4739
ISSUE:       4739
TITLE:       gbak: cannot commit index ; primary key with german umlaut
DESCRIPTION:
JIRA:        CORE-4417
FBTEST:      bugs.core_4417
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core4417-ods-11_2.fbk')

test_script = """
    -- Confirmed crash of restoring on WI-V2.5.2.26540: message about abnnormal program termination appeared.
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
