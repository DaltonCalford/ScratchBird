#coding:utf-8

"""
ID:          intfunc.string.replace
TITLE:       REPLACE( <stringtosearch>, <findstring>, <replstring> )
DESCRIPTION:
  Replaces all occurrences of <findstring> in <stringtosearch> with <replstring>.
FBTEST:      functional.intfunc.string.replace_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select REPLACE('toto','o','i') from rdb$database;")

expected_stdout = """
REPLACE
=======
titi
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
