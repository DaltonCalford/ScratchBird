#coding:utf-8

"""
ID:          issue-3194
ISSUE:       3194
TITLE:       Views based on procedures can't be created if the proc's output fields participate in an expression
DESCRIPTION:
JIRA:        CORE-2806
FBTEST:      bugs.core_2806
NOTES:
    [27.06.2025] pzotov
    Reimplemented. No need to use 'SHOW VIEW' in this test.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

VIEW_DDL = 'select rc * 2 from sp_test'
test_script = f"""
    set list on;
    set blob all;
    set term ^;
    create procedure sp_test returns(rc int) as
    begin
        rc = 1;
        suspend;
    end^
    set term ;^
    create view v_test(double_rc) as {VIEW_DDL};
    commit;
    select r.rdb$view_source as blob_id from rdb$relations r where r.rdb$relation_name = upper('v_test');
    select * from v_test;
"""

substitutions = [('[ \t]+', ' '), ('BLOB_ID.*', '')]

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = f"""
    {VIEW_DDL}
    DOUBLE_RC 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
