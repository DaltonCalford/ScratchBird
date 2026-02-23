#coding:utf-8

"""
ID:          issue-4760
ISSUE:       4760
TITLE:       ISQL crashed without connect when execute command "show version"
DESCRIPTION:
JIRA:        CORE-4440
FBTEST:      bugs.core_4440
NOTES:
    [12.12.2023] pzotov
    Added 'Error reading/writing' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see message related to any error.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    show version;
    set list on;
    select current_user as whoami from rdb$database;
"""

act = isql_act('db', test_script, substitutions = [ ('[ \t]+', ' '), ('^((?!SQLSTATE|(Error\\s+(reading|writing))|WHOAMI).)*$', '') ] )

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    expected_stdout = f"""
        WHOAMI {act.db.user.upper()}
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
