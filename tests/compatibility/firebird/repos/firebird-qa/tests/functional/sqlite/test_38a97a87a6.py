#coding:utf-8

"""
ID:          38a97a87a6
ISSUE:       https://www.sqlite.org/src/tktview/38a97a87a6
TITLE:       Inaccurate int/float comparison results in corrupt index
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    See also:
        https://en.wikipedia.org/wiki/Double-precision_floating-point_format
        Integers from -2^53 to 2^53 (-9,007,199,254,740,992 to 9,007,199,254,740,992) can be exactly represented.
        Integers between 2^53 and 2^54 = 18,014,398,509,481,984 round to a multiple of 2 (even number).
        Integers between 2^54 and 2^55 = 36,028,797,018,963,968 round to a multiple of 4.
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test(a integer primary key, b double precision);

    insert into test(a,b) values(9, 9007199254740993);
    insert into test(a,b) values(8, 9007199254740993.0);
    insert into test(a,b) values(7, 18014398509481984);
    insert into test(a,b) values(6, 18014398509481984.0);
    insert into test(a,b) values(5, 36028797018963968);
    insert into test(a,b) values(4, 36028797018963968.0);

    insert into test(a,b) values(3, 356282677878746339);
    insert into test(a,b) values(2, 356282677878746339.0);
    insert into test(a,b) values(1, 356282677878746340);
    commit;

    create index test_b on test(b);
    delete from test where a in (2,4,6,8);

    set count on;
    select * from test order by a desc;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 9
    B 9007199254740992.
    A 7
    B 1.801439850948198e+16
    A 5
    B 3.602879701896397e+16
    A 3
    B 3.562826778787464e+17
    A 1
    B 3.562826778787464e+17
    Records affected: 5
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
