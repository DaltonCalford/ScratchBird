#coding:utf-8

"""
ID:          issue-5620
ISSUE:       5620
TITLE:       Window Function: named window
DESCRIPTION:
  Simlified test for just check ability to compile and run a query with named window.
  More complex tests (e.g. when result of window functions are involved in JOIN expr.)
  will be implemented later.
NOTES:
  bug was found on 4.0.0.2298 when using named window.
  Discussed with hvlad and dimitr, letters since 21.12.2020 08:59
  See CORE-6460 and fix: https://github.com/FirebirdSQL/firebird/commit/964438507cacdfa192b8140ca3f1a2df340d511d
JIRA:        CORE-5346
FBTEST:      bugs.core_5346
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table emp_test (
        emp_no       smallint,
        dept_no      char(3),
        salary       numeric(10,2)
    );

    insert into emp_test (emp_no, dept_no, salary) values (2, '600', 105900);
    insert into emp_test (emp_no, dept_no, salary) values (4, '621', 97500);
    insert into emp_test (emp_no, dept_no, salary) values (5, '130', 102750);
    insert into emp_test (emp_no, dept_no, salary) values (8, '180', 64635);
    insert into emp_test (emp_no, dept_no, salary) values (9, '622', 75060);
    insert into emp_test (emp_no, dept_no, salary) values (11, '130', 86292.94);
    insert into emp_test (emp_no, dept_no, salary) values (12, '000', 53793);
    insert into emp_test (emp_no, dept_no, salary) values (14, '900', 69482.63);
    insert into emp_test (emp_no, dept_no, salary) values (15, '623', 67241.25);
    insert into emp_test (emp_no, dept_no, salary) values (20, '671', 89655);
    insert into emp_test (emp_no, dept_no, salary) values (24, '671', 81810.19);
    insert into emp_test (emp_no, dept_no, salary) values (28, '120', 22935);
    insert into emp_test (emp_no, dept_no, salary) values (29, '623', 69482.63);
    insert into emp_test (emp_no, dept_no, salary) values (34, '110', 61637.81);
    insert into emp_test (emp_no, dept_no, salary) values (36, '120', 33620.63);
    insert into emp_test (emp_no, dept_no, salary) values (37, '120', 39224.06);
    insert into emp_test (emp_no, dept_no, salary) values (44, '623', 56034.38);
    insert into emp_test (emp_no, dept_no, salary) values (45, '621', 80689.5);
    insert into emp_test (emp_no, dept_no, salary) values (46, '900', 116100);
    insert into emp_test (emp_no, dept_no, salary) values (52, '180', 42742.5);
    insert into emp_test (emp_no, dept_no, salary) values (61, '110', 68805);
    insert into emp_test (emp_no, dept_no, salary) values (65, '670', 31275);
    insert into emp_test (emp_no, dept_no, salary) values (71, '622', 53167.5);
    insert into emp_test (emp_no, dept_no, salary) values (72, '140', 100914);
    insert into emp_test (emp_no, dept_no, salary) values (83, '621', 62550);
    insert into emp_test (emp_no, dept_no, salary) values (85, '100', 111262.5);
    insert into emp_test (emp_no, dept_no, salary) values (94, '672', 56295);
    insert into emp_test (emp_no, dept_no, salary) values (105, '000', 212850);
    insert into emp_test (emp_no, dept_no, salary) values (107, '670', 111262.5);
    insert into emp_test (emp_no, dept_no, salary) values (109, '600', 27000);
    insert into emp_test (emp_no, dept_no, salary) values (110, '115', 6000000);
    insert into emp_test (emp_no, dept_no, salary) values (113, '671', 48000);
    insert into emp_test (emp_no, dept_no, salary) values (114, '623', 35000);
    insert into emp_test (emp_no, dept_no, salary) values (118, '115', 7480000);
    insert into emp_test (emp_no, dept_no, salary) values (121, '125', 33000);
    insert into emp_test (emp_no, dept_no, salary) values (127, '100', 44000);
    insert into emp_test (emp_no, dept_no, salary) values (134, '123', 38500);
    insert into emp_test (emp_no, dept_no, salary) values (136, '623', 60000);
    insert into emp_test (emp_no, dept_no, salary) values (138, '621', 36000);
    insert into emp_test (emp_no, dept_no, salary) values (141, '121', 110000);
    insert into emp_test (emp_no, dept_no, salary) values (144, '672', 35000);
    insert into emp_test (emp_no, dept_no, salary) values (145, '622', 32000);
    commit;

    set heading off;
    select
        e.emp_no,
        e.dept_no,
        e.salary,
        count(*) over w1,
        first_value(e.salary) over w2,
        last_value(e.salary) over w2
    from emp_test e
        window w1 as (partition by e.dept_no),
        w2 as (w1 order by e.salary, e.emp_no)
    order by
        rank() over w2,
        e.dept_no,
        e.salary;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    12 000                  53793.00                     2              53793.00              53793.00
    127 100                  44000.00                     2              44000.00              44000.00
    34 110                  61637.81                     2              61637.81              61637.81
    110 115                6000000.00                     2            6000000.00            6000000.00
    28 120                  22935.00                     3              22935.00              22935.00
    141 121                 110000.00                     1             110000.00             110000.00
    134 123                  38500.00                     1              38500.00              38500.00
    121 125                  33000.00                     1              33000.00              33000.00
    11 130                  86292.94                     2              86292.94              86292.94
    72 140                 100914.00                     1             100914.00             100914.00
    52 180                  42742.50                     2              42742.50              42742.50
    109 600                  27000.00                     2              27000.00              27000.00
    138 621                  36000.00                     4              36000.00              36000.00
    145 622                  32000.00                     3              32000.00              32000.00
    114 623                  35000.00                     5              35000.00              35000.00
    65 670                  31275.00                     2              31275.00              31275.00
    113 671                  48000.00                     3              48000.00              48000.00
    144 672                  35000.00                     2              35000.00              35000.00
    14 900                  69482.63                     2              69482.63              69482.63
    105 000                 212850.00                     2              53793.00             212850.00
    85 100                 111262.50                     2              44000.00             111262.50
    61 110                  68805.00                     2              61637.81              68805.00
    118 115                7480000.00                     2            6000000.00            7480000.00
    36 120                  33620.63                     3              22935.00              33620.63
    5 130                 102750.00                     2              86292.94             102750.00
    8 180                  64635.00                     2              42742.50              64635.00
    2 600                 105900.00                     2              27000.00             105900.00
    83 621                  62550.00                     4              36000.00              62550.00
    71 622                  53167.50                     3              32000.00              53167.50
    44 623                  56034.38                     5              35000.00              56034.38
    107 670                 111262.50                     2              31275.00             111262.50
    24 671                  81810.19                     3              48000.00              81810.19
    94 672                  56295.00                     2              35000.00              56295.00
    46 900                 116100.00                     2              69482.63             116100.00
    37 120                  39224.06                     3              22935.00              39224.06
    45 621                  80689.50                     4              36000.00              80689.50
    9 622                  75060.00                     3              32000.00              75060.00
    136 623                  60000.00                     5              35000.00              60000.00
    20 671                  89655.00                     3              48000.00              89655.00
    4 621                  97500.00                     4              36000.00              97500.00
    15 623                  67241.25                     5              35000.00              67241.25
    29 623                  69482.63                     5              35000.00              69482.63
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

