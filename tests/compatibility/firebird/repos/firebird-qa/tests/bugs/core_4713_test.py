#coding:utf-8

"""
ID:          issue-5020
ISSUE:       5020
TITLE:       "BLOB not found" error at rollback after insert into table with expression index
DESCRIPTION:
JIRA:        CORE-4713
FBTEST:      bugs.core_4713
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table T1 ( bx blob sub_type text );

    create index t1_idx on T1 computed by (cast(substring(bx from 1 for 1000) as varchar(1000)));
    commit;

    set term ^;
    execute block as
        declare n int = 250;
    begin
        while (n>0) do insert into t1(bx) values( rpad( 'QWERTY', 1000, uuid_to_char(gen_uuid()) ) ) returning :n-1 into n;
    end
    ^
    set term ;^

    rollback;
    -- 2.5.3 (WI-V2.5.3.26780): no error.
    -- 2.5.2 (WI-V2.5.2.26540):
    --    Statement failed, SQLSTATE = HY000
    --    BLOB not found
    --    -BLOB not found
    --    -BLOB not found
    --    -BLOB not found
    --    Statement failed, SQLSTATE = 08003
    --    invalid transaction handle (expecting explicit transaction start)
    -- 2.5.1 (WI-V2.5.1.26351):
    --    the same as 2.5.2 + crash ("08006 / -Error reading data from the connection")
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
