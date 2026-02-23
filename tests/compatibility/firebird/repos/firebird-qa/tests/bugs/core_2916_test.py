#coding:utf-8

"""
ID:          issue-3300
ISSUE:       3300
TITLE:       Broken error handling in the case of a conversion error happened during index creation
DESCRIPTION:
JIRA:        CORE-2916
FBTEST:      bugs.core_2916
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

# version: 3.0

db = db_factory()

test_script = """
    recreate table tab (col date);
    insert into tab (col) values ( current_date );
    commit;
    create index itab on tab computed (cast(col as int));
    commit;
"""

substitutions = [('(-)?conversion error from string.*', 'conversion error from string')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_3x = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "2021-09-27"
"""

expected_stdout_5x = """
    Statement failed, SQLSTATE = 22018
    Expression evaluation error for index "***unknown***" on table "TAB"
    conversion error from string "2021-09-27"
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 22018
    Expression evaluation error for index "***unknown***" on table "PUBLIC"."TAB"
    conversion error from string "2021-09-27"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
