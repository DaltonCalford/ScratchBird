#coding:utf-8

"""
ID:          issue-6761
ISSUE:       6761
TITLE:       Hash join cannot match records using some TIME ZONE / DECFLOAT keys
DESCRIPTION:
JIRA:        CORE-6534
FBTEST:      bugs.core_6534
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- All subsequent statements must return non-empty result:
    set heading off;
    select 1 from (select timestamp '01.01.2021 13:00:00 +03:00' as ts from rdb$database) natural join (select timestamp '01.01.2021 12:00:00 +02:00' as ts from rdb$database);
    select 2 from (select cast(10 as decfloat) as df from rdb$database) natural join (select cast(10.000 as decfloat) as df from rdb$database);
    select 3 from (select cast('+0' as decfloat) as df from rdb$database) natural join (select cast('-0' as decfloat) as df from rdb$database);
    select 4 from (select cast('+0' as float) as f from rdb$database) natural join (select cast('-0' as float) as f from rdb$database);
    select 5 from (select cast('+0' as double precision) as d from rdb$database) natural join (select cast('-0' as double precision) as d from rdb$database);
    select 6 from (select cast('0E-6176' as decfloat) as d from rdb$database) natural join (select cast('0e0' as decfloat) as d from rdb$database);
    select 7 from (select time '05:05:05.5555' at time zone '-10:0' t from rdb$database) natural join (select time '05:05:05.5555' at time zone '+14:0' t from rdb$database);
"""

act = isql_act('db', test_script)

expected_stdout = """
    1
    2
    3
    4
    5
    6
    7
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
