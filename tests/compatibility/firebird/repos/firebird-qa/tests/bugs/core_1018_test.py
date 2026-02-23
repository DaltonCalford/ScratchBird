#coding:utf-8

"""
ID:          issue-632
ISSUE:       632
TITLE:       Provide mechanism to get engine version without needing to call API function
DESCRIPTION:
JIRA:        CORE-1018
FBTEST:      bugs.core_1018
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    -- Engine version could contain more than one digit in any section or more sections.
    -- Changed pattern for matching such cases of engine version as:
    -- '3.2.23' or '3.3.2.1.0.1.2.3.4.5.7' etc
    select iif( t.ev similar to '[1-9]+.[[:DIGIT:]]+.[[:DIGIT:]]+((.?[[:DIGIT:]]+)*)', 'PRESENTS', null) as version
    from (
        select rdb$get_context('SYSTEM', 'ENGINE_VERSION') ev
        from rdb$database
    )t;
"""

act = isql_act('db', test_script, substitutions = [ ('[ \\t]+', ' ') ] )

expected_stdout = """
    VERSION PRESENTS
"""

@pytest.mark.version('>=3.0')
def test_3(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
