#coding:utf-8

"""
ID:          issue-2174
ISSUE:       2174
TITLE:       DDL statement with AUTODDL ON won't show statistics
DESCRIPTION:
JIRA:        CORE-1749
FBTEST:      bugs.core_1749
"""

import pytest
from firebird.qa import *

substitutions = [('^Current memory.*', ''), ('^Delta memory.*', ''),
                 ('^Max memory.*', ''), ('^Elapsed time.*', ''), ('^Buffers.*', ''),
                 ('^Reads.*', ''), ('^Writes.*', ''), ('^Elapsed time.*', ''),
                 ('^Cpu.*', ''), ('^Fetches.*', 'STATS')]

init_script = """"""

db = db_factory(init=init_script)

test_script = """set stat on;

create generator A;

set autoddl off;

create generator B;
commit;

"""

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """STATS
STATS
STATS
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

