#coding:utf-8

"""
ID:          issue-3998
ISSUE:       3998
TITLE:       Window Function: frame (rows / range) clause
DESCRIPTION:
  I decided to compare results with some trivial tests that can be found in WWW for old Oracle SCOTT scheme.
  DDL and data for this can be found in several places, e.g.:
  * https://code.google.com/archive/p/adf-samples-demos/downloads
  * http://www.orafaq.com/wiki/SCOTT
  * http://www.sql.ru/forum/26520/shema-scott-a

  Note that sources contain different values for EMP.HIRE_DATE for two records
  with empno = 7788 and 7876 ('Scott' and 'Adams').
  Also, some sources can contain for record with empno = 7839 ('king') field emp.comm = 0 or null.
  This script uses following values:
  scott: hiredate = 09-dec-1982;  adams: hireate = 12-jan-1983;  king: comm = null.

  If some other tests of window (analytical) functions will require the same script
  this DDL will be moved to separate .fbk for sharing between them.
NOTES:
[21.12.2020]
  added 'EMPNO' (primary key column) to ORDER BY list inside OVER() clauses.
  Otherwise records in 'sample2-a' can appear in unpredictable order.
JIRA:        CORE-3647
FBTEST:      bugs.core_3647
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table emp (id int);
    recreate table dept (id int);
    commit;

    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop sequence emp_table_seq';
        when any do begin end
        end
        begin
            execute statement 'drop sequence dept_table_seq';
        when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create sequence emp_table_seq start with 1000;
    create sequence dept_table_seq start with 41;

    recreate table dept (
        deptno smallint,
        dname varchar(14),
        loc varchar(13),
        constraint dept_pk primary key (deptno)
    );

    insert into dept values (10, 'accounting', 'new york');
    insert into dept values (20, 'research',   'dallas');
    insert into dept values (30, 'sales',      'chicago');
    insert into dept values (40, 'operations', 'boston');
    commit;

    recreate table emp (
        empno smallint not null,
        ename varchar(10),
        job varchar(9),
        mgr smallint,
        hiredate date,
        sal numeric(7, 2),
        comm numeric(7, 2),
        deptno smallint,
        constraint employee_pk primary key (empno),
        constraint works_in_dept foreign key (deptno) references dept on delete set null
    );

    -- https://code.google.com/archive/p/adf-samples-demos/downloads
    insert into emp values(7369, 'smith',  'clerk',     7902,'17-dec-1980',  800, null, 20);
    insert into emp values(7499, 'allen',  'salesman',  7698,'20-feb-1981', 1600,  300, 30);
    insert into emp values(7521, 'ward',   'salesman',  7698,'22-feb-1981', 1250,  500, 30);
    insert into emp values(7566, 'jones',  'manager',   7839,'2-apr-1981',  2975, null, 20);
    insert into emp values(7654, 'martin', 'salesman',  7698,'28-sep-1981', 1250, 1400, 30);
    insert into emp values(7698, 'blake',  'manager',   7839,'1-may-1981',  2850, null, 30);
    insert into emp values(7782, 'clark',  'manager',   7839,'9-jun-1981',  2450, null, 10);
    insert into emp values(7788, 'scott',  'analyst',   7566,'09-dec-1982', 3000, null, 20); -- <<< 19-apr-1987 ?
    insert into emp values(7839, 'king',   'president', null,'17-nov-1981', 5000, null, 10);
    insert into emp values(7844, 'turner', 'salesman',  7698,'8-sep-1981',  1500,    0, 30);
    insert into emp values(7876, 'adams',  'clerk',     7788,'12-jan-1983', 1100, null, 20); -- << 23-may-1987 ?
    insert into emp values(7900, 'james',  'clerk',     7698,'3-dec-1981',   950, null, 30);
    insert into emp values(7902, 'ford',   'analyst',   7566,'3-dec-1981',  3000, null, 20);
    insert into emp values(7934, 'miller', 'clerk',     7782,'23-jan-1982', 1300, null, 10);
    commit;

    /*
    -- http://www.orafaq.com/wiki/SCOTT
    SCOTT> SELECT * FROM emp ORDER BY empno;
         EMPNO ENAME      JOB              MGR HIREDATE           SAL       COMM     DEPTNO
    ---------- ---------- --------- ---------- ----------- ---------- ---------- ----------
          7369 SMITH      CLERK           7902 17-DEC-1980        800                    20
          7499 ALLEN      SALESMAN        7698 20-FEB-1981       1600        300         30
          7521 WARD       SALESMAN        7698 22-FEB-1981       1250        500         30
          7566 JONES      MANAGER         7839 02-APR-1981       2975                    20
          7654 MARTIN     SALESMAN        7698 28-SEP-1981       1250       1400         30
          7698 BLAKE      MANAGER         7839 01-MAY-1981       2850                    30
          7782 CLARK      MANAGER         7839 09-JUN-1981       2450                    10
          7788 SCOTT      ANALYST         7566 19-APR-1987       3000                    20 <<< 9-dec-1982 ?
          7839 KING       PRESIDENT            17-NOV-1981       5000                    10
          7844 TURNER     SALESMAN        7698 08-SEP-1981       1500          0         30
          7876 ADAMS      CLERK           7788 23-MAY-1987       1100                    20 <<< 12-jan-1983 ?
          7900 JAMES      CLERK           7698 03-DEC-1981        950                    30
          7902 FORD       ANALYST         7566 03-DEC-1981       3000                    20
          7934 MILLER     CLERK           7782 23-JAN-1982       1300                    10
    14 rows selected.

    -- http://www.sql.ru/forum/26520/shema-scott-a

    insert into emp values(7369, 'smith',  'clerk',     7902,to_date('17-dec-1980', 'dd-mon-yyyy'),  800, null, 20);
    insert into emp values(7499, 'allen',  'salesman',  7698,to_date('20-feb-1981', 'dd-mon-yyyy'), 1600,  300, 30);
    insert into emp values(7521, 'ward',   'salesman',  7698,to_date('22-feb-1981', 'dd-mon-yyyy'), 1250,  500, 30);
    insert into emp values(7566, 'jones',  'manager',   7839,to_date('2-apr-1981', 'dd-mon-yyyy'),  2975, null, 20);
    insert into emp values(7654, 'martin', 'salesman',  7698,to_date('28-sep-1981', 'dd-mon-yyyy'), 1250, 1400, 30);
    insert into emp values(7698, 'blake',  'manager',   7839,to_date('1-may-1981', 'dd-mon-yyyy'),  2850, null, 30);
    insert into emp values(7782, 'clark',  'manager',   7839,to_date('9-jun-1981', 'dd-mon-yyyy'),  2450, null, 10);
    insert into emp values(7788, 'scott',  'analyst',   7566,to_date('09-dec-1982', 'dd-mon-yyyy'), 3000, null, 20);
    insert into emp values(7839, 'king',   'president', null,to_date('17-nov-1981', 'dd-mon-yyyy'), 5000, null, 10);
    insert into emp values(7844, 'turner', 'salesman',  7698,to_date('8-sep-1981', 'dd-mon-yyyy'),  1500,    0, 30);
    insert into emp values(7876, 'adams',  'clerk',     7788,to_date('12-jan-1983', 'dd-mon-yyyy'), 1100, null, 20);
    insert into emp values(7900, 'james',  'clerk',     7698,to_date('3-dec-1981', 'dd-mon-yyyy'),   950, null, 30);
    insert into emp values(7902, 'ford',   'analyst',   7566,to_date('3-dec-1981', 'dd-mon-yyyy'),  3000, null, 20);
    insert into emp values(7934, 'miller', 'clerk',     7782,to_date('23-jan-1982', 'dd-mon-yyyy'), 1300, null, 10);
    */

    create index deptno_on_emp on emp(deptno);
    commit;

    set term ^;
    create or alter trigger dept_bi
    active before insert on dept as
    begin
      if ( new.deptno is null or new.deptno < 0) then
        new.deptno = gen_id(dept_table_seq, 1);
    end
    ^

    create or alter trigger emp_table_bi
    active before insert on emp as
    begin
      if (:new.empno is null or :new.empno < 0) then
        new.empno = gen_id(emp_Table_seq, 1);
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    -- set list on;

    -- https://community.oracle.com/thread/1020352
    select
        --empno, ename, job, mgr, hiredate, sal, comm, deptno,
        'sample1' as info
        ,empno
        ,ename
        ,sal
        ,last_value(sal)over(
            partition by deptno order by empno
            rows between unbounded preceding and 1 preceding
          ) last_sal
    from emp
    order by empno
    ;


    -- http://www.orafaq.com/node/55
    select
        'sample2-a' as info
        ,empno
        ,deptno
        ,hire_year
        ,count(*) over (
            partition by hire_year
            order by hiredate, empno rows between 3 preceding and 1 following) from_p3_to_f1
        ,count(*) over (
            partition by hire_year
            order by hiredate, empno rows between unbounded preceding and current row) from_pu_to_c
        ,count(*) over (
            partition by hire_year
            order by hiredate, empno rows between 3 preceding and 1 preceding) from_p2_to_p1
        ,count(*) over (
            partition by hire_year
            order by hiredate, empno rows between 1 following and 3 following) from_f1_to_f3
    from (select e.*, extract(year from e.hiredate) hire_year from emp e )
    order by empno
    ;


    -- For each employee give the count of employees getting half more that their
    -- salary and also the count of employees in the departments 20 and 30 getting half
    -- less than their salary.
    select
        'sample2-b' as info
        ,deptno
        ,empno
        ,sal
        ,count(*) over (
            partition by deptno order by sal range
            between unbounded preceding and (sal/2) preceding
        ) cnt_lt_half
        ,count(*) over (
            partition by deptno order by sal range
            between (sal/2) following and unbounded following
        ) cnt_mt_half
    from emp
    order by empno
    ;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    INFO      EMPNO ENAME               SAL     LAST_SAL
    ======= ======= ========== ============ ============
    sample1    7369 smith            800.00       <null>
    sample1    7499 allen           1600.00       <null>
    sample1    7521 ward            1250.00      1600.00
    sample1    7566 jones           2975.00       800.00
    sample1    7654 martin          1250.00      1250.00
    sample1    7698 blake           2850.00      1250.00
    sample1    7782 clark           2450.00       <null>
    sample1    7788 scott           3000.00      2975.00
    sample1    7839 king            5000.00      2450.00
    sample1    7844 turner          1500.00      2850.00
    sample1    7876 adams           1100.00      3000.00
    sample1    7900 james            950.00      1500.00
    sample1    7902 ford            3000.00      1100.00
    sample1    7934 miller          1300.00      5000.00

    INFO        EMPNO  DEPTNO HIRE_YEAR         FROM_P3_TO_F1          FROM_PU_TO_C         FROM_P2_TO_P1         FROM_F1_TO_F3
    ========= ======= ======= ========= ===================== ===================== ===================== =====================
    sample2-a    7369      20      1980                     1                     1                     0                     0
    sample2-a    7499      30      1981                     2                     1                     0                     3
    sample2-a    7521      30      1981                     3                     2                     1                     3
    sample2-a    7566      20      1981                     4                     3                     2                     3
    sample2-a    7654      30      1981                     5                     7                     3                     3
    sample2-a    7698      30      1981                     5                     4                     3                     3
    sample2-a    7782      10      1981                     5                     5                     3                     3
    sample2-a    7788      20      1982                     2                     2                     1                     0
    sample2-a    7839      10      1981                     5                     8                     3                     2
    sample2-a    7844      30      1981                     5                     6                     3                     3
    sample2-a    7876      20      1983                     1                     1                     0                     0
    sample2-a    7900      30      1981                     5                     9                     3                     1
    sample2-a    7902      20      1981                     4                    10                     3                     0
    sample2-a    7934      10      1982                     2                     1                     0                     1

    INFO       DEPTNO   EMPNO          SAL           CNT_LT_HALF           CNT_MT_HALF
    ========= ======= ======= ============ ===================== =====================
    sample2-b      20    7369       800.00                     0                     3
    sample2-b      30    7499      1600.00                     0                     1
    sample2-b      30    7521      1250.00                     0                     1
    sample2-b      20    7566      2975.00                     2                     0
    sample2-b      30    7654      1250.00                     0                     1
    sample2-b      30    7698      2850.00                     3                     0
    sample2-b      10    7782      2450.00                     0                     1
    sample2-b      20    7788      3000.00                     2                     0
    sample2-b      10    7839      5000.00                     2                     0
    sample2-b      30    7844      1500.00                     0                     1
    sample2-b      20    7876      1100.00                     0                     3
    sample2-b      30    7900       950.00                     0                     3
    sample2-b      20    7902      3000.00                     2                     0
    sample2-b      10    7934      1300.00                     0                     2
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

