#coding:utf-8

"""
ID:          issue-5555
ISSUE:       5555
TITLE:       Parameters with multibyte character sets allow to bypass the character limit of varchar fields
DESCRIPTION:
JIRA:        CORE-5277
FBTEST:      bugs.core_5277
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test (c varchar(2) character set utf8);
    commit;
"""

db = db_factory(sql_dialect=3, init=init_script)

act = python_act('db', substitutions=[('BULK> ', '')])

expected_stdout = """
    C                               ab
    C                               qw
    C                               er
    MNEMONIC                        MULTI_USER_SHUTDOWN
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 2, actual 3
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 1
    -stop
"""


test_script = """
    set bulk_insert insert into test values(?);
    ('ab')
    ('qw')
    ('er')
    ('tyu')
    stop
    set list on; -- !! do NOT remove this "duplicate" statement, otherwise data will be shown as when set list OFF !! Incredible... 8-O
    set list on;
    select * from test;

    commit;
    -----------------------------------
    -- Following sample was discussed with dimitr 29.06.2016.
    -- Statement failed, SQLSTATE = 22001
    -- arithmetic exception, numeric overflow, or string truncation
    -- -string right truncation
    -- -expected length 10, actual 17
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
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.isql(switches=[], input=test_script, charset='UTF8')
    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)
