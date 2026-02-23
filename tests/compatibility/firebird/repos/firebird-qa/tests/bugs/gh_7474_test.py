#coding:utf-8

"""
ID:          issue-7474
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7474
TITLE:       Incorrect condition evaluation - FB 5.0
NOTES:
    [01.03.2023] pzotov
    Confirmed bug on 5.0.0.932
    Checked on 5.0.0.964, intermediate build with timestamp 01-mar-2023 08:00. All fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table employee(i int);
    recreate table department (i int);
    commit;

    recreate table country (
        country   varchar(15) not null
        ,currency  varchar(10) not null
    );
    alter table country add primary key (country);


    recreate table department (
        dept_no     char(3) not null
        ,department  varchar(25) not null
        ,head_dept   char(3)
        ,mngr_no     smallint
        ,budget      decimal(12,2)
        ,location    varchar(15)
        ,phone_no    varchar(20) default '555-1234'
    );

    recreate table employee (
        emp_no       smallint not null
        ,first_name   varchar(15) not null
        ,last_name    varchar(20) not null
        ,phone_ext    varchar(4)
        ,hire_date    timestamp default 'now' not null
        ,dept_no      char(3) not null
        ,job_code     varchar(5) not null
        ,job_grade    smallint not null
        ,job_country  varchar(15) not null
        ,salary       numeric(10,2) not null
        ,full_name    computed by (last_name || ', ' || first_name)
    );

    create index namex on employee (last_name, first_name);
    create descending index budgetx on department (budget);
    commit;


    insert into country (country, currency) values ('usa', 'dollar');
    insert into country (country, currency) values ('england', 'pound');
    insert into country (country, currency) values ('canada', 'cdndlr');
    insert into country (country, currency) values ('switzerland', 'sfranc');
    insert into country (country, currency) values ('japan', 'yen');
    insert into country (country, currency) values ('italy', 'euro');
    insert into country (country, currency) values ('france', 'euro');
    insert into country (country, currency) values ('germany', 'euro');
    insert into country (country, currency) values ('australia', 'adollar');
    insert into country (country, currency) values ('hong kong', 'hkdollar');
    insert into country (country, currency) values ('netherlands', 'euro');
    insert into country (country, currency) values ('belgium', 'euro');
    insert into country (country, currency) values ('austria', 'euro');
    insert into country (country, currency) values ('fiji', 'fdollar');
    insert into country (country, currency) values ('russia', 'ruble');
    insert into country (country, currency) values ('romania', 'rleu');


    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('000', 'corporate headquarters', null, 105, 1000000, 'monterey', '(408) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('100', 'sales and marketing', '000', 85, 2000000, 'san francisco', '(415) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('600', 'engineering', '000', 2, 1100000, 'monterey', '(408) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('900', 'finance', '000', 46, 400000, 'monterey', '(408) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('180', 'marketing', '100', null, 1500000, 'san francisco', '(415) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('620', 'software products div.', '600', null, 1200000, 'monterey', '(408) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('621', 'software development', '620', null, 400000, 'monterey', '(408) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('622', 'quality assurance', '620', 9, 300000, 'monterey', '(408) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('623', 'customer support', '620', 15, 650000, 'monterey', '(408) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('670', 'consumer electronics div.', '600', 107, 1150000, 'burlington, vt', '(802) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('671', 'research and development', '670', 20, 460000, 'burlington, vt', '(802) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('672', 'customer services', '670', 94, 850000, 'burlington, vt', '(802) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('130', 'field office: east coast', '100', 11, 500000, 'boston', '(617) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('140', 'field office: canada', '100', 72, 500000, 'toronto', '(416) 677-1000');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('110', 'pacific rim headquarters', '100', 34, 600000, 'kuaui', '(808) 555-1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('115', 'field office: japan', '110', 118, 500000, 'tokyo', '3 5350 0901');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('116', 'field office: singapore', '110', null, 300000, 'singapore', '3 55 1234');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('120', 'european headquarters', '100', 36, 700000, 'london', '71 235-4400');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('121', 'field office: switzerland', '120', 141, 500000, 'zurich', '1 211 7767');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('123', 'field office: france', '120', 134, 400000, 'cannes', '58 68 11 12');
    insert into department (dept_no, department, head_dept, mngr_no, budget, location, phone_no) values ('125', 'field office: italy', '120', 121, 400000, 'milan', '2 430 39 39');


    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (2, 'robert', 'nelson', '250', '28-dec-1988 00:00:00', '600', 'vp', 2, 'usa', 105900);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (4, 'bruce', 'young', '233', '28-dec-1988 00:00:00', '621', 'eng', 2, 'usa', 97500);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (5, 'kim', 'lambert', '22', '6-feb-1989 00:00:00', '130', 'eng', 2, 'usa', 102750);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (8, 'leslie', 'johnson', '410', '5-apr-1989 00:00:00', '180', 'mktg', 3, 'usa', 64635);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (9, 'phil', 'forest', '229', '17-apr-1989 00:00:00', '622', 'mngr', 3, 'usa', 75060);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (11, 'k. j.', 'weston', '34', '17-jan-1990 00:00:00', '130', 'srep', 4, 'usa', 86292.94);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (12, 'terri', 'lee', '256', '1-may-1990 00:00:00', '000', 'admin', 4, 'usa', 53793);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (14, 'stewart', 'hall', '227', '4-jun-1990 00:00:00', '900', 'finan', 3, 'usa', 69482.63);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (15, 'katherine', 'young', '231', '14-jun-1990 00:00:00', '623', 'mngr', 3, 'usa', 67241.25);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (20, 'chris', 'papadopoulos', '887', '1-jan-1990 00:00:00', '671', 'mngr', 3, 'usa', 89655);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (24, 'pete', 'fisher', '888', '12-sep-1990 00:00:00', '671', 'eng', 3, 'usa', 81810.19);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (28, 'ann', 'bennet', '5', '1-feb-1991 00:00:00', '120', 'admin', 5, 'england', 22935);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (29, 'roger', 'de souza', '288', '18-feb-1991 00:00:00', '623', 'eng', 3, 'usa', 69482.63);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (34, 'janet', 'baldwin', '2', '21-mar-1991 00:00:00', '110', 'sales', 3, 'usa', 61637.81);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (36, 'roger', 'reeves', '6', '25-apr-1991 00:00:00', '120', 'sales', 3, 'england', 33620.63);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (37, 'willie', 'stansbury', '7', '25-apr-1991 00:00:00', '120', 'eng', 4, 'england', 39224.06);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (44, 'leslie', 'phong', '216', '3-jun-1991 00:00:00', '623', 'eng', 4, 'usa', 56034.38);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (45, 'ashok', 'ramanathan', '209', '1-aug-1991 00:00:00', '621', 'eng', 3, 'usa', 80689.5);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (46, 'walter', 'steadman', '210', '9-aug-1991 00:00:00', '900', 'cfo', 1, 'usa', 116100);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (52, 'carol', 'nordstrom', '420', '2-oct-1991 00:00:00', '180', 'prel', 4, 'usa', 42742.5);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (61, 'luke', 'leung', '3', '18-feb-1992 00:00:00', '110', 'srep', 4, 'usa', 68805);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (65, 'sue anne', 'o''brien', '877', '23-mar-1992 00:00:00', '670', 'admin', 5, 'usa', 31275);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (71, 'jennifer m.', 'burbank', '289', '15-apr-1992 00:00:00', '622', 'eng', 3, 'usa', 53167.5);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (72, 'claudia', 'sutherland', null, '20-apr-1992 00:00:00', '140', 'srep', 4, 'canada', 100914);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (83, 'dana', 'bishop', '290', '1-jun-1992 00:00:00', '621', 'eng', 3, 'usa', 62550);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (85, 'mary s.', 'macdonald', '477', '1-jun-1992 00:00:00', '100', 'vp', 2, 'usa', 111262.5);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (94, 'randy', 'williams', '892', '8-aug-1992 00:00:00', '672', 'mngr', 4, 'usa', 56295);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (105, 'oliver h.', 'bender', '255', '8-oct-1992 00:00:00', '000', 'ceo', 1, 'usa', 212850);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (107, 'kevin', 'cook', '894', '1-feb-1993 00:00:00', '670', 'dir', 2, 'usa', 111262.5);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (109, 'kelly', 'brown', '202', '4-feb-1993 00:00:00', '600', 'admin', 5, 'usa', 27000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (110, 'yuki', 'ichida', '22', '4-feb-1993 00:00:00', '115', 'eng', 3, 'japan', 6000000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (113, 'mary', 'page', '845', '12-apr-1993 00:00:00', '671', 'eng', 4, 'usa', 48000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (114, 'bill', 'parker', '247', '1-jun-1993 00:00:00', '623', 'eng', 5, 'usa', 35000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (118, 'takashi', 'yamamoto', '23', '1-jul-1993 00:00:00', '115', 'srep', 4, 'japan', 7480000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (121, 'roberto', 'ferrari', '1', '12-jul-1993 00:00:00', '125', 'srep', 4, 'italy', 33000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (127, 'michael', 'yanowski', '492', '9-aug-1993 00:00:00', '100', 'srep', 4, 'usa', 44000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (134, 'jacques', 'glon', null, '23-aug-1993 00:00:00', '123', 'srep', 4, 'france', 38500);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (136, 'scott', 'johnson', '265', '13-sep-1993 00:00:00', '623', 'doc', 3, 'usa', 60000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (138, 't.j.', 'green', '218', '1-nov-1993 00:00:00', '621', 'eng', 4, 'usa', 36000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (141, 'pierre', 'osborne', null, '3-jan-1994 00:00:00', '121', 'srep', 4, 'switzerland', 110000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (144, 'john', 'montgomery', '820', '30-mar-1994 00:00:00', '672', 'eng', 5, 'usa', 35000);
    insert into employee (emp_no, first_name, last_name, phone_ext, hire_date, dept_no, job_code, job_grade, job_country, salary) values (145, 'mark', 'guckenheimer', '221', '2-may-1994 00:00:00', '622', 'eng', 5, 'usa', 32000);
    commit;

    alter table department add unique (department);
    alter table department add primary key (dept_no);

    alter table employee add primary key (emp_no);

    alter table employee add foreign key (dept_no) references department (dept_no);
    alter table department add foreign key (head_dept) references department (dept_no);
    alter table department add foreign key (mngr_no) references employee (emp_no);
    commit;


    set count on;
    set list on;
    set term ^;
    execute block returns(
        emp_no type of column employee.emp_no
       ,dept_no type of column department.dept_no
       ,head_no type of column department.dept_no
    ) as
    begin
        for
            execute statement ('
            select e.emp_no, d.dept_no, h.dept_no
              from employee e
                inner join department d on d.dept_no = e.dept_no
                left outer join department h on h.dept_no = d.head_dept
              where e.emp_no in (12, 105) and
                    ((? is null) or (h.dept_no = ?))
              order by 1,2,3
            ') (null, null)
            into emp_no, dept_no, head_no
        do
            suspend;
    end
    ^
"""

act = isql_act('db', test_script)

expected_stdout = """
    EMP_NO                          12
    DEPT_NO                         000
    HEAD_NO                         <null>

    EMP_NO                          105
    DEPT_NO                         000
    HEAD_NO                         <null>

    Records affected: 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
