#coding:utf-8

"""
ID:          issue-7731
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7731
TITLE:       Display length of timestamp with timezone is wrong in dialect 1
DESCRIPTION:
NOTES:
    [30.08.2023] pzotov
    Confirmed problem on 5.0.0.1169
    Checked on 5.0.0.1177 (intermediate snapshots).
"""

import locale

import pytest
from firebird.qa import *

db = db_factory(sql_dialect = 1)

test_script = f"""
    set heading off;
    SET BIND OF TIMESTAMP with time zone TO varchar;
    select timestamp '2023-08-29 21:22:23.0123 America/Argentina/ComodRivadavia' from rdb$database;
    select mon$sql_dialect from mon$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    29-AUG-2023 21:22:23.0123 America/Argentina/ComodRivadavia
    1
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
