#coding:utf-8

"""
ID:          create-database-12
TITLE:       Create database: with PAGE_SIZE=32768: check actual size of page in the created database.
DESCRIPTION:
"""

import pytest
from firebird.qa import *

db = db_factory(page_size=32768)

test_script = """
    set list on;
    select mon$page_size as page_size from mon$database;
"""

act = isql_act('db', test_script, substitutions=[('^((?!PAGE_SIZE).)*$', '')])

# version: 3.0

expected_stdout_1 = """
    PAGE_SIZE                       16384
"""

@pytest.mark.version('>=3,<4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

expected_stdout_2 = """
    PAGE_SIZE                       32768
"""

@pytest.mark.skip("Replaced with functional/database/create/test_00.py")
@pytest.mark.version('>=4')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
