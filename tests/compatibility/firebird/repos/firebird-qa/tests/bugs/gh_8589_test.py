#coding:utf-8

"""
ID:          issue-8589
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8589
TITLE:       PERCENT_RANK may return NaN instead of 0
DESCRIPTION:
NOTES:
    [07.06.2025] pzotov
    Confirmed bug on 6.0.0.797-0-303e8d4
    Checked on 6.0.0.797-bc305e6; 5.0.3.1369-fe53465; 4.0.6.3206-9580691

    [28.12.2025] pzotov
    Changed substitutions list: value +/-0e0 can be displayed with 16 digits after decimal point.
    We have to suppress "excessive" 16th+ zeroes.
    Detected on Intel Xeon W-2123 ("Intel64 Family 6 Model 85 Stepping 4") // Windows-10
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """

    set list on;
    recreate table test(
        id integer not null,
        the_int integer,
        primary key (id)
    );

    insert into test(the_int, id) values (5, 1);
    insert into test(the_int, id) values (6, 2);
    insert into test(the_int, id) values (7, 3);
    insert into test(the_int, id) values (13, 4);
    insert into test(the_int, id) values (5, 5);

    select
        id,
        the_int,
        row_number() over(partition by t.the_int order by t.id),
        percent_rank() over(partition by t.the_int order by t.id),
        cume_dist() over(partition by t.the_int order by t.id) 
    from test t 
    order by 1;
"""

act = isql_act('db', test_script, substitutions = [('[\t ]+', ' '), ('.0{15,}', '.000000000000000')])

expected_stdout = """
    ID 1
    THE_INT 5
    ROW_NUMBER 1
    PERCENT_RANK 0.000000000000000
    CUME_DIST 0.5000000000000000

    ID 2
    THE_INT 6
    ROW_NUMBER 1
    PERCENT_RANK 0.000000000000000
    CUME_DIST 1.000000000000000

    ID 3
    THE_INT 7
    ROW_NUMBER 1
    PERCENT_RANK 0.000000000000000
    CUME_DIST 1.000000000000000

    ID 4
    THE_INT 13
    ROW_NUMBER 1
    PERCENT_RANK 0.000000000000000
    CUME_DIST 1.000000000000000

    ID 5
    THE_INT 5
    ROW_NUMBER 2
    PERCENT_RANK 1.000000000000000
    CUME_DIST 1.000000000000000
"""

@pytest.mark.version('>=4.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

