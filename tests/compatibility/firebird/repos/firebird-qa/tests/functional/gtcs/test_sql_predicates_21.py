#coding:utf-8

"""
ID:          n/a
TITLE:       SINGULAR - Subquery returning exactly one row.
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/C_SQL_PRED_21.script
NOTES:
    [30.09.2025] pzotov
    Checked on 6.0.0.1282; 5.0.4.1715; 4.0.7.3235; 3.0.14.33826.
"""
import os
import pytest
from firebird.qa import *

db = db_factory()

substitutions = [ ('[ \t]+', ' '), ]
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    sql_init = (act.files_dir / 'gtcs-shtest-ddl-and-data.sql').read_text()
    sql_addi = """
        set list on;
        select 'point-00' as msg from rdb$database;
        set count on;

        select v2.deptno, v2.department, v2.mgrno
        from dept_budget v2
        where singular
        (
            select * from fullemp v3, empsal v4, sales t1
            where
                v2.department = v3.department
                and v3.empno = v4.empno
                and v4.empno = t1.sales_rep
                and t1.total_value > (select max(t2.total_value) from sales t2 where t2.sales_rep = 100)
        )
        order by v2.deptno;

        set count off;
        select 'point-01' as msg from rdb$database;
        set count on;

        select v2.deptno, v2.department, v2.mgrno
        from dept_budget v2
        where
            1 =
            (
                select count(*) from fullemp v3, empsal v4, sales t1
                where
                v2.department = v3.department
                and v3.empno = v4.empno
                and v4.empno = t1.sales_rep
                and t1.total_value > (select max(t2.total_value) from sales t2 where t2.sales_rep = 100)
            )
        order by v2.deptno;

        set count off;
        select 'point-02' as msg from rdb$database;
    """
    
    act.expected_stdout = """
        MSG point-00
        DEPTNO 115
        DEPARTMENT Federal Sales Region
        MGRNO 138
        Records affected: 1
        MSG point-01
        DEPTNO 115
        DEPARTMENT Federal Sales Region
        MGRNO 138
        Records affected: 1
        MSG point-02
    """
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ), combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
