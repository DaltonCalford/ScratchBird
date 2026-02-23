#coding:utf-8

"""
ID:          issue-1430
ISSUE:       1430
TITLE:       Make information available regading ODS Version and Dialect via SQL
DESCRIPTION:
JIRA:        CORE-1019
FBTEST:      bugs.core_1019
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    -- Refactored 05-may-2018, to be universal for all possible ODS numbers:
    select
          iif( mon$ods_major similar to '[[:digit:]]{1,2}', 'YES', 'NO!') as "ods_major_looks_ok ?"
        , iif( mon$ods_minor similar to '[[:digit:]]{1,2}', 'YES', 'NO!') as "ods_minor_looks_ok ?"
        , iif( mon$sql_dialect similar to '[[:digit:]]{1}', 'YES', 'NO!') as "sql_dialect_looks_ok ?"
    from mon$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ods_major_looks_ok ?            YES
    ods_minor_looks_ok ?            YES
    sql_dialect_looks_ok ?          YES
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

