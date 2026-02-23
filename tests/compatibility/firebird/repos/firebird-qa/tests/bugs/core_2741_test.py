#coding:utf-8

"""
ID:          issue-3136
ISSUE:       3136
TITLE:       Naive metadata extraction code in isql is defeated by "check" keyword typed in mixed case
DESCRIPTION:
JIRA:        CORE-2741
FBTEST:      bugs.core_2741
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain dm_int int cHeCk(vAlUE<>0);
    create domain dm_dts timestamp cHeCk(valUe<>cUrrent_timEstamp);
    commit;
    show domain dm_int;
    show domain dm_dts;
"""

substitutions = [('[ \t]+', ' ')]

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    DM_INT INTEGER Nullable
    cHeCk(vAlUE<>0)
    DM_DTS TIMESTAMP Nullable
    cHeCk(valUe<>cUrrent_timEstamp)
"""

expected_stdout_6x = """
    PUBLIC.DM_INT INTEGER Nullable
    cHeCk(vAlUE<>0)
    PUBLIC.DM_DTS TIMESTAMP Nullable
    cHeCk(valUe<>cUrrent_timEstamp)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
