#coding:utf-8

"""
ID:          issue-2289
ISSUE:       2289
TITLE:       Arithmetic overflow or division by zero has occurred. in MAX function
DESCRIPTION:
  It was found that error raises in 2.1.0.17798 when we create table with two fields
  of UNICODE_FSS type and add there six rows. Content of fields were taken from table
  rdb$procedure_parameters of empty database.
  Data that are inserted have been found after several simplifications of source DB.
  Exception that raised:
    Statement failed, SQLCODE = -802
    arithmetic exception, numeric overflow, or string truncation
JIRA:        CORE-1859
FBTEST:      bugs.core_1859
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test( param_name rdb$parameter_name, proc_name rdb$procedure_name );
    commit;
    insert into test values('A1',  'ABCDEFGHIJK');
    insert into test values('A',   'ASDFGHJKL');
    commit;

    set list on;
    set count on;
    select sign(count(*))
    from (
        select t.param_name, max( t.proc_name ) pnmax
        from
        ( select t.param_name, t.proc_name from test t rows 6 ) t
        group by 1
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    SIGN                            1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

