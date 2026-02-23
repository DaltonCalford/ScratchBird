#coding:utf-8

"""
ID:          issue-3186
ISSUE:       3186
TITLE:       DB_KEY is always zero for external tables
DESCRIPTION:
NOTES:
  !!! IMPORTANT !!!
  This test requires config ExternalFileAccess = Full
JIRA:        CORE-2796
FBTEST:      bugs.core_2796
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table ext_test external file '$(DATABASE_LOCATION)c2796.dat' (col char(24), lf char(1));
    set list on;
    set term ^;
    execute block returns (ms_diff bigint)
    as
        declare dbkey char(8) character set octets;
        declare v_dts1 timestamp;
    begin
        v_dts1=cast('now' as timestamp);
        insert into ext_test values( :v_dts1-rand()*100, ascii_char(10));
        insert into ext_test values( :v_dts1-rand()*100, ascii_char(10));
        insert into ext_test values( :v_dts1-rand()*100, ascii_char(10));
        insert into ext_test values( :v_dts1,            ascii_char(10));
        insert into ext_test values( :v_dts1-rand()*100, ascii_char(10));

        select rdb$db_key from ext_test order by col desc rows 1 into :dbkey;

        for
            select datediff(millisecond from cast(col as timestamp) to :v_dts1)
            from ext_test
            where rdb$db_key = :dbkey
            into ms_diff
        do
            suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    MS_DIFF                         0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

