#coding:utf-8

"""
ID:          n/a
TITLE:       GROUP BY of result of SINGULAR which is applied to result of JOIN
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/C_SQL_PRED_18.script
NOTES:
    [29.09.2025] pzotov
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

        select v1.deptno, v1.department, v1.mgrno, count(*)
        from head_dept v1
        where singular
        (
            select *
            from dept_budget v2, fullemp v3, empsal v4
            where
                v2.deptno= v1.deptno
                and v2.department = v3.department
                and v3.empno = v4.empno
        )
        group by v1.deptno, v1.department, v1.mgrno
        order by v1.deptno;

        set count off;
        select 'point-01' as msg from rdb$database;
        set count on;

        select v1.deptno, v1.department, v1.mgrno, count(*)
        from head_dept v1
        where 1 =
        (
            select count(*)
            from dept_budget v2, fullemp v3, empsal v4
            where
                v2.deptno= v1.deptno
                and v2.department = v3.department
                and v3.empno = v4.empno
        )
        group by v1.deptno, v1.department, v1.mgrno
        order by v1.deptno;

        set count off;
        select 'point-02' as msg from rdb$database;
    """
    
    act.expected_stdout = """
        MSG point-00
        DEPTNO 000
        DEPARTMENT Corporate Headquarters
        MGRNO 120
        COUNT 1
        Records affected: 1
        MSG point-01
        DEPTNO 000
        DEPARTMENT Corporate Headquarters
        MGRNO 120
        COUNT 1
        Records affected: 1
        MSG point-02
    """
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ), combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
