#coding:utf-8

"""
ID:          new-database-08
TITLE:       New DB - RDB$FILES
DESCRIPTION: Check for correct content of RDB$FILES in new database.
FBTEST:      functional.basic.db.08
NOTES:
[17.01.2023] pzotov
    DISABLED after discussion with dimitr, letters 17-sep-2022 11:23.
    Reasons:
        * There is no much sense to keep such tests because they fails extremely often during new major FB developing.
        * There is no chanse to get successful outcome for the whole test suite is some of system table became invalid,
          i.e. lot of other tests will be failed in such case.
    Single test for check DDL (type of columns, their order and total number) will be implemented for all RDB-tables.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    select * from rdb$files
    order by
        rdb$file_name
        ,rdb$file_sequence
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
