#coding:utf-8

"""
ID:          decimal-declared-scale
TITLE:       Dummy test
DESCRIPTION: Samples are from #3912 and  #5989.
FBTEST:      functional.datatypes.decimal_declared_scale
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test ( id int, b numeric(18,5) );
    insert into test(id, b) values (1, 1.0000199);
    insert into test(id, b) values (2, (select round(min(b),5) from test) );
    commit;

    select id, b, iif(b = 1.00002, 'true', 'false') c from test order by id;
    commit;

    recreate table test(id int, a decimal(18,18), b decimal(3,3) );
    commit;

    insert into test(id, a, b) values( 1, '9.123456789012345678', '999999.999' );
    select * from test order by id;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    B                               1.00002
    C                               true

    ID                              2
    B                               1.00002
    C                               true

    ID                              1
    A                               9.123456789012345678
    B                               999999.999
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
