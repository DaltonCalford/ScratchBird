#coding:utf-8

"""
ID:          issue-7133
ISSUE:       7133
TITLE:       Order by for big (>34 digits) int128 values is broken when index on that field is used.
NOTES:
    [23.02.2023] pzotov
    Confirmed bug on 5.0.0.520
    Checked on 5.0.0.958 - works fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table biTest(dat int128);
    insert into biTest values(-170141183460469231731687303715884105726);
    insert into biTest values(170141183460469231731687303715884105725);
    insert into biTest values(170141183460469231731687303715884105727);
    insert into biTest values(-170141183460469231731687303715884105728);
    insert into biTest values(-170141183460469231731687303715884105727);
    insert into biTest values(170141183460469231731687303715884105726);

    insert into biTest values(12345678901234567890123456789012345678);
    insert into biTest values(12345678901234567890123456789012345679);
    insert into biTest values(12345678901234567890123456789012345677);
    commit;
    create index it on biTest(dat);
    commit;

    set heading off;
    select dat from biTest order by dat;
"""

act = isql_act('db', test_script)

expected_stdout = """
    -170141183460469231731687303715884105728
    -170141183460469231731687303715884105727
    -170141183460469231731687303715884105726
    12345678901234567890123456789012345677
    12345678901234567890123456789012345678
    12345678901234567890123456789012345679
    170141183460469231731687303715884105725
    170141183460469231731687303715884105726
    170141183460469231731687303715884105727
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
