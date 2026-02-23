#coding:utf-8

"""
ID:          issue-5438
ISSUE:       5438
TITLE:       [CREATE OR] ALTER USER statement: clause PASSWORD (if present) must be always specified just after USER
DESCRIPTION:
JIRA:        CORE-5155
FBTEST:      bugs.core_5155
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter user tmp$c5155 password '123' firstname 'john' revoke admin role;
    create or alter user tmp$c5155 revoke admin role firstname 'john' password '123';
    create or alter user tmp$c5155 firstname 'john' revoke admin role password '123' lastname 'smith';
    create or alter user tmp$c5155 lastname 'adams' grant admin role firstname 'mick' password '123';
    create or alter user tmp$c5155 revoke admin role lastname 'adams' firstname 'mick' password '123';
    drop user tmp$c5155;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()

