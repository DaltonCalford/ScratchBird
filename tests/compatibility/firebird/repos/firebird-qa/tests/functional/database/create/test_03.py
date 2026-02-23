#coding:utf-8

"""
ID:          create-database-03
TITLE:       Create database: with PAGE_SIZE=1024: check actual size of page in the created database.
DESCRIPTION:
FBTEST:      functional.database.create.03
"""

import pytest
from firebird.qa import *

db = db_factory(page_size=1024)

test_script = """
    set list on;
    select mon$page_size as page_size from mon$database;
"""

act = isql_act('db', test_script, substitutions=[('^((?!PAGE_SIZE).)*$', '')])

expected_stdout = """
    PAGE_SIZE                       4096
"""

@pytest.mark.skip("Replaced with functional/database/create/test_00.py")
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
