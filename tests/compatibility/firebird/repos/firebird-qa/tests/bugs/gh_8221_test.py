#coding:utf-8

"""
ID:          issue-8221
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8221
TITLE:       Crash when MAKE_DBKEY() is called with 0 or 1 arguments
DESCRIPTION:
NOTES:
    [20.08.2024] pzotov
    Confirmed crash on 6.0.0.438-d40d01b (dob: 20.08.2024 04:44).
    Checked on 6.0.0.438-d9f9b28, 5.0.2.1479-adfe97a, 4.0.6.3142-984ccb9
"""
import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail OFF;
    select 1 from rdb$database where rdb$db_key = make_dbkey();
    select 1 from rdb$database where rdb$db_key = make_dbkey('RDB$DATABASE');
"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

@pytest.mark.version('>=4.0.6')
def test_1(act: Action):

    expected_stdout = f"""
        Statement failed, SQLSTATE = 39000
        function MAKE_DBKEY could not be matched

        Statement failed, SQLSTATE = 39000
        function MAKE_DBKEY could not be matched
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
