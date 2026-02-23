#coding:utf-8

"""
ID:          issue-3274
ISSUE:       3274
TITLE:       SQLSTATE should also be available as a PSQL context variable like GDSCODE/SQLCODE
DESCRIPTION:
JIRA:        CORE-2890
FBTEST:      bugs.core_2890
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core2890.fbk')

test_script = """
    commit;
    set transaction no wait;

    update test set id = -id where id = 2;

    set list on;

    set term ^;
    execute block returns(res_sqlcode int, res_gdscode int, res_sqlstate char(5)) as
    begin
        for
            select res_sqlcode, res_gdscode, res_sqlstate
            from sp_test('I')
            into res_sqlcode, res_gdscode, res_sqlstate
        do
           suspend;
        -------------------------------------
        for
            select res_sqlcode, res_gdscode, res_sqlstate
            from sp_test('D')
            into res_sqlcode, res_gdscode, res_sqlstate
        do
           suspend;
        -------------------------------------
        for
            select res_sqlcode, res_gdscode, res_sqlstate
            from sp_test('U')
            into res_sqlcode, res_gdscode, res_sqlstate
        do
           suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    RES_SQLCODE                     -803
    RES_GDSCODE                     335544665
    RES_SQLSTATE                    23000

    RES_SQLCODE                     -913
    RES_GDSCODE                     335544336
    RES_SQLSTATE                    40001

    RES_SQLCODE                     -802
    RES_GDSCODE                     335544321
    RES_SQLSTATE                    22012
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

