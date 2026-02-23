#coding:utf-8

"""
ID:          issue-5059
ISSUE:       5059
TITLE:       Parameterized exception: wrong output when number of arguments greater than 7
DESCRIPTION:
JIRA:        CORE-4755
FBTEST:      bugs.core_4755
NOTES:
    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
	recreate exception ex_something_wrong 'Arguments for raising exeption:  @1  @2  @3  @4  @5  @6  @7  @8  @9';
	commit;
	set term ^;
	execute block as
	  declare v_1 int = 10001;
	  declare v_2 int = 10002;
	  declare v_3 int = 10003;
	  declare v_4 int = 10004;
	  declare v_5 int = 10005;
	  declare v_6 int = 10006;
	  declare v_7 int = 10007;
	  declare v_8 int = 10008;
	  declare v_9 int = 10009;
	begin
	   exception ex_something_wrong using(
          v_1
         ,v_2
         ,v_3
         ,v_4
         ,v_5
         ,v_6
         ,v_7
         ,v_8
         ,v_9
      );
    end
    ^
    set term ;^
    commit;

    recreate exception ex_too_much_detailed 'Arguments for raising exeption:  @1  @2  @3  @4  @5  @6  @7  @8  @9  @10';
    commit;
    set term ^;
    execute block as
      declare v_1 int = 10001;
      declare v_2 int = 10002;
      declare v_3 int = 10003;
      declare v_4 int = 10004;
      declare v_5 int = 10005;
      declare v_6 int = 10006;
      declare v_7 int = 10007;
      declare v_8 int = 10008;
      declare v_9 int = 10009;
      declare v_10 int = 10010;
    begin
       exception ex_too_much_detailed using(
          v_1
         ,v_2
         ,v_3
         ,v_4
         ,v_5
         ,v_6
         ,v_7
         ,v_8
         ,v_9
         ,v_10
      );
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('-At block line: [\\d]+, col: [\\d]+',
                                                  '-At block line')])

expected_stdout_5x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_SOMETHING_WRONG
    -Arguments for raising exeption:  10001  10002  10003  10004  10005  10006  10007  10008  10009
    -At block line
    Statement failed, SQLSTATE = 07002
    Number of arguments (10) exceeds the maximum (9) number of EXCEPTION USING arguments
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_SOMETHING_WRONG"
    -Arguments for raising exeption:  10001  10002  10003  10004  10005  10006  10007  10008  10009
    -At block line
    Statement failed, SQLSTATE = 07002
    Number of arguments (10) exceeds the maximum (9) number of EXCEPTION USING arguments
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
