#coding:utf-8

"""
ID:          issue-8230
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8230
TITLE:       Ability to obtain PID of server process for current connection without querying mon$ tables
DESCRIPTION:
    Test verifies ability to call appropriate rdb$get_context() and compare its value with 
    mon$attachments.mon$server_pid. They must be equal (and no error must raise).
NOTES:
    [29.10.2024] pzotov
    Checked on 6.0.0.511-781e5d9 (intermediate build).
"""
import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select cast(rdb$get_context('SYSTEM', 'SERVER_PID') as int) - a.mon$server_pid as result
    from mon$attachments a
    where a.mon$attachment_id = current_connection;
"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        RESULT    0
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
