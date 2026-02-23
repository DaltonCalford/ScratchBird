#coding:utf-8

"""
ID:          issue-5520
ISSUE:       5520
TITLE:       Affected rows are not counted for some update operations with views
DESCRIPTION:
JIRA:        CORE-5241
FBTEST:      bugs.core_5241
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate view v_test_b as select 1 i from rdb$database;
    recreate view v_test_a as select 1 i from rdb$database;
    commit;
    recreate table test (col int);
    recreate view v_test_a as select col from test;
    recreate view v_test_b as select col from v_test_a;
    commit;

    insert into test values (1);
    commit;

    set list on;

    set term ^;

    execute block returns (out_table int, out_view_a int, out_view_b int)
    as
    begin
      update test set col = 2;
      out_table = row_count;

      update v_test_a set col = 2;
      out_view_a = row_count;

      update v_test_b set col = 2;
      out_view_b = row_count;

      suspend;

    end^

    recreate trigger v_test_a_bu for v_test_a before update as begin end^
    recreate trigger v_test_b_bu for v_test_b before update as begin end^
    commit^

    execute block returns (out_table int, out_view_a int, out_view_b int)
    as
    begin


      update test set col = 2;
      out_table = row_count;

      update v_test_a set col = 2;
      out_view_a = row_count;

      update v_test_b set col = 2;
      out_view_b = row_count;

      suspend;

    end^

    execute block returns (out_table int, out_view_a int, out_view_b int)
    as
    begin

      update v_test_a set col = 2;
      out_view_a = row_count;

      update v_test_b set col = 2;
      out_view_b = row_count;


      update test set col = 2;
      out_table = row_count;

      suspend;

    end^

"""

act = isql_act('db', test_script)

expected_stdout = """
    OUT_TABLE                       1
    OUT_VIEW_A                      1
    OUT_VIEW_B                      1

    OUT_TABLE                       1
    OUT_VIEW_A                      1
    OUT_VIEW_B                      1

    OUT_TABLE                       1
    OUT_VIEW_A                      1
    OUT_VIEW_B                      1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

