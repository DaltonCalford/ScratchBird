#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8805
TITLE:       Regression in 6.x: query with "... where <field> in ( <scalar_value> | <scalar_func>, null )" returns empty rowset instead of existing record.
DESCRIPTION:
NOTES:
    [19.11.2025] pzotov
    Problem can be reproduced on 6.x snapshots since 07-nov-2025, up to 6.0.0.1356-df73f92.
    Checked on 6.0.0.1356-af5ee0f; 5.0.4.1735; 4.0.7.3237; 3.0.14.33827.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(id int);

    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION', 'VALUE_TO_BE_CHECKED', 1);
    end
    ^
    set term ;^

    insert into test values( rdb$get_context('USER_SESSION', 'VALUE_TO_BE_CHECKED') );
    commit;

    set count on;

    select 'a' as expected_data from test where id = rdb$get_context('USER_SESSION', 'VALUE_TO_BE_CHECKED') or id is null;
    select 'b' as expected_data from test where id = 1 or id is null;
    select 'c' as expected_data from test where id in ( rdb$get_context('USER_SESSION', 'VALUE_TO_BE_CHECKED'), null );
    select 'd' as expected_data from test where id in ( 1, null );
    select 'e' as expected_data from test where id in ( (select id from test rows 1), null );
"""

substitutions = [] # [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    EXPECTED_DATA                   a
    Records affected: 1
    EXPECTED_DATA                   b
    Records affected: 1
    EXPECTED_DATA                   c
    Records affected: 1
    EXPECTED_DATA                   d
    Records affected: 1
    EXPECTED_DATA                   e
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
