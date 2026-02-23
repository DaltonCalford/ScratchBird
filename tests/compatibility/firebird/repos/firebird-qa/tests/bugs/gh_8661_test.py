#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8661
TITLE:       Strange output from SHOW DEPENDENCIES command on 6.0
NOTES:
    [23.07.2025] pzotov
    Presense of several VIEWS that depend on table <T> caused weird output of command 'show depend <T>'.
    Confirmed bug (weird output) on 6.0.0.1050-cee7854.
    Checked on 6.0.0.1052-c6658eb; 5.0.3.1684; 4.0.6.3222; 3.0.13.33818

    [09.10.2025] pzotov
    Adjusted output for 6.x after 3e3b75 ("Re-add indexes for object name without schema name...")
    Checked on 6.0.0.1299
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test(id int primary key);
    create view v_test1 as select id from test;
    create view v_test2 as select id from test;
    create view v_test3 as select id from test;
    create view v_test4 as select id from test;
    create view v_test5 as select id from test;

    show depen test;
"""

act = isql_act('db', test_script, substitutions = [(r'\+\+\+.*', '')])

expected_stdout_5x = """
    V_TEST1:View, V_TEST1:View->ID, V_TEST2:View, V_TEST2:View->ID, V_TEST3:View, V_TEST3:View->ID, V_TEST4:View, V_TEST4:View->ID, V_TEST5:View, V_TEST5:View->ID
    [TEST:Table]
"""

expected_stdout_6x = """
    PUBLIC.V_TEST1:View, PUBLIC.V_TEST2:View, PUBLIC.V_TEST3:View, PUBLIC.V_TEST4:View, PUBLIC.V_TEST5:View, PUBLIC.V_TEST1:View->ID, PUBLIC.V_TEST2:View->ID, PUBLIC.V_TEST3:View->ID, PUBLIC.V_TEST4:View->ID, PUBLIC.V_TEST5:View->ID
    [PUBLIC.TEST:Table]
"""

@pytest.mark.version('>=3.0.13')
def test_1(act: Action):
    expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
