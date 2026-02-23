#coding:utf-8

"""
ID:          issue-3554
ISSUE:       3554
TITLE:       ALTER VIEW with not matched columns in declaration and selection crashes the server
DESCRIPTION:
JIRA:        CORE-3180
FBTEST:      bugs.core_3180
NOTES:
    [27.06.2025] pzotov
    Suppressed name of altered view in order to have same expected* text for versions prior/since 6.x

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create view TEST_VIEW (ID) as select 1 from rdb$database;
    alter view TEST_VIEW (ID) as select 1, 2 from rdb$database;
    commit;
"""

substitutions = [('(-)?ALTER VIEW \\S+ failed', 'ALTER VIEW failed')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 07002
    unsuccessful metadata update
    ALTER VIEW failed
    -SQL error code = -607
    -Invalid command
    -number of columns does not match select list
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
