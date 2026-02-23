#coding:utf-8

"""
ID:          issue-3901
ISSUE:       3901
TITLE:       Inconsistent domain's constraint validation in PSQL
DESCRIPTION:
JIRA:        CORE-3545
FBTEST:      bugs.core_3545
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
    begin
      begin execute statement 'drop table test'; when any do begin end end
      begin execute statement 'drop domain dm_text'; when any do begin end end
      begin execute statement 'drop domain dm_nums'; when any do begin end end
      begin execute statement 'drop collation co_nums'; when any do begin end end
    end
    ^ set term ;^
    commit;

    create domain dm_text varchar(2) check (value < '5');
    create collation co_nums for utf8 from unicode 'NUMERIC-SORT=1';
    create domain dm_nums varchar(3) character set utf8 check (value < '50') collate co_nums;
    create table test(id int, n dm_nums);
    commit;

    -- This should NOT produce error: domain 'dm_text' check text values without
    -- taking in account their numeric-sort aspect, so '40' will be LESS than '5'
    -- because of *alphabetical* comparison of these strings:
    set term ^;
    execute block as
      declare v1 dm_text = '40';
    begin
    end
    ^

    -- This also should NOT produce error: variable 'v1' will be implicitly casted
    -- to varchar(2), i.e. '40' and then alphabetical comparison will be in action:
    execute block as
      declare v1 dm_text = 40;
    begin
    end
    ^
    set term ;^
    ----------------------------------------------------------------------------------------------------------
    -- Block has been added 24.08.2015 due to message in http://sourceforge.net/p/firebird/code/62113
    -- ("Fixed (again) CORE-3545 - Inconsistent domain's constraint validation in PSQL. Related to CORE-3947")
    -- See tracker message of 28/May/15 02:11 PM about trouble with accepting incorrect values.
    ----------------------------------------------------------------------------------------------------------

    set list on;

    insert into test(id, n) values(1, 4);
    insert into test(id, n) values(2, 399);
    insert into test(id, n) values(2, 50);

    insert into test(id, n) values(3, '4');
    insert into test(id, n) values(3,'399');
    insert into test(id, n) values(4, '50');


    select * from test;
    commit;

    --set echo on;

    -- EB where no casting is required variables are assigned immediately to text literals.
    -- Should FAIL due to "var2_assignment_without_cast = '50';" - it violates constraint of domain dm_nums
    set term ^;
    execute block returns(var1_assignment_without_cast dm_nums, var2_assignment_without_cast dm_nums) as
    begin
      var1_assignment_without_cast = '4';
      var2_assignment_without_cast = '50';
      suspend;
    end
    ^

    -- EB where no casting is required variables are assigned immediately to text literals:
    -- Should FAIL due to "var2_assignment_without_cast = '399';" - it violates constraint of domain dm_nums
    execute block returns(var1_assignment_without_cast dm_nums, var2_assignment_without_cast dm_nums) as
    begin
      var1_assignment_without_cast = '4';
      var2_assignment_without_cast = '399';
      suspend;
    end
    ^

    -- EB where casting to domain type (varchar) is required due to integer values in right parts.
    -- Should FAIL due to "var2_cast_int_to_domain = 50;" - it violates constraint of domain dm_nums
    execute block returns(var1_cast_int_to_domain dm_nums, var2_cast_int_to_domain dm_nums) as
    begin
      var1_cast_int_to_domain = 4;
      var2_cast_int_to_domain = 50;
      suspend;
    end
    ^

    -- EB where casting to domain type (varchar) is required due to integer values in right parts.
    -- Should FAIL due to "var2_cast_int_to_domain = 399;" - it violates constraint of domain dm_nums
    execute block returns(var1_cast_int_to_domain dm_nums, var2_cast_int_to_domain dm_nums) as
    begin
      var1_cast_int_to_domain = 4;
      var2_cast_int_to_domain = 399;
      suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('-At block line: [\\d]+, col: [\\d]+', '-At block line')])

expected_stdout_5x = """
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."N", value "399"
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."N", value "50"
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."N", value "399"
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."N", value "50"
    ID                              1
    N                               4
    ID                              3
    N                               4
    Statement failed, SQLSTATE = 42000
    validation error for variable VAR2_ASSIGNMENT_WITHOUT_CAST, value "50"
    -At block line
    Statement failed, SQLSTATE = 42000
    validation error for variable VAR2_ASSIGNMENT_WITHOUT_CAST, value "399"
    -At block line
    Statement failed, SQLSTATE = 42000
    validation error for variable VAR2_CAST_INT_TO_DOMAIN, value "50"
    -At block line
    Statement failed, SQLSTATE = 42000
    validation error for variable VAR2_CAST_INT_TO_DOMAIN, value "399"
    -At block line
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."TEST"."N", value "399"
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."TEST"."N", value "50"
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."TEST"."N", value "399"
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."TEST"."N", value "50"
    ID                              1
    N                               4
    ID                              3
    N                               4
    Statement failed, SQLSTATE = 42000
    validation error for variable "VAR2_ASSIGNMENT_WITHOUT_CAST", value "50"
    -At block line
    Statement failed, SQLSTATE = 42000
    validation error for variable "VAR2_ASSIGNMENT_WITHOUT_CAST", value "399"
    -At block line
    Statement failed, SQLSTATE = 42000
    validation error for variable "VAR2_CAST_INT_TO_DOMAIN", value "50"
    -At block line
    Statement failed, SQLSTATE = 42000
    validation error for variable "VAR2_CAST_INT_TO_DOMAIN", value "399"
    -At block line
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
