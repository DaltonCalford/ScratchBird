#coding:utf-8

"""
ID:          issue-5234
ISSUE:       5234
TITLE:       Dialect 1 casting date to string breaks when in the presence a domain with a check constraint
DESCRIPTION:
JIRA:        CORE-4943
FBTEST:      bugs.core_4943
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    -- Confirmed fail on build 2.5.5.26916.
    -- Works fine on: 2.5.5.26933, 3.0.0.32052

    set wng off;
    set term ^;
    create domain dm_without_chk as varchar(5);
    ^

    create or alter procedure sp_test1
    returns (
        retDate varchar(25)
        ,out_without_chk dm_without_chk
    ) as

    begin

      out_without_chk = 'qwe';

      retDate = cast('today' as date);

      suspend;
    end
    ^

    create domain dm_with_check as varchar(5)
           check (value is null or value in ('qwe', 'rty'))
    ^

    create or alter procedure sp_test2
    returns (
        retDate varchar(25)
        ,out_with_check dm_with_check
    ) as
    begin
      out_with_check = 'rty';

      retDate = cast('today' as date);

      suspend;
    end
    ^
    set term ;^
    commit;

    set list on;

    select iif(char_length(retDate)<=11, 1, 0) as sp_test1_format_ok
    from sp_test1;

    select iif(char_length(retDate)<=11, 1, 0) as sp_test2_format_ok
    from sp_test2;
"""

act = isql_act('db', test_script)

expected_stdout = """

    SP_TEST1_FORMAT_OK              1
    SP_TEST2_FORMAT_OK              1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

