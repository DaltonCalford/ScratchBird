#coding:utf-8

"""
ID:          issue-5382
ISSUE:       5382
TITLE:       COMPUTED-BY expressions are not converted to their field type inside the engine
DESCRIPTION:
JIRA:        CORE-5097
FBTEST:      bugs.core_5097
NOTES:
    [13.12.2023] pzotov
    Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test1(
        t0 timestamp default 'now'
        ,t1 timestamp computed by( 'now' )
        ,t2 computed by( extract(weekday from t1) )
    );
    recreate table test2 (n1 integer, c1 integer computed by (1.2));
    commit;

    insert into test1 default values;
    insert into test2 values (0);
    commit;

    set list on;
    set sqlda_display on;

    select * from test1 rows 0;
    select * from test2 rows 0;

    set sqlda_display off;

    select iif( t2 between 0 and 6, 1, 0 ) t2_check from test1;
    select c1 || '' as c1_check from test2;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype|T2_CHECK|C1_CHECK).)*$', '')])

expected_stdout = """
    01: sqltype: 510 TIMESTAMP Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 510 TIMESTAMP Nullable scale: 0 subtype: 0 len: 8
    03: sqltype: 500 SHORT Nullable scale: 0 subtype: 0 len: 2
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    T2_CHECK                        1
    C1_CHECK                        1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
