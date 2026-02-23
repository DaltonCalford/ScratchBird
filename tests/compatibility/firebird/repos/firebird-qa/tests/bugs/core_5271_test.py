#coding:utf-8

"""
ID:          issue-5549
ISSUE:       5549
TITLE:       Regression: Can not create large index
DESCRIPTION:
JIRA:        CORE-5271
FBTEST:      bugs.core_5271
"""

import pytest
from firebird.qa import *

db = db_factory(page_size=16384)

act_1 = python_act('db')

expected_stdout = """
    RDB$INDEX_NAME                  PK_T1_S
    RDB$RELATION_NAME               T1
    RDB$UNIQUE_FLAG                 1
    RDB$SEGMENT_COUNT               1
    Records affected: 1
"""

test_sript = """
    recreate table t1(
      s varchar(4000) not null
    );

    commit;
    set term !;

    execute block
    as
      declare i int = 0;
    begin
      -- min = 7275 when database page_size=32768 and index key length = 8000
      -- min= 14750 when database page_size=16384 and index key length = 4000
      -- min= 29000 when database page_size=16384 and index key length = 2000
      while (i < 14750) do
      begin
        insert into t1(s) values( rpad('', 4000, uuid_to_char( gen_uuid() ) )  );
        i = i + 1;
      end
    end!

    set term ;!

    commit;
    alter table t1 add constraint pk_t1 primary key (s) using index pk_t1_s;
    commit;

    set list on;
    set count on;
    select rdb$index_name, rdb$relation_name, rdb$unique_flag,rdb$segment_count
    from rdb$indices
    where rdb$index_name = upper('pk_t1_s');
"""

@pytest.mark.version('>=4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout
    act_1.isql(switches=[], input=test_sript)
    assert act_1.clean_stdout == act_1.clean_expected_stdout


