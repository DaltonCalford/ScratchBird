#coding:utf-8

"""
ID:          issue-3255
ISSUE:       3255
TITLE:       Outer ORDER BY clause has no effect
DESCRIPTION:
  If a derived table (or a view) contains both a left/right join and an ORDER BY clause
  and the outer query also contains an ORDER BY clause, the latter one gets ignored.
JIRA:        CORE-2871
FBTEST:      bugs.core_2871
"""

import pytest
from firebird.qa import *

init_script = """
CREATE DOMAIN COUNTRYNAME AS
VARCHAR(15);

CREATE DOMAIN DEPTNO AS
CHAR(3)
CHECK (VALUE = '000' OR (VALUE > '0' AND VALUE <= '999') OR VALUE IS NULL);

CREATE DOMAIN EMPNO AS
SMALLINT;

CREATE DOMAIN FIRSTNAME AS
VARCHAR(15);

CREATE DOMAIN JOBCODE AS
VARCHAR(5)
CHECK (VALUE > '99999');

CREATE DOMAIN JOBGRADE AS
SMALLINT
CHECK (VALUE BETWEEN 0 AND 6);

CREATE DOMAIN LASTNAME AS
VARCHAR(20);

CREATE DOMAIN PHONENUMBER AS
VARCHAR(20);

CREATE DOMAIN SALARY AS
NUMERIC(10,2)
DEFAULT 0
CHECK (VALUE > 0);

CREATE TABLE COUNTRY (
    COUNTRY   COUNTRYNAME NOT NULL,
    CURRENCY  VARCHAR(10) NOT NULL
);

CREATE TABLE EMPLOYEE (
    EMP_NO       EMPNO NOT NULL,
    FIRST_NAME   FIRSTNAME NOT NULL,
    LAST_NAME    LASTNAME NOT NULL,
    PHONE_EXT    VARCHAR(4),
    HIRE_DATE    TIMESTAMP DEFAULT 'NOW' NOT NULL,
    DEPT_NO      DEPTNO NOT NULL,
    JOB_CODE     JOBCODE NOT NULL,
    JOB_GRADE    JOBGRADE NOT NULL,
    JOB_COUNTRY  COUNTRYNAME NOT NULL,
    SALARY       SALARY NOT NULL,
    FULL_NAME    COMPUTED BY (last_name || ', ' || first_name)
);

INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (2, 'Robert', 'Nelson', '250', '1988-12-28 00:00:00', '600', 'VP', 2, 'USA', 105900);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (4, 'Bruce', 'Young', '233', '1988-12-28 00:00:00', '621', 'Eng', 2, 'USA', 97500);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (5, 'Kim', 'Lambert', '22', '1989-02-06 00:00:00', '130', 'Eng', 2, 'USA', 102750);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (8, 'Leslie', 'Johnson', '410', '1989-04-05 00:00:00', '180', 'Mktg', 3, 'USA', 64635);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (9, 'Phil', 'Forest', '229', '1989-04-17 00:00:00', '622', 'Mngr', 3, 'USA', 75060);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (11, 'K. J.', 'Weston', '34', '1990-01-17 00:00:00', '130', 'SRep', 4, 'USA', 86292.94);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (12, 'Terri', 'Lee', '256', '1990-05-01 00:00:00', '000', 'Admin', 4, 'USA', 53793);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (14, 'Stewart', 'Hall', '227', '1990-06-04 00:00:00', '900', 'Finan', 3, 'USA', 69482.63);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (15, 'Katherine', 'Young', '231', '1990-06-14 00:00:00', '623', 'Mngr', 3, 'USA', 67241.25);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (20, 'Chris', 'Papadopoulos', '887', '1990-01-01 00:00:00', '671', 'Mngr', 3, 'USA', 89655);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (24, 'Pete', 'Fisher', '888', '1990-09-12 00:00:00', '671', 'Eng', 3, 'USA', 81810.19);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (28, 'Ann', 'Bennet', '5', '1991-02-01 00:00:00', '120', 'Admin', 5, 'England', 22935);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (29, 'Roger', 'De Souza', '288', '1991-02-18 00:00:00', '623', 'Eng', 3, 'USA', 69482.63);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (34, 'Janet', 'Baldwin', '2', '1991-03-21 00:00:00', '110', 'Sales', 3, 'USA', 61637.81);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (36, 'Roger', 'Reeves', '6', '1991-04-25 00:00:00', '120', 'Sales', 3, 'England', 33620.63);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (37, 'Willie', 'Stansbury', '7', '1991-04-25 00:00:00', '120', 'Eng', 4, 'England', 39224.06);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (44, 'Leslie', 'Phong', '216', '1991-06-03 00:00:00', '623', 'Eng', 4, 'USA', 56034.38);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (45, 'Ashok', 'Ramanathan', '209', '1991-08-01 00:00:00', '621', 'Eng', 3, 'USA', 80689.5);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (46, 'Walter', 'Steadman', '210', '1991-08-09 00:00:00', '900', 'CFO', 1, 'USA', 116100);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (52, 'Carol', 'Nordstrom', '420', '1991-10-02 00:00:00', '180', 'PRel', 4, 'USA', 42742.5);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (61, 'Luke', 'Leung', '3', '1992-02-18 00:00:00', '110', 'SRep', 4, 'USA', 68805);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (65, 'Sue Anne', 'O''Brien', '877', '1992-03-23 00:00:00', '670', 'Admin', 5, 'USA', 31275);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (71, 'Jennifer M.', 'Burbank', '289', '1992-04-15 00:00:00', '622', 'Eng', 3, 'USA', 53167.5);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (72, 'Claudia', 'Sutherland', NULL, '1992-04-20 00:00:00', '140', 'SRep', 4, 'Canada', 100914);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (83, 'Dana', 'Bishop', '290', '1992-06-01 00:00:00', '621', 'Eng', 3, 'USA', 62550);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (85, 'Mary S.', 'MacDonald', '477', '1992-06-01 00:00:00', '100', 'VP', 2, 'USA', 111262.5);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (94, 'Randy', 'Williams', '892', '1992-08-08 00:00:00', '672', 'Mngr', 4, 'USA', 56295);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (105, 'Oliver H.', 'Bender', '255', '1992-10-08 00:00:00', '000', 'CEO', 1, 'USA', 212850);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (107, 'Kevin', 'Cook', '894', '1993-02-01 00:00:00', '670', 'Dir', 2, 'USA', 111262.5);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (109, 'Kelly', 'Brown', '202', '1993-02-04 00:00:00', '600', 'Admin', 5, 'USA', 27000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (110, 'Yuki', 'Ichida', '22', '1993-02-04 00:00:00', '115', 'Eng', 3, 'Japan', 6000000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (113, 'Mary', 'Page', '845', '1993-04-12 00:00:00', '671', 'Eng', 4, 'USA', 48000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (114, 'Bill', 'Parker', '247', '1993-06-01 00:00:00', '623', 'Eng', 5, 'USA', 35000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (118, 'Takashi', 'Yamamoto', '23', '1993-07-01 00:00:00', '115', 'SRep', 4, 'Japan', 7480000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (121, 'Roberto', 'Ferrari', '1', '1993-07-12 00:00:00', '125', 'SRep', 4, 'Italy', 99000000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (127, 'Michael', 'Yanowski', '492', '1993-08-09 00:00:00', '100', 'SRep', 4, 'USA', 44000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (134, 'Jacques', 'Glon', NULL, '1993-08-23 00:00:00', '123', 'SRep', 4, 'France', 390500);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (136, 'Scott', 'Johnson', '265', '1993-09-13 00:00:00', '623', 'Doc', 3, 'USA', 60000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (138, 'T.J.', 'Green', '218', '1993-11-01 00:00:00', '621', 'Eng', 4, 'USA', 36000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (141, 'Pierre', 'Osborne', NULL, '1994-01-03 00:00:00', '121', 'SRep', 4, 'Switzerland', 110000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (144, 'John', 'Montgomery', '820', '1994-03-30 00:00:00', '672', 'Eng', 5, 'USA', 35000);
INSERT INTO EMPLOYEE (EMP_NO, FIRST_NAME, LAST_NAME, PHONE_EXT, HIRE_DATE, DEPT_NO, JOB_CODE, JOB_GRADE, JOB_COUNTRY, SALARY) VALUES (145, 'Mark', 'Guckenheimer', '221', '1994-05-02 00:00:00', '622', 'Eng', 5, 'USA', 32000);

COMMIT WORK;

INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('USA', 'Dollar');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('England', 'Pound');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Canada', 'CdnDlr');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Switzerland', 'SFranc');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Japan', 'Yen');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Italy', 'Lira');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('France', 'FFranc');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Germany', 'D-Mark');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Australia', 'ADollar');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Hong Kong', 'HKDollar');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Netherlands', 'Guilder');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Belgium', 'BFranc');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Austria', 'Schilling');
INSERT INTO COUNTRY (COUNTRY, CURRENCY) VALUES ('Fiji', 'FDollar');

COMMIT WORK;

ALTER TABLE COUNTRY ADD PRIMARY KEY (COUNTRY);
ALTER TABLE EMPLOYEE ADD PRIMARY KEY (EMP_NO);

CREATE INDEX NAMEX ON EMPLOYEE (LAST_NAME, FIRST_NAME);
"""

db = db_factory(init=init_script)

test_script = """
select *
from (
    select
        emp_no, first_name, last_name, country, currency
    from employee
        left join country on employee.job_country = country.country
    order by last_name
)
order by emp_no desc;
"""

act = isql_act('db', test_script)

expected_stdout = """ EMP_NO FIRST_NAME      LAST_NAME            COUNTRY         CURRENCY
======= =============== ==================== =============== ==========
    145 Mark            Guckenheimer         USA             Dollar
    144 John            Montgomery           USA             Dollar
    141 Pierre          Osborne              Switzerland     SFranc
    138 T.J.            Green                USA             Dollar
    136 Scott           Johnson              USA             Dollar
    134 Jacques         Glon                 France          FFranc
    127 Michael         Yanowski             USA             Dollar
    121 Roberto         Ferrari              Italy           Lira
    118 Takashi         Yamamoto             Japan           Yen
    114 Bill            Parker               USA             Dollar
    113 Mary            Page                 USA             Dollar
    110 Yuki            Ichida               Japan           Yen
    109 Kelly           Brown                USA             Dollar
    107 Kevin           Cook                 USA             Dollar
    105 Oliver H.       Bender               USA             Dollar
     94 Randy           Williams             USA             Dollar
     85 Mary S.         MacDonald            USA             Dollar
     83 Dana            Bishop               USA             Dollar
     72 Claudia         Sutherland           Canada          CdnDlr
     71 Jennifer M.     Burbank              USA             Dollar

 EMP_NO FIRST_NAME      LAST_NAME            COUNTRY         CURRENCY
======= =============== ==================== =============== ==========
     65 Sue Anne        O'Brien              USA             Dollar
     61 Luke            Leung                USA             Dollar
     52 Carol           Nordstrom            USA             Dollar
     46 Walter          Steadman             USA             Dollar
     45 Ashok           Ramanathan           USA             Dollar
     44 Leslie          Phong                USA             Dollar
     37 Willie          Stansbury            England         Pound
     36 Roger           Reeves               England         Pound
     34 Janet           Baldwin              USA             Dollar
     29 Roger           De Souza             USA             Dollar
     28 Ann             Bennet               England         Pound
     24 Pete            Fisher               USA             Dollar
     20 Chris           Papadopoulos         USA             Dollar
     15 Katherine       Young                USA             Dollar
     14 Stewart         Hall                 USA             Dollar
     12 Terri           Lee                  USA             Dollar
     11 K. J.           Weston               USA             Dollar
      9 Phil            Forest               USA             Dollar
      8 Leslie          Johnson              USA             Dollar
      5 Kim             Lambert              USA             Dollar

 EMP_NO FIRST_NAME      LAST_NAME            COUNTRY         CURRENCY
======= =============== ==================== =============== ==========
      4 Bruce           Young                USA             Dollar
      2 Robert          Nelson               USA             Dollar

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

