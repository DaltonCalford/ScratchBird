#coding:utf-8

"""
ID:          issue-4983
ISSUE:       4983
TITLE:       Regression: computed index based on a computed column stores NULL for all its keys
DESCRIPTION:
JIRA:        CORE-4673
FBTEST:      bugs.core_4673
NOTES:
    [30.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
  recreate table tc(
       id int primary key
      ,x int
      ,y int
      ,z_expr computed by (x+y)
      ,z_noex computed by (y)
  );
  commit;
  insert into tc values( 1, 101, 201 );
  commit;
  create index tc_lpad_z_expr on tc computed by( lpad('' || z_expr , 10, 0) );

  -- Added 2nd COMPUTED_BY index WITHOUT any expression,
  -- see dimitr's issue in the ticket, 02/Feb/15 08:52 AM.
  -- See also several samples (rus):
  -- sql.ru/forum/actualutils.aspx?action=gotomsg&tid=945713&msg=12655568
  create index tc_lpad_z_noex on tc computed by( z_noex );
  commit;
"""

db = db_factory(init=init_script)

test_script = """
  set plan on;
  set list on;
  select count(*) check_cnt from tc  where tc.z_noex = 201
  union all
  select count(*) from tc  where 0 + tc.z_noex = 201
  union all
  select count(*) cnt from tc where lpad('' || tc.z_expr, 10, 0) between '0000000302' and '0000000302'
  union all
  select count(*) from tc where '' || lpad('' || tc.z_expr, 10, 0) between '0000000302' and '0000000302';
"""

substitutions = [ ('[ \t]+', ' ') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
  PLAN (TC INDEX (TC_LPAD_Z_NOEX), TC NATURAL, TC INDEX (TC_LPAD_Z_EXPR), TC NATURAL)
  CHECK_CNT                       1
  CHECK_CNT                       1
  CHECK_CNT                       1
  CHECK_CNT                       1
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."TC" INDEX ("PUBLIC"."TC_LPAD_Z_NOEX"), "PUBLIC"."TC" NATURAL, "PUBLIC"."TC" INDEX ("PUBLIC"."TC_LPAD_Z_EXPR"), "PUBLIC"."TC" NATURAL)
    CHECK_CNT                       1
    CHECK_CNT                       1
    CHECK_CNT                       1
    CHECK_CNT                       1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
