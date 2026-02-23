#coding:utf-8

"""
ID:          issue-4883
ISSUE:       4883
TITLE:       Incorrect size of the output parameter/argument when execute block, procedure
  or function use system field in metadata charset
DESCRIPTION:
JIRA:        CORE-4566
FBTEST:      bugs.core_4566
"""

import pytest
from firebird.qa import *

db = db_factory(charset='WIN1251')

test_script = """
    set term ^;
    create or alter function get_mnemonic (
        afield_name type of column rdb$types.rdb$field_name,
        atype type of column rdb$types.rdb$type)
    returns type of column rdb$types.rdb$type_name
    as
    begin
      return (select rdb$type_name
              from rdb$types
              where
                  rdb$field_name = :afield_name
                  and rdb$type = :atype
              order by rdb$type_name
             );
    end
    ^
    set term ;^
    commit;

    set list on;

    select get_mnemonic('MON$SHUTDOWN_MODE', 1) as mnemonic from rdb$database;
    --select cast(get_mnemonic('MON$SHUTDOWN_MODE', 1) as varchar(100)) as mnemonic from rdb$database;

    set term ^;
    execute block returns (FIELD_NAME RDB$FIELD_NAME) as
    begin
      for select rdb$type_name
          from rdb$types
          where rdb$type_name starting with 'BLOB_FILTER'
          order by rdb$type_name
          rows 1
          into field_name
      do
        suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    MNEMONIC                        MULTI_USER_SHUTDOWN
    FIELD_NAME                      BLOB_FILTER
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

